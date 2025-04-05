#include "./xlibc/include/stdio.h"

int main(int argc, char *argv[], char *envp[])
{
    write(1, "shell process is running!!!\n", 0);

    while (1)
        __asm__ __volatile__("pause");
}
