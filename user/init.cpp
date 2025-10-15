#include "./xapi/include/x3api.h"

int main(int argc, char *argv[], char *envp[])
{
    write(1, (char *)"init process is running!!!\n", 0);

    char *av[]={"mkdir","/GuoqiFish"};
    execve((char *)"/system/shell.elf", av, NULL);
    
    while(1)__asm__ __volatile__("pause");
    return -1;
}
