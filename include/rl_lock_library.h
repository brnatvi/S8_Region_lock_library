#pragma once

#include <stdlib.h>
#include <stdio.h>          /* for srand, rand */
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/mman.h>       /* for shm_open */
#include <sys/stat.h>       /* for mode constants */
#include <fcntl.h>          /* for O_* constants and fcntl */
#include <sys/types.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdarg.h>         /* for functions take a variable number of arguments */

/* ==================================== MACRO VARIABLES ============================================================= */

#define NB_OWNERS           20
#define NB_LOCKS            10

/* ======================================= STRUCTURES =============================================================== */

typedef struct
{
    pid_t   proc;     /* pid of process */
    int     des;      /* file descripor */
} owner;

typedef struct
{
    int             next_lock;
    off_t           starting_offset;
    off_t           len;
    short           type;   //F_RDLCK or F_WRLCK
    size_t          nb_owners;
    owner           lock_owners[NB_OWNERS];
} rl_lock;

typedef struct
{
    int             first;
    rl_lock         lock_table[NB_LOCKS];
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    int             blockCnt;
    int             refCnt;
} rl_open_file;


typedef struct
{
    int             d;
    rl_open_file    *f;
} rl_descriptor;

///////////////////////////////////         RL_LIBRARY FUNCTIONS       /////////////////////////////////////////////////
//////////                                                                                                      ////////

/**
 * Initialization of library's static structure.
 * @param None
 * @return 0
 */ 
int rl_init_library();

/**
 * TODO a good deployed description.
 * Possible usage:
 *     int open(const char *pathname, int flags);
 *     int open(const char *pathname, int flags, mode_t mode);
 * @param 
 * @return rl_descriptor with 'd' field equal of file descriptor opened, −1 otherwise
 */
rl_descriptor rl_open(const char *path, int oflag, ...);

/**
 * TODO a good deployed description.
 * @param 
 * @return 0 - success, −1 otherwise
 */
int rl_close(rl_descriptor lfd);


//TODO: doc
rl_descriptor rl_dup(rl_descriptor lfd);

//TODO: doc
rl_descriptor rl_dup2(rl_descriptor lfd, int newd);

//TODO: doc
pid_t rl_fork();

//TODO: doc
int rl_fcntl(rl_descriptor lfd, int cmd, struct flock *lck);

/**
 * print internal structures
 * @param lfd file descriptor
 */
void rl_print(rl_descriptor lfd);
