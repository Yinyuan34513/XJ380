#include "./include/xposix.h"
#include "./include/libsys.h"

uint64_t fork()
{
    return enter_syscall(SYS_FORK, 0, 0, 0, 0, 0, 0);
}

uint64_t vfork()
{
    return enter_syscall(SYS_VFORK, 0, 0, 0, 0, 0, 0);
}

uint64_t execve(char *filename, char *argv[], char *envp[])
{
    return enter_syscall(SYS_EXECVE, (uint64_t)filename, (uint64_t)argv, (uint64_t)envp, 0, 0, 0);
}
