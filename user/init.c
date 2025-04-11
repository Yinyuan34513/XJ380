#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[], char *envp[])
{
    // printf("init process is running!!!\n");

    // int pid = fork();
    // if (pid == 0)
    // {
    //     printf("child process is running!!!\n");
    //     execve("/usr/shell.elf", NULL, NULL);
    //     while (1)
    //         __asm__ __volatile__("pause");
    // }
    // else
    // {
    //     printf("parent process is running!!! child pid = %d\n", pid);
    //     while (1)
    //         __asm__ __volatile__("pause");
    // }
    while(1)__asm__ __volatile__("pause");
    return -1;
}
