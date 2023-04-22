#include "utils.h"

// Possible usage:
//     int open(const char *pathname, int flags);
//     int open(const char *pathname, int flags, mode_t mode);

rl_descriptor rl_open(const char *path, int oflag, ...)
{    
    va_list        parameters;
    mode_t         mode                   = -1;
    int            fdFile, fdSharedMemory = -1;
    char           pSharedMemName[SHARED_NAME_MAX_LEN];
    char           pSharedSemName[SHARED_NAME_MAX_LEN];
    sem_t         *sharedSem              = NULL;
    rl_open_file*  pRlOpenFile            = NULL;
    rl_descriptor  stRlDescriptor         = {.d = -1, .f = NULL};
    bool           isNewFile              = true;
    bool           isError                = false;
    
    // ======================================== get arguments ==========================================================
    
    if (oflag & O_CREAT)
    {
        va_start(parameters, oflag);
        mode = (mode_t)va_arg(parameters, int);
        va_end(parameters);
    }

    pthread_mutex_lock(&rl_all_files.mutex);

    if (NB_FILES == rl_all_files.nb_files)
    {
        PROC_ERROR("Unable to proceed rl_open because the library's limit on the number of open files (NB_FILES) has been reached");
        isError = true;
        goto lExit;
    }

    //first open the file, if we can't - everything else is useless 
    fdFile = open(path, oflag, mode);
    if (fdFile < 0)
    {
        PROC_ERROR("open() file failure");
        isError = true;
        goto lExit;
    }

   
    // =================== open or create shared memory object =========================================================
    if (    (!makeSharedNameByPath(path, SHARED_PREFIX_MEM, pSharedMemName, SHARED_NAME_MAX_LEN))
         || (!makeSharedNameByPath(path, SHARED_PREFIX_SEM, pSharedSemName, SHARED_NAME_MAX_LEN))
       )
    {
        PROC_ERROR("making shared names failure!");
        isError = true;
        goto lExit;
    }

    printf("{%s} shared name %s\n", path, pSharedMemName);

    // use semaphore to protect process of creation shared memory
    sharedSem = sem_open(pSharedSemName, O_CREAT | O_EXCL, 0666, 0);
    if (NULL == sharedSem)
    {
        sharedSem = sem_open(pSharedSemName, 0);
        if (NULL == sharedSem)
        {
            PROC_ERROR("sem_open() failed");
            isError = true;
            goto lExit;  
        }
        if (0 > sem_wait(sharedSem))
        {
            PROC_ERROR("sem_wait() error");
            isError = true;
            goto lExit;  
        }
    }

    fdSharedMemory = shm_open(pSharedMemName, 
                              O_CREAT | O_RDWR | O_EXCL,  
                              S_IRUSR|S_IRGRP|S_IROTH|S_IWUSR|S_IWGRP|S_IWOTH);

    if (0 <= fdSharedMemory)
    {
        printf("new shared file!\n");
    }
    else
    {
        printf("existing shared file!\n");
        isNewFile = false;

        fdSharedMemory = shm_open(pSharedMemName, 
                                  O_RDWR, 
                                  S_IRUSR|S_IRGRP|S_IROTH|S_IWUSR|S_IWGRP|S_IWOTH);

        if (0 > fdSharedMemory)
        {
            PROC_ERROR("shm_open() failure");
            isError = true;
            goto lExit;  
        }
    }

    if ((isNewFile) && (0 > ftruncate(fdSharedMemory, sizeof(rl_open_file))))
    {
        PROC_ERROR("ftruncate() failure");
        isError = true;
        goto lExit;  
    }

    // map object to memory
    pRlOpenFile = mmap(0, sizeof(rl_open_file), PROT_READ|PROT_WRITE, MAP_SHARED, fdSharedMemory, 0);
    if (MAP_FAILED == (void *)pRlOpenFile)
    {
        PROC_ERROR("mmap() failure");
        isError = true;
        goto lExit;  
    }

    if (isNewFile) 
    {
        memset(pRlOpenFile, 0, sizeof(rl_open_file));
        initialiserMutex(&pRlOpenFile->mutex);
        pRlOpenFile->first = NEXT_NULL;
        for (int i =0; i < NB_LOCKS; i++)
        {
            pRlOpenFile->lock_table[i].next_lock = NEXT_NULL;
        }
    }

    // ============================== register new rl_open_file in rl_all_files ========================================
    rl_all_files.tab_open_files[rl_all_files.nb_files].f    = pRlOpenFile;
    rl_all_files.tab_open_files[rl_all_files.nb_files].dCnt = 0;
    rl_all_files.nb_files++;

    pRlOpenFile->refCnt++;
    printf("RC : %d\n", pRlOpenFile->refCnt);

lExit:

    pthread_mutex_unlock(&rl_all_files.mutex);

    if (isError)
    {
        CLOSE_FILE(fdFile);
        fdFile = FILE_UNK;

        FREE_MMAP(pRlOpenFile, sizeof(rl_open_file));

        if (isNewFile)
        {
            shm_unlink(pSharedMemName);
        }
    }

    CLOSE_FILE(fdSharedMemory);

    if (sharedSem)
    {
        sem_post(sharedSem);
        sem_close(sharedSem);
        if ((isError) && (isNewFile))
        {
            sem_unlink(pSharedSemName);
        }
    }

    stRlDescriptor.d = fdFile;
    stRlDescriptor.f = pRlOpenFile;

    
    return stRlDescriptor;
}


int rl_close(rl_descriptor lfd)
{
    char    pSharedMemName[SHARED_NAME_MAX_LEN];
    char    pSharedSemName[SHARED_NAME_MAX_LEN];
    sem_t  *sharedSem      = NULL;
    bool    isError        = false;
    bool    isLastRef      = false;
    pid_t   pidCur         = getpid();
    int     lockIdx        = NEXT_NULL;
    bool    isCloseFd      = true;


    if ((lfd.d == FILE_UNK) || (!lfd.f))
    {
        PROC_ERROR("wrong input");
        return RES_ERR;
    }

    if (    (!makeSharedNameByFd(lfd.d, SHARED_PREFIX_MEM, pSharedMemName, SHARED_NAME_MAX_LEN))
         || (!makeSharedNameByFd(lfd.d, SHARED_PREFIX_SEM, pSharedSemName, SHARED_NAME_MAX_LEN))
       )
    {
        PROC_ERROR("making shared names failure!");
        isError = true;
        goto lExit;
    }

    sharedSem = sem_open(pSharedSemName, 0);
    if (NULL == sharedSem)
    {
        PROC_ERROR("sem_open() failed");
        isError = true;
        goto lExit;  
    }
    if (0 > sem_wait(sharedSem))
    {
        PROC_ERROR("sem_wait() error");
        isError = true;
        goto lExit;  
    }

    pthread_mutex_lock(&lfd.f->mutex);

    printf("looking through locks...\n");
    lockIdx = lfd.f->first;
    while (lockIdx >= 0)
    {
        for (int i = 0; i < lfd.f->lock_table[lockIdx].nb_owners; i++)
        {
            if (    (lfd.f->lock_table[lockIdx].lock_owners[i].proc == pidCur)   //if this proc has lock.s for this fd
                 && (lfd.f->lock_table[lockIdx].lock_owners[i].des  == lfd.d)
               )
            {
                if ((i+1) < lfd.f->lock_table[lockIdx].nb_owners)
                {
                    memmove(&lfd.f->lock_table[lockIdx].lock_owners[i],    // erase him from owners and shift left the rest
                            &lfd.f->lock_table[lockIdx].lock_owners[i+1],
                            sizeof(owner) * (lfd.f->lock_table[lockIdx].nb_owners - (i+1)) );
                    i--;
                }
                lfd.f->lock_table[lockIdx].nb_owners--;    
            }
        }

        if (!lfd.f->lock_table[lockIdx].nb_owners) //if there is no more owners for lock -> remove lock from lock_table
        {
            struct flock fLock;    //remove existed lock
            fLock.l_len    = lfd.f->lock_table[lockIdx].len;
            fLock.l_type   = F_UNLCK;
            fLock.l_start  = lfd.f->lock_table[lockIdx].starting_offset;
            fLock.l_whence = SEEK_SET;
            fLock.l_pid    = getpid();
            fcntl(lfd.d, F_SETLK, &fLock); 
            
            int nextLock = lfd.f->lock_table[lockIdx].next_lock;
            lfd.f->lock_table[lockIdx].next_lock = NEXT_NULL;
            
            if (lfd.f->first == lockIdx)
            {
                lfd.f->first = nextLock;
            }
            lockIdx = nextLock;
        }
    }

    //if this process has another fd opened for file we can't close file
    lockIdx = lfd.f->first;
    while ((lockIdx >= 0) && (isCloseFd == true))
    {
        for (int i = 0; i < lfd.f->lock_table[lockIdx].nb_owners; i++)
        {
            if (lfd.f->lock_table[lockIdx].lock_owners[i].proc == pidCur)
            {
                isCloseFd = false;
                break;
            }
        }
    }
   
    if (!isCloseFd)
    {
        printf("postpone descriptor closing\n");

        pthread_mutex_lock(&rl_all_files.mutex);
        rl_file *pRlFile = NULL;

        for (int i = 0; i < rl_all_files.nb_files; i++)
        {
            pRlFile = &rl_all_files.tab_open_files[i];

            if (pRlFile->f == lfd.f)
            {
                pRlFile->d[pRlFile->dCnt] = lfd.d;
                pRlFile->dCnt++;
                break;
            }
        }
        pthread_mutex_unlock(&rl_all_files.mutex);
    }

    printf("<< RC : %d!\n", lfd.f->refCnt);

    lfd.f->refCnt --;
    if (lfd.f->refCnt <= 0)
    {
        isLastRef = true;    
    }

    pthread_mutex_unlock(&lfd.f->mutex);

lExit:
    if (isCloseFd)
    {
        CLOSE_FILE(lfd.d);
    }

    if (sharedSem)
    {
        sem_post(sharedSem);
        sem_close(sharedSem);
        if (isLastRef)
        {
            sem_unlink(pSharedSemName);
        }
    }

    if (isError) return -1;
    return 0;
}






int main(int argc, const char *argv[])
{
    if (argc != 2)
    {
        PROC_ERROR("wrong input");
        return EXIT_FAILURE;
    }
    printf("file to open : %s\n", argv[1]);

    rl_descriptor rl_fd1 = rl_open(argv[1], O_RDWR, S_IRUSR|S_IRGRP|S_IROTH|S_IWUSR|S_IWGRP|S_IWOTH);
    rl_descriptor rl_fd2 = rl_open(argv[1], O_RDWR, S_IRUSR|S_IRGRP|S_IROTH|S_IWUSR|S_IWGRP|S_IWOTH);

    printf("fd1 = %d, fd2 = %d\n", rl_fd1.d, rl_fd2.d);

    rl_close(rl_fd2);
    rl_close(rl_fd1);

    return EXIT_SUCCESS;
}