#include "./xlibc/include/stdint.h"
#include "./xlibc/include/stdlib.h"
#include "./xlibc/include/unistd.h"

int main(int argc, char *argv[], char *envp[])
{
    write(1, "init process is running!!!\n", 0);

    int pid = vfork();
    if (pid == 0)
    {
        write(1, "child process is running!!!\n", 0);
        execve("/usr/shell.elf", NULL, NULL);
    }
    else
    {
        write(1, "parent process is running!!!\n", 0);
        while (1)
            __asm__ __volatile__("pause");
    }

    return -1;
}
