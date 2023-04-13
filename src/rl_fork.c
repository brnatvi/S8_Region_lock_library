#include "../include/rl_fork.h"

pid_t rl_fork() {
    pid_t pid;
    switch(pid = fork()) {
        case -1 :
            PROC_ERROR("fork() failure");
            return -1;
        case 0 :
            pid_t parent = getppid();
            pid_t fils = getpid();
            for(int i = 0; i < rl_all_files.nb_files; i++){
                if(canAddNewOwner(parent, rl_all_files.tab_open_files[i]) == -1) {
                    // gestion erreur
                    PROC_ERROR("rl_fork() failure NB_OWNERS at max");
                    exit(EXIT_FAILURE);
                }
            }
            for(int i = 0; i < rl_all_files.nb_files; i++){
                addNewOwner(parent, fils, rl_all_files.tab_open_files[i]);
            }
            exit(EXIT_SUCCESS);
        default :
            if(wait(NULL) == -1) { // necessaire ?
                PROC_ERROR("wait() failure");
                return -1;
            }
            return pid;
    }

}

// -1 si impossible, 0 sinon
int canAddNewOwner(pid_t parent, rl_open_file *f){
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

// assume we can add
int addNewOwner(pid_t parent, pid_t fils, rl_open_file *f){
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

