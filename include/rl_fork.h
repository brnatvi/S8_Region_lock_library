#include "utils.h"

pid_t rl_fork();
//int add_new_owner();
int canAddNewOwner(pid_t parent, rl_open_file *f);
int addNewOwner(pid_t parent, pid_t fils, rl_open_file *f);