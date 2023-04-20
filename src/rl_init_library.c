#include "../include/rl_init_library.h"

int rl_init_library() {
    rl_all_files.nb_files = 0;
    memset(rl_all_files.tab_open_files, 0, sizeof(rl_open_file) * NB_FILES);
    return 0;

}