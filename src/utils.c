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

char* getSharedMemoryName(const char *filePath)
{
    char* name = malloc(256 * sizeof(char));
    int returnValue = -1;
    struct stat statBuffer;
    
    returnValue = stat(filePath, &statBuffer);
    if (returnValue < 0)
    {
        PROC_ERROR("stat() failure");
        return NULL;
    }
 
    sprintf(name, SHARED_MEM_FORMAT, statBuffer.st_dev, statBuffer.st_ino);
    return name;
}