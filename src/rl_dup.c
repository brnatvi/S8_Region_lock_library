#include "rl_dup.h"
#include "rl_open.h"

rl_descriptor rl_dup(rl_descriptor lfd){
    int newd = dup(lfd.d);

    // Gestion erreur
    if(newd == -1) {
        PROC_ERROR("dup() failure"); // no close of newd (ref man dup)
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
    if(add_new_owner(lfd, newd) == -1){
        PROC_ERROR("rl_dup2() failure : NB_LOCKS at max");
        rl_descriptor bad = {.d = -1, .f = NULL};
        CLOSE_FILE(newd);
        return bad;
    }
    rl_descriptor new_rl_descriptor = {.d = newd, .f = lfd.f};
    return new_rl_descriptor;
}

int add_new_owner(rl_descriptor lfd, int newd) {
    owner lfd_owner = {.proc = getpid(), .des = lfd.d};
    owner new_owner = {.proc = getpid(), .des = newd};

    int ind = lfd.f->first;
    while(ind != -1) {
        size_t nbOwners = lfd.f->lock_table[ind].nb_owners;

        // parcours de lock_owners
        for(size_t j = 0; j < nbOwners; j++){
            if(ownerEquals(lfd.f->lock_table[ind].lock_owners[j], lfd_owner)) {
                // test si place pour ajouter nouveau propriétaire
                if(nbOwners < NB_OWNERS) {
                    lfd.f->lock_table[ind].lock_owners[nbOwners] = new_owner;
                    lfd.f->lock_table[ind].nb_owners++;
                    break;
                }
                else {
                    return -1;
                }
            }
        }
        // indice du prochain rl_lock
        ind = lfd.f->lock_table[ind].next_lock;       
    }
    return 0;
}

int main(int argc, const char *argv[])
{
    return EXIT_SUCCESS;
}