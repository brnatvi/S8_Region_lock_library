#include "rl_lock_library.h"
#include <unistd.h>

#define SHR_TEST_SEM        "/rl_test_shared_sem"

#define PROC_ERROR(Message) { fprintf(stderr, "%s : error {%s} in file {%s} on line {%d}\n", Message, strerror(errno), __FILE__, __LINE__); }

//https://en.wikipedia.org/wiki/ANSI_escape_code#SGR_(Select_Graphic_Rendition)_parameters
#define KNRM                "\x1B[0m\n"
#define KRED                "\x1B[31m"
#define KGRN                "\x1B[32m"
#define KYEL                "\x1B[33m"
#define KBLU                "\x1B[34m"
#define KMAG                "\x1B[35m"
#define KCYN                "\x1B[36m"
#define KWHT                "\x1B[37m"

#define SHR_TEST_SEM        "/rl_test_shared_sem"
#define TEST_ALL            0
static int indexTest = 0;


//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//IMPORTANT !!! If test is failed need manually remove shared objects from !
//                           /dev/shm                                      !
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!



#define TEST_EXEC(result, desc, testIndex)\
    if ((testIndex == indexTest) || (indexTest == TEST_ALL))\
    {\
        printf("\x1B[92;100m[%d]>>Execute test {%s} -------------------------------------------------" KNRM, getpid(), desc);\
        if (!result)\
        {\
            printf("\x1B[30;101m[%d]>>Test {%s} failed" KNRM, getpid(), desc);\
            res = -1;\
            goto lExit;\
        }\
        else\
        {\
            printf("\x1B[30;102m[%d]>>Test {%s} success" KNRM, getpid(), desc);\
        }\
    }\


bool test_reference_counter(const char *fileName)
{
    printf("file to open : %s\n", fileName);

    // open
    rl_descriptor rl_fd1 = rl_open(fileName, O_RDWR, S_IRUSR|S_IRGRP|S_IROTH|S_IWUSR|S_IWGRP|S_IWOTH);

    if (rl_fd1.f == NULL)
    {
        return false;
    }

    // dup2
    rl_descriptor rl_fd2 = rl_dup(rl_fd1);
    if (rl_fd2.f == NULL)
    {
        return false;
    }

    sem_t *sharedSem = sem_open(SHR_TEST_SEM, O_CREAT, S_IRUSR|S_IRGRP|S_IROTH|S_IWUSR|S_IWGRP|S_IWOTH, 0);

    // fork
    pid_t pid = rl_fork();
    if (-1 == pid)
    {
        return false;
    }
    if (0 == pid)
    {
        sharedSem = sem_open(SHR_TEST_SEM, 0);

        rl_print(rl_fd2);

        rl_close(rl_fd2);
        rl_close(rl_fd1);
        
        sem_post(sharedSem);
        sem_close(sharedSem);
        indexTest = 1; // only this test

        return true;
    }
    else
    {
        sem_wait(sharedSem);
        sem_close(sharedSem);
        sem_unlink(SHR_TEST_SEM);
        
        printf("close %d : \n", rl_fd1.d);

        rl_close(rl_fd2);

        return (0 == rl_close(rl_fd1));
    }
    return false;
}






int main(int argc, const char *argv[])
{
    if (argc != 3)
    {
        PROC_ERROR("wrong input, fist argument - name of the file, second - test index (0 == all tests)");
        return EXIT_FAILURE;
    }

    int res = EXIT_SUCCESS;    
    indexTest = atoi(argv[2]);

    if (rl_init_library() != 0)
    {
        res = -1;
        printf("\x1B[30;101m[%d]>>Test initialization failed" KNRM, getpid());
    }

    printf("[%d] file to Process : %s, test : %d\n", getpid(), argv[1], indexTest);

    TEST_EXEC(test_reference_counter(argv[1]), "test_reference_counter", 1);

lExit:
    printf("[%d] exit process\n", getpid());
    return res;
}