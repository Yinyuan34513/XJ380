#include "./xlibc/include/stdint.h"
#include "./xlibc/include/stdlib.h"
#include "./xlibc/include/unistd.h"

int main(int argc, char *argv[], char *envp[])
{
    write(1, "init process is running!!!\n", 0);

    execve("/usr/shell.elf", NULL, NULL);

    return 0;
}
