#include "utils.h"

rl_descriptor rl_dup(rl_descriptor lfd);
rl_descriptor rl_dup2(rl_descriptor lfd, int newd);
// int add_new_owner(rl_descriptor lfd, int newd);
int canAddNewOwner(owner own, rl_open_file *f);
int addNewOwner(owner own, owner new_owner, rl_open_file *f);