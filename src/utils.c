#include "utils.h"

int initialiserMutex(pthread_mutex_t *pMutex)
{
    pthread_mutexattr_t mutexAttr;
    int code = pthread_mutexattr_init(&mutexAttr);
    if (code != 0)
    {
        return code;
    }
    code = pthread_mutexattr_setpshared(&mutexAttr, PTHREAD_PROCESS_SHARED);
    if (code != 0)
    {
        return code;
    }
    code = pthread_mutex_init(pMutex, &mutexAttr);
    return code;
}

int initialiserCond(pthread_cond_t *pCond)
{
    pthread_condattr_t condAttr;
    int code;
    code = pthread_condattr_init(&condAttr);

    if (code != 0)
    {
        return code;
    }
    
    code = pthread_condattr_setpshared(&condAttr, PTHREAD_PROCESS_SHARED);
    if (code != 0)
    {
        return code;
    }
    return pthread_cond_init(pCond, &condAttr);
}

bool makeSharedNameByPath(const char *filePath, char type, char *name, size_t maxLen)
{        
    int returnValue = -1;
    struct stat statBuffer;
    
    if ((!filePath) || (!name))
    {
        PROC_ERROR("wrong arguments");
        return NULL;
    }

    returnValue = stat(filePath, &statBuffer);
    if (returnValue < 0)
    {
        PROC_ERROR("stat() failure");
        return NULL;
    }

    if (0 > snprintf(name, maxLen, SHARED_MEM_FORMAT, type, statBuffer.st_dev, statBuffer.st_ino))
    {
        PROC_ERROR("name formatting error! not enough space?");
        return NULL;
    }
    return true;
}


bool makeSharedNameByFd(int fd, char type, char *name, size_t maxLen)
{
    int returnValue = -1;
    struct stat statBuffer;
    
    if (!name)
    {
        PROC_ERROR("wrong arguments");
        return false;
    }

    returnValue = fstat(fd, &statBuffer);
    if (returnValue < 0)
    {
        PROC_ERROR("stat() failure");
        return NULL;
    }
 
    if (0 > snprintf(name, maxLen, SHARED_MEM_FORMAT, type, statBuffer.st_dev, statBuffer.st_ino))
    {
        PROC_ERROR("name formatting error! not enough space?");
        return NULL;
    }
    return true;
}


int ownerEquals(owner o1, owner o2){
    return (o1.des == o2.des) && (o1.proc == o2.proc);
}
