#include "./xapi/include/x3api.h"

static int init_main_impl(int argc, char *argv[], char *envp[])
{
    (void)argc;
    (void)argv;
    (void)envp;

    const char *sh_path = "/usr/bin/bash";
    char *const sh_argv[] = {
        (char *)"bash",
        (char *)"/system/resources/start-weston.sh",
        NULL
    };

    mkdir("/run/user/0", 0700);

    execve(sh_path, sh_argv, NULL);
    return 1;
}

extern "C" int init_main_cpp(int argc, char *argv[], char *envp[]) asm("_Z4mainiPPcS0_");
extern "C" int init_main_cpp(int argc, char *argv[], char *envp[])
{
    return init_main_impl(argc, argv, envp);
}
