#include "../include/rl_dup.h"

rl_descriptor rl_dup(rl_descriptor lfd){
    int newd = dup(lfd.d);

    // Gestion erreur
    if(newd == -1) {
        PROC_ERROR("dup() failure");
        // no close of newd (ref man dup)
        rl_descriptor bad = {.d = -1, .f = NULL};
        return bad;
    }
    return dup_function(lfd, newd);
}

rl_descriptor rl_dup2(rl_descriptor lfd, int newd) {
    if(dup2(lfd.d, newd) == -1) {
        PROC_ERROR("dup2() failure"); // no close of newd
        rl_descriptor bad = {.d = -1, .f = NULL};
        return bad;
    }
    return dup_function(lfd, newd);
}

rl_descriptor dup_function(rl_descriptor lfd, int newd) {
    owner lfd_owner = {.proc = getpid(), .des = lfd.d};
    owner new_owner = {.proc = getpid(), .des = newd};

    if(add_new_owner(lfd_owner, new_owner) == -1){
        PROC_ERROR("rl_dup2() failure : NB_LOCKS at max");
        rl_descriptor bad = {.d = -1, .f = NULL};
        CLOSE_FILE(newd);
        return bad;
    }

    rl_descriptor new_rl_descriptor = {.d = newd, .f = lfd.f};
    return new_rl_descriptor;
}

int add_new_owner(owner lfd_owner, owner new_owner) {
    for(int i = 0; i < rl_all_files.nb_files; i++){
        // index of first rl_lock in lock_table
        int ind = rl_all_files.tab_open_files[i]->first;
        /* parcourir lock_table jusqu'au dernier élément indiqué par -1 */
        while(ind != -1){
            size_t nbOwners = rl_all_files.tab_open_files[i]->lock_table[ind].nb_owners;

            // parcours de lock_owners
            for(size_t j = 0; j < nbOwners; j++){
                if(ownerEquals(rl_all_files.tab_open_files[i]->lock_table[ind].lock_owners[j], lfd_owner)) {
                    // test si place pour ajouter nouveau propriétaire
                    if(nbOwners < NB_OWNERS) {
                        rl_all_files.tab_open_files[i]->lock_table[ind].lock_owners[nbOwners] = new_owner;
                        rl_all_files.tab_open_files[i]->lock_table[ind].nb_owners++;
                        break;
                    }
                    else {
                        return -1;
                    }
                }
            }
            // indice du prochain rl_lock
            ind = rl_all_files.tab_open_files[i]->lock_table[ind].next_lock;
        }
    }
    return 0;
}