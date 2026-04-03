#include "./xapi/include/x3api.h"

static int init_main_impl(int argc, char *argv[], char *envp[])
{
    write(1, (char *)"init process is running!!!\n", 0);

    execve((char *)"/cp537.elf", NULL, NULL);
    
    while(1)__asm__ __volatile__("pause");
    return -1;
}

extern "C" int init_main_cpp(int argc, char *argv[], char *envp[]) asm("_Z4mainiPPcS0_");
extern "C" int init_main_cpp(int argc, char *argv[], char *envp[])
{
    return init_main_impl(argc, argv, envp);
}
