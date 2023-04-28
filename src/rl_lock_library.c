#include "rl_lock_library.h"

#define NB_FILES            256
#define NB_FD               512
#define NEXT_NULL           -2
#define NEXT_LAST           -1
#define FILE_UNK            -1
#define RES_ERR             -1

#define SHARED_NAME_MAX_LEN 64
#define SHARED_MEM_FORMAT   "/%c_%ld_%ld"
#define SHARED_PREFIX_MEM 'f'
#define SHARED_PREFIX_SEM 's'


/* ==================================== MACRO FUNCTIONS ============================================================= */

#define CLOSE_FILE(File) if (File > 0) { close(File); File = -1; }
#define KILL_SEMATHORE(Sem) if (Sem)  { sem_close(Sem); sem_destroy(Sem); }
#define UNLINK_SEMATHORE(Name) if (Name) { sem_unlink(Name); }
#define FREE_MMAP(Mem, len) if (Mem) { munmap(Mem, len); }
#define FREE_MEM(Mem) if (Mem) { free(Mem); Mem = NULL; }

#define PROC_ERROR(Message) { fprintf(stderr, "%s : error {%s} in file {%s} on line {%d}\n", Message, strerror(errno), __FILE__, __LINE__); }

#define LOCK_ERROR(Code) if (Code != 0) { fprintf(stderr, "%s : error {%d} in file {%s} on line {%d}\n", "mutex_lock() failure", strerror(Code), __FILE__, __LINE__); }
#define UNLOCK_ERROR(Code) if (Code != 0) { fprintf(stderr, "%s : error {%d} in file {%s} on line {%d}\n", "mutex_unlock() failure", strerror(Code), __FILE__, __LINE__); }


typedef struct
{
    size_t        dCnt;
    int           d[NB_FD];
    rl_open_file *f;
} rl_file;

static struct 
{
    int             nb_files;
    rl_file         tab_open_files[NB_FILES];
    pthread_mutex_t mutex; //protect multi-threading access for library
} rl_all_files;


/* ================================  AUXILIARY FUNCTIONS DEFINITIONS  =============================================== */

//TODO: doc
// int add_new_owner(rl_descriptor lfd, int newd);
// 1 si oui, 0 si own non dans table, -1 si capacité max
int canAddNewOwner(owner own, rl_open_file *f);

//TODO: doc
// assume we can add
int addNewOwner(owner own, owner new_owner, rl_open_file *f);

//TODO: doc
//int add_new_owner();
// -1 si impossible, 0 sinon
int canAddNewOwnerByPid(pid_t parent, rl_open_file *f);

//TODO: doc
// assume we can add
int addNewOwnerByPid(pid_t parent, pid_t fils, rl_open_file *f);

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
 * @return true if equals, false otherwise
 */
bool ownerEquals(owner o1, owner o2);

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


///////////////////////////////////         RL_LIBRARY FUNCTIONS       /////////////////////////////////////////////////
//////////                                                                                                      ////////

int rl_init_library() {

    int code = -1;    
    if ((code = initialiserMutex(&rl_all_files.mutex)) != 0)
    {
        PROC_ERROR(strerror(code));
        return code;
    }
    rl_all_files.nb_files = 0;
    memset(rl_all_files.tab_open_files, 0, sizeof(rl_open_file) * NB_FILES);   

    return code;
}


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
        rl_file *pRlFile = NULL;                                    //AZH I've change code here

        for (int i = 0; i < rl_all_files.nb_files; i++)
        {
            pRlFile = &rl_all_files.tab_open_files[i];              //AZH I've change code here

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

if (isLastRef)
    {
        printf("last ref!\n");

        if (lfd.f->first >= 0)
        {
            PROC_ERROR("Last reference deleted, but file locks aren't deleted!");
        }

        pthread_mutex_lock(&rl_all_files.mutex);
        for (int i = 0; i < rl_all_files.nb_files; i++)
        {
            if (rl_all_files.tab_open_files[i].f == lfd.f)
            {
                for (int j = 0; j < rl_all_files.tab_open_files[i].dCnt; j++)
                {
                    CLOSE_FILE(rl_all_files.tab_open_files[i].d[j]);
                }

                if ((i+1) < rl_all_files.nb_files)
                {
                    memmove(&rl_all_files.tab_open_files[i],
                            &rl_all_files.tab_open_files[i+1],
                            sizeof(rl_file) * (rl_all_files.nb_files - (i+1)) );
                }
                rl_all_files.nb_files--;    
                break;
            }
        }
        pthread_mutex_unlock(&rl_all_files.mutex);


        pthread_mutex_destroy(&lfd.f->mutex);
        FREE_MMAP(lfd.f, sizeof(rl_open_file));
        shm_unlink(pSharedMemName);
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

    return isError ? -1 : 0;
}

//==============================================================================================================


rl_descriptor rl_dup(rl_descriptor lfd){
    int newd = dup(lfd.d);

    // Gestion erreur
    if(newd == -1) {
        PROC_ERROR("dup() failure"); // no close of newd (ref man dup)
        rl_descriptor bad = {.d = -1, .f = NULL};
        return bad;
    }
    owner own = {.des = lfd.d, .proc = getpid()};

    // lock
    pthread_mutex_lock(&lfd.f->mutex);

    int add = canAddNewOwner(own, lfd.f);
    if(add == -1) {
        // lever lock
        pthread_mutex_unlock(&lfd.f->mutex);
        PROC_ERROR("rl_dup() failure NB_OWNERS at max");
        rl_descriptor bad = {.d = -1, .f = NULL};
        return bad;
    }
    owner new_owner = {.des = newd, .proc = getpid()};
    if(add == 1) {
        addNewOwner(own, new_owner, lfd.f);
    }
    // unlock
    pthread_mutex_unlock(&lfd.f->mutex);
    rl_descriptor new_descr = {.d = newd, .f = lfd.f};
    return new_descr;
}


rl_descriptor rl_dup2(rl_descriptor lfd, int newd) {
    if(dup2(lfd.d, newd) == -1) {
        PROC_ERROR("dup2() failure"); // no close of newd
        rl_descriptor bad = {.d = -1, .f = NULL};
        return bad;
    }
    owner own = {.des = lfd.d, .proc = getpid()};
    pthread_mutex_lock(&lfd.f->mutex);
    int add = canAddNewOwner(own, lfd.f);
    if(add == -1) {
        pthread_mutex_unlock(&lfd.f->mutex);
        PROC_ERROR("rl_dup() failure NB_OWNERS at max");
        rl_descriptor bad = {.d = -1, .f = NULL};
        return bad;
    }
    owner new_owner = {.des = newd, .proc = getpid()};
    if(add == 1) {
        addNewOwner(own, new_owner, lfd.f);
    }
    pthread_mutex_unlock(&lfd.f->mutex);
    rl_descriptor new_descr = {.d = newd, .f = lfd.f};
    return new_descr;
    
}


pid_t rl_fork() {
    pid_t pid;
    switch(pid = fork()) {
        case -1 :
            PROC_ERROR("fork() failure");
            return -1;
        case 0 :
            // semaphore
            pthread_mutex_lock(&rl_all_files.mutex);
            for(int i = 0; i < rl_all_files.nb_files; i++){
                // lock
                pthread_mutex_lock(&rl_all_files.tab_open_files[i].f->mutex);
                // gestion erreur
                if(canAddNewOwnerByPid(getppid(), rl_all_files.tab_open_files[i].f) == -1) {
                    // lever lock + semaphore
                    pthread_mutex_unlock(&rl_all_files.tab_open_files[i].f->mutex);
                    pthread_mutex_unlock(&rl_all_files.mutex);
                    PROC_ERROR("rl_fork() failure NB_OWNERS at max");
                    return -1;
                }
                // unlock
                pthread_mutex_unlock(&rl_all_files.tab_open_files[i].f);
            }
            for(int i = 0; i < rl_all_files.nb_files; i++){
                // lock
                pthread_mutex_lock(&rl_all_files.tab_open_files[i].f->mutex);
                addNewOwnerByPid(getppid(), getpid(), rl_all_files.tab_open_files[i].f);
                // unlock
                pthread_mutex_unlock(&rl_all_files.tab_open_files[i].f->mutex);
            }
            pthread_mutex_unlock(&rl_all_files.mutex);
            exit(EXIT_SUCCESS);
        default :
            if(wait(NULL) == -1) { // necessaire ?
                PROC_ERROR("wait() failure");
                return -1;
            }
            return pid;
    }

}

int rl_fcntl(rl_descriptor lfd, int cmd, struct flock *lck) {
    owner lfd_owner = {.proc = getpid(), .des = lfd};
    if(cmd == F_SETLK) {
        if(lck->l_type == F_UNLCK) {
            // lever verrou au milieu d'un segment => deux segments verrouillés
            /* TODO : parcourir lfd.f
            * verifier sur quels endroits sont posés les verrous
            * si séparation en deux segments verrouillés -> canAddNewOwner + addNewOwner(owner,owner,...)
            * si owner non dans table => ERREUR
            */
            
        }
        else if(lck->l_type == F_RDLCK){

        }
        else if(lck->l_type == F_WRLCK) {

        }
    }
}

////////////////////////////////////         AUXILIARY FONCTIONS       /////////////////////////////////////////////////
//////////                                                                                                      ////////

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


bool ownerEquals(owner o1, owner o2){
    return ((o1.des == o2.des) && (o1.proc == o2.proc));
}


int canAddNewOwner(owner own, rl_open_file *f){
    int ind = f->first;
    int present = 0; // boolean
    while(ind != -1) {
        for(size_t i = 0; i < f->lock_table[ind].nb_owners; i++){
            if(ownerEquals(f->lock_table[ind].lock_owners[i], own)
            && f->lock_table[ind].nb_owners == NB_OWNERS) { 
                return -1;
            }
            if(ownerEquals(f->lock_table[ind].lock_owners[i], own)) {
                present = 1;
            }
        }
        ind = f->lock_table[ind].next_lock;
    }
    return present;
}


int addNewOwner(owner own, owner new_owner, rl_open_file *f){
    int ind = f->first;
    while(ind != -1) {
        int tmp = (int) f->lock_table[ind].nb_owners;
        for(size_t i = 0; i < f->lock_table[ind].nb_owners; i++){
            if(ownerEquals(f->lock_table[ind].lock_owners[i], own)){
                f->lock_table[ind].lock_owners[tmp] = new_owner;
                f->lock_table[ind].nb_owners ++;
            }
        }
        ind = f->lock_table[ind].next_lock;
    }
    return 0;
}


int canAddNewOwnerByPid(pid_t parent, rl_open_file *f){
    int ind = f->first;
    
    // loop through lock_table
    while(ind != -1){ 
        // loop through lock_owners
        for(size_t i = 0; i < f->lock_table[ind].nb_owners; i++){
            if(f->lock_table[ind].lock_owners[i].proc == parent 
            && f->lock_table[ind].nb_owners == NB_OWNERS) {
                return -1;
            }
            
        }
        ind = f->lock_table[ind].next_lock;
    }
    return 0;
}


int addNewOwnerByPid(pid_t parent, pid_t fils, rl_open_file *f){
    int ind = f->first;
    while(ind != -1){
        int tmp = (int) f->lock_table[ind].nb_owners;
        for(size_t i = 0; i < f->lock_table[ind].nb_owners; i++){
            if(f->lock_table[ind].lock_owners[i].proc == parent){
                owner new_owner = {.des = f->lock_table[ind].lock_owners[i].des,
                .proc = fils};
                f->lock_table[ind].lock_owners[tmp] = new_owner;
                f->lock_table[ind].nb_owners ++;
            }
        }
        ind = f->lock_table[ind].next_lock;
    }
    return 0;
}
