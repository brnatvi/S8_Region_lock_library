#include "utils.h"

/**
 * TODO a good deployed description.
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