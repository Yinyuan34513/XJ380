#include "./xapi/include/x3api.h"

int main(int argc, char *argv[], char *envp[])
{
    write(1, (char *)"init process is running!!!\n", 0);

    execve((char *)"/cp537.elf", NULL, NULL);
    
    while(1)__asm__ __volatile__("pause");
    return -1;
}
