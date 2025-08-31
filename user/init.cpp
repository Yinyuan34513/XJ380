#include "./xapi/include/x3api.h"

int main(int argc, char *argv[], char *envp[])
{
    write(1, (char *)"init process is running!!!\n", 0);

    // uint64_t pid = vfork();
    // if (pid == 0)
    // {
    //     write(1, "child process is running!!!\n", 0);
    // execve("/usr/shell.elf", NULL, NULL);
    execve((char *)"/cp537.elf", NULL, NULL);
    // }
    // else
    // {
    //     write(1, "Parent process is running!!!\n", 0);
    // }
    
    while(1)__asm__ __volatile__("pause");
    return -1;
}
