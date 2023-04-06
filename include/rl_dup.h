#include "utils.h"

rl_descriptor rl_dup(rl_descriptor lfd);
rl_descriptor rl_dup2(rl_descriptor lfd, int newd);
rl_descriptor dup_function(rl_descriptor lfd, int newd);
int add_new_owner(owner lfd_owner, owner new_owner);