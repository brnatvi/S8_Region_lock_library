#include "rl_lock_library.h"

#define PROC_ERROR(Message) { fprintf(stderr, "%s : error {%s} in file {%s} on line {%d}\n", Message, strerror(errno), __FILE__, __LINE__); }


int main(int argc, const char *argv[])
{
    if (argc != 2)
    {
        PROC_ERROR("wrong input");
        return EXIT_FAILURE;
    }
    printf("file to open : %s\n", argv[1]);

    rl_descriptor rl_fd1 = rl_open(argv[1], O_RDWR, S_IRUSR|S_IRGRP|S_IROTH|S_IWUSR|S_IWGRP|S_IWOTH);
    rl_descriptor rl_fd2 = rl_open(argv[1], O_RDWR, S_IRUSR|S_IRGRP|S_IROTH|S_IWUSR|S_IWGRP|S_IWOTH);

    printf("fd1 = %d, fd2 = %d\n", rl_fd1.d, rl_fd2.d);

    rl_close(rl_fd2);
    rl_close(rl_fd1);

    return EXIT_SUCCESS;
}
