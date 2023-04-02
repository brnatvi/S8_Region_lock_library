#include "utils.h"

// Possible usage:
//     int open(const char *pathname, int flags);
//     int open(const char *pathname, int flags, mode_t mode);


rl_descriptor rl_open(const char *path, int oflag, ...)
{   
    int         returnValue         = -1;
    int         fdFile, fdSharedMem = -1;
    va_list     parameters;
    mode_t      mode                = -1;
    char*       sharedMemName       = "";
    struct stat bufStat;
    rl_lock*    structLockMem;
    

    // ==================== get arguments ==============================================================================
    va_start(parameters, oflag);
    mode = (mode_t)va_arg(parameters, int);
    va_end(parameters);    

    // =================== open or create shared memory object =========================================================

    // obtain shared memory name
    strcpy(sharedMemName, (const char* restrict)getSharedMemoryName(path));
    if (NULL == sharedMemName)
    {
        PROC_ERROR("getSharedMemoryName() failure");
        goto lBadExit;
    }
    
    // open shared memory object
    fdSharedMem = shm_open(sharedMemName
                                        , O_CREAT | O_RDWR
                                        , S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH);
    
    fstat(fdSharedMem, &bufStat);
    if (0 == (int)bufStat.st_size)      // if object not exist yet
    {
        // create, trancate   
        if( ftruncate( -1 == fdSharedMem, sizeof(rl_lock)) )
        {
            PROC_ERROR("ftruncate() failure");
            goto lBadExit;
        }

        // map object to memory
        structLockMem = mmap(0, sizeof(rl_lock), PROT_READ | PROT_WRITE, MAP_SHARED, fdSharedMem, 0);
        if (MAP_FAILED == (void*)structLockMem)
        {        
            PROC_ERROR("mmap() failure");
            goto lBadExit;
        }

        memset(structLockMem, 0, sizeof(rl_lock));
    }
    else
    {

    }


    
    // =================== create or open file descriptor ==============================================================

    fdFile = open(path, oflag, mode);
    if (fdFile < 0)
    {        
        PROC_ERROR("open() failure");
        goto lBadExit;
    }


    
lBadExit:
    CLOSE_FILE(fdFile);  
    FREE_MMAP(structLockMem, sizeof(rl_lock));
    CLOSE_FILE(fdSharedMem);


lGoodExit:
    CLOSE_FILE(fdFile); 
    FREE_MMAP(structLockMem, sizeof(rl_lock));
    CLOSE_FILE(fdSharedMem);
}

int main(int argc, const char *argv[])
{
    return 0;
}