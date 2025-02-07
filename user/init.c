#include "./xlibc/include/stdint.h"
#include "./xlibc/include/unistd.h"
#include "./xlibc/include/stdlib.h"

int main(int argc, char *argv[], char *envp[])
{
    write(1, "user process is running!!!\n", 0);

    uint64_t pid = fork();
    if (pid == 0)
    {
        write(1, "child process is running!!!\n", 0);
        // enter_syscall(SYS_EXECVE, (uint64_t)"/usr/shell.elf", NULL, NULL, 0, 0, 0);
    }
    else
    {
        write(1, "Parent process is running!!!\n", 0);
    }

    return 0;
}
