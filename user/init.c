#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[], char *envp[])
{
    write(1, "init process is running!!!\n", 28);

    int pid = vfork();
    if (pid == 0)
    {
        write(1, "child process is running!!!\n", 29);
        execve("/usr/shell.elf", NULL, NULL);
        exit(-1);
    }
    else
    {
        write(1, "parent process is running!!!\n", 30);
        while (1)
            __asm__ __volatile__("pause");
    }

    return -1;
}
