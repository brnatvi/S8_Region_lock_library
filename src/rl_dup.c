#include "../include/rl_dup.h"

rl_descriptor rl_dup(rl_descriptor lfd){
    int newd = dup(lfd.d);

    // Gestion erreur
    if(newd == -1) {
        PROC_ERROR("dup() failure"); // no close of newd (ref man dup)
        rl_descriptor bad = {.d = -1, .f = NULL};
        return bad;
    }
    owner own = {.des = lfd.d, .proc = getpid()};
    int add = canAddNewOwner(own, lfd.f);
    if(add == -1) {
        PROC_ERROR("rl_dup() failure NB_OWNERS at max");
        rl_descriptor bad = {.d = -1, .f = NULL};
        return bad;
    }
    owner new_owner = {.des = newd, .proc = getpid()};
    if(add == 1) {
        addNewOwner(own, new_owner, lfd.f);
    }
}

rl_descriptor rl_dup2(rl_descriptor lfd, int newd) {
    if(dup2(lfd.d, newd) == -1) {
        PROC_ERROR("dup2() failure"); // no close of newd
        rl_descriptor bad = {.d = -1, .f = NULL};
        return bad;
    }
    owner own = {.des = lfd.d, .proc = getpid()};
    int add = canAddNewOwner(own, lfd.f);
    if(add == -1) {
        PROC_ERROR("rl_dup() failure NB_OWNERS at max");
        rl_descriptor bad = {.d = -1, .f = NULL};
        return bad;
    }
    owner new_owner = {.des = newd, .proc = getpid()};
    if(add == 1) {
        addNewOwner(own, new_owner, lfd.f);
    }
    rl_descriptor new_descr = {.d = newd, .f = lfd.f};
    return new_descr;
    
}

// 1 si oui, 0 si own non dans table, -1 si capacité max
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

// assume we can add
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

