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
#define NB_FILES            256
#define NB_FD               512
#define NEXT_NULL           -2
#define FILE_UNK            -1

#define SHARED_NAME_MAX_LEN 64
#define SHARED_MEM_FORMAT   "/%c_%ld_%ld"

/* ==================================== MACRO FUNCTIONS ============================================================= */

#define CLOSE_FILE(File) if (File > 0) { close(File); File = -1; }
#define KILL_SEMATHORE(Sem) if (Sem)  { sem_close(Sem); sem_destroy(Sem); }
#define UNLINK_SEMATHORE(Name) if (Name) { sem_unlink(Name); }
#define FREE_MMAP(Mem, len) if (Mem) { munmap(Mem, len); }
#define FREE_MEM(Mem) if (Mem) { free(Mem); Mem = NULL; }

#define PROC_ERROR(Message) { fprintf(stderr, "%s : error {%s} in file {%s} on line {%d}\n", Message, strerror(errno), __FILE__, __LINE__); }

#define LOCK_ERROR(Code) if (Code != 0) { fprintf(stderr, "%s : error {%d} in file {%s} on line {%d}\n", "mutex_lock() failure", strerror(Code), __FILE__, __LINE__); }
#define UNLOCK_ERROR(Code) if (Code != 0) { fprintf(stderr, "%s : error {%d} in file {%s} on line {%d}\n", "mutex_unlock() failure", strerror(Code), __FILE__, __LINE__); }


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
    short           type; 
    size_t          nb_owners;
    owner           lock_owners[NB_OWNERS];
} rl_lock;

typedef struct
{
    int             first;
    rl_lock         lock_table[NB_LOCKS];
    pthread_mutex_t mutex;
    int             refCnt;
} rl_open_file;


typedef struct
{
    size_t        dCnt;
    int           d[NB_FD];
    rl_open_file *f;
} rl_file;

typedef struct
{
    int             d;
    rl_open_file    *f;
} rl_descriptor;

static struct 
{
    int             nb_files;
    rl_file         tab_open_files[NB_FILES];
    pthread_mutex_t mutex; //protect multi-threading access for library
} rl_all_files;
