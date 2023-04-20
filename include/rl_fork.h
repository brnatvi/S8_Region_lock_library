#include "utils.h"

pid_t rl_fork();
//int add_new_owner();
int canAddNewOwnerByPid(pid_t parent, rl_open_file *f);
int addNewOwnerByPid(pid_t parent, pid_t fils, rl_open_file *f);