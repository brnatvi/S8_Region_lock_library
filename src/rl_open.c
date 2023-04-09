#include "utils.h"

// Possible usage:
//     int open(const char *pathname, int flags);
//     int open(const char *pathname, int flags, mode_t mode);

rl_descriptor rl_open(const char *path, int oflag, ...)
{    
    va_list        parameters;
    mode_t         mode                   = -1;
    int            fdFile, fdSharedMemory = -1;
    char*          sharedMemName          = NULL;
    rl_open_file*  stRlOpenFile           = NULL;
    rl_descriptor* stRlDescriptor         = NULL;
    
    // ======================================== get arguments ==========================================================
    
    va_start(parameters, oflag);
    mode = (mode_t)va_arg(parameters, int);
    va_end(parameters);

    // ========================================= create rl_descriptor ==================================================

    stRlDescriptor = malloc(sizeof(rl_descriptor));
    if (NULL == stRlDescriptor)
    {
        PROC_ERROR("malloc() for rl_descriptor failure");
        exit(EXIT_FAILURE);
    }
    memset(stRlDescriptor, 0, sizeof(rl_descriptor));
    stRlDescriptor->d = -1;

    // =============================== reject open if size rl_all_files + 1 > NB_FILES =================================

    if (NB_FILES == rl_all_files.nb_files)
    {
        PROC_ERROR("Unable to proceed rl_open because the library's limit on the number of open files (NB_FILES) has been reached");
        goto lBadExit;
    }
    
    // =================== open or create shared memory object =========================================================

    // obtain shared memory name
    sharedMemName = getSharedMemoryName(path);
    if (NULL == sharedMemName)
    {
        PROC_ERROR("getSharedMemoryName() failure");
        goto lBadExit;
    }
    printf("\n name %s\n", sharedMemName);
    

    // open/create shared memory object
    fdSharedMemory = shm_open(sharedMemName, O_CREAT|O_RDWR|O_EXCL, S_IRUSR|S_IRGRP|S_IROTH|S_IWUSR|S_IWGRP|S_IWOTH);
    if (-1 == fdSharedMemory)
    {
        printf(" fdSharedMemory %d\n", fdSharedMemory);
        
        if (EEXIST == errno)        // shared memory already exist
        {            
            // try to open it again to read/write
            fdSharedMemory = shm_open(sharedMemName, O_RDWR, S_IRUSR|S_IRGRP|S_IROTH|S_IWUSR|S_IWGRP|S_IWOTH);
            if (-1 == fdSharedMemory)
            {
                PROC_ERROR("shm_open() with only O_RDWR failure");
                goto lBadExit;  
            }

            // map object to memory
            stRlOpenFile = mmap(0, sizeof(rl_open_file), PROT_READ|PROT_WRITE, MAP_SHARED, fdSharedMemory, 0);
            if (MAP_FAILED == (void *)stRlOpenFile)
            {
                PROC_ERROR("mmap() failure");
                goto lBadExit;
            }
        }
        else    // another errors
        {        
            PROC_ERROR("shm_open() with O_CREAT|O_EXCL failure");
            goto lBadExit;
        }
    }
    else    // shared memory does not exist yet
    {
        // =================================== project to memory, trancate =============================================
        // allocate necessary size of memory
        if (ftruncate(fdSharedMemory, sizeof(rl_open_file)))
        {
            PROC_ERROR("ftruncate() failure");
            goto lBadExit;
        }

        // map object to memory
        stRlOpenFile = mmap(0, sizeof(rl_open_file), PROT_READ|PROT_WRITE, MAP_SHARED, fdSharedMemory, 0);
        if (MAP_FAILED == (void *)stRlOpenFile)
        {
            PROC_ERROR("mmap() failure");
            goto lBadExit;
        }
        memset(stRlOpenFile, 0, sizeof(rl_open_file));

        // TODO really need?
        memset(stRlOpenFile->lock_table, 0, NB_LOCKS * sizeof(rl_lock));  /* sinon, rl_open crée un shared memory object 
                                                                            de taille sizeof(rl_open_file), le projette
                                                            en mémoire, et initialise les éléments de tableau lock_table */
    } 
    
    // ============================================= open file =========================================================
    
    fdFile = open(path, oflag, mode);
    if (fdFile < 0)
    {
        PROC_ERROR("open() file failure");
        goto lBadExit;
    }

    // ========================================= put new fd into rl_descriptor =========================================

    stRlDescriptor->d = fdFile;
    stRlDescriptor->f = stRlOpenFile;

    // ============================== register new rl_open_file in rl_all_files ========================================
 
    int index = rl_all_files.nb_files;
    rl_all_files.tab_open_files[index] = stRlOpenFile;
    rl_all_files.nb_files++;
    
    // ========================================= make EXIT_SUCCESS =====================================================

    return *stRlDescriptor;
    


lBadExit:  
    FREE_MMAP(stRlOpenFile, sizeof(rl_open_file));
    CLOSE_FILE(fdSharedMemory);
    CLOSE_FILE(fdFile);  
    FREE_MEM(sharedMemName); 
    return *stRlDescriptor;
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

    printf("fd1 = %d, fd2 = %d\n", rl_all_files.tab_open_files[0]->first, rl_all_files.tab_open_files[1]->first);


    return EXIT_SUCCESS;
}