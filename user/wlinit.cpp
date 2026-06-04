#include "xapi/include/xposix.h"

static int wlinit_main_impl(int argc, char *argv[], char *envp[])
{
    (void)argc;
    (void)argv;
    (void)envp;

    const char *sh_path = "/usr/bin/bash";
    char *const sh_argv[] = {
        (char *)"bash",
        (char *)"/system/resources/start-wayland.sh",
        NULL
    };

    mkdir("/run/user/0", 0700);

    execve(sh_path, sh_argv, NULL);
    return 1;
}

extern "C" int wlinit_main(int argc, char *argv[], char *envp[]) asm("_Z4mainiPPcS0_");
extern "C" int wlinit_main(int, char **, char **)
{
    return wlinit_main_impl(0, NULL, NULL);
}
