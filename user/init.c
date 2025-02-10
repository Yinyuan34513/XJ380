#include "./xlibc/include/stdint.h"
#include "./xlibc/include/unistd.h"
#include "./xlibc/include/stdlib.h"

int main(int argc, char *argv[], char *envp[])
{
    // uint64_t pid = vfork();
    // if (pid == 0)
    // {
    //     write(1, "child process is running!!!\n", 0);
    execve("/usr/shell.elf", NULL, NULL);
    // }
    // else
    // {
    //     write(1, "Parent process is running!!!\n", 0);
    // }

    return 0;
}
