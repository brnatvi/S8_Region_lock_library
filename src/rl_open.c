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
    printf("RC:%d\n", pRlOpenFile->refCnt);

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


    return EXIT_SUCCESS;
}