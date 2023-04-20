#include "main_header.h"

#define SHARED_PREFIX_MEM 'f'
#define SHARED_PREFIX_SEM 's'


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
 * Test equality between two owners
 * @param o1 owner
 * @param o2 owener
 * @return 1 if equals 0 otherwisz
 */
int ownerEquals(owner o1, owner o2);

/**
 * Compose shared memory object name according format "/f_dev_ino", basing on file's stat information 
 * @param filePath [in] file path
 * @param type [in] shared object type: SHARED_PREFIX_MEM or SHARED_PREFIX_SEM
 * @param name [out] generated name
 * @param maxLen [in] maximum name length in characters
 * @return true - OK, false - error
 */
bool makeSharedNameByPath(const char *filePath, char type, char *name, size_t maxLen);

/**
 * Compose shared memory object name according format "/f_dev_ino", basing on file's stat information 
 * @param fd [in] file descriptor
 * @param type [in] shared object type: SHARED_PREFIX_MEM or SHARED_PREFIX_SEM
 * @param name [out] generated name
 * @param maxLen [in] maximum name length in characters
 * @return true - OK, false - error
 */
bool makeSharedNameByFd(int fd, char type, char *name, size_t maxLen);
