#include "main_header.h"

/**
 * Initialize a mutex.
 * @param pMutex pointer to mutex
 * @return 0 if succesful, otherwise, an error number shall be returned to indicate the error
 */
int initialiserMutex(pthread_mutex_t *pMutex);

/**
 * Initialize a condition variable attributes object.
 * @param pCond pointer to condition
 * @return 0 if succesful, otherwise, an error number shall be returned to indicate the error
 */
int initialiserCond(pthread_cond_t *pCond);

/**
 * Compose shared memory object name according format "/f_dev_ino", basing on file's stat information 
 * @param filePath file path
 * @return char* of shared memory object name if succesful, NULL otherwise
 */
char* getSharedMemoryName(const char *filePath);

/**
 * Test equality between two owners
 * @param o1 owner
 * @param o2 owener
 * @return 1 if equals 0 otherwisz
 */
int ownerEquals(owner o1, owner o2);