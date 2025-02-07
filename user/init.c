#include "./xlibc/include/stdint.h"
#include "./xlibc/include/libsys.h"

int main(int argc, char *argv[], char *envp[])
{
    enter_syscall(SYS_WRITE, 1, (uint64_t)"user process is running!!!\n", 0, 0, 0, 0);

    uint64_t pid = enter_syscall(SYS_FORK, 0, 0, 0, 0, 0, 0);
    if (pid == 0)
    {
        enter_syscall(SYS_WRITE, 1, (uint64_t)"child process is running!!!\n", 0, 0, 0, 0);
        // enter_syscall(SYS_EXECVE, (uint64_t)"/usr/shell.elf", NULL, NULL, 0, 0, 0);
        enter_syscall(SYS_EXIT, 0, 0, 0, 0, 0, 0);
    }
    else
    {
        enter_syscall(SYS_WRITE, 1, (uint64_t)"Parent process is running!!!\n", 0, 0, 0, 0);
        enter_syscall(SYS_EXIT, 0, 0, 0, 0, 0, 0);
    }
}
