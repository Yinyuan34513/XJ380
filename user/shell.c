#include <stdio.h>

int main(int argc, char *argv[], char *envp[])
{
    printf("shell process is running!!!\n");

    while (1)
        __asm__ __volatile__("pause");
}
