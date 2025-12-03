/*
 *
 *
 *      XJ380 终端应用程序启动框架
 *      2025/11/5 - GuoqiFish
 *      Copyright(C) XINGJI Interactive Software 2017 - 2025 All rights reserved.
 * 
 * 
 */

#include "./include/x3api.h"
#include "./include/libsys.h"

extern int main(int argc, char *argv[], char *envp[]);
// extern void _init();
// extern void __preinit_array_start();
// extern void __preinit_array_end();
// extern void __init_array_start();
// extern void __init_array_end();
// extern void __fini_array_start();
// extern void __fini_array_end();

#define SXAH_CHECK_TERMINAL_INIT_STATUS  3801    // 查询终端初始化状态

extern "C" void xapi_start(int argc, char *argv[], char *envp[])
{
    //{
    //    void (*fn)() = __preinit_array_start;
    //    while (fn != __preinit_array_end)
    //    {
    //        (*fn)();
    //        fn++;
    //    }
    //}

    //_init();

    //{
    //    void (*fn)() = __init_array_start;
    //    while (fn != __init_array_end)
    //    {
    //        (*fn)();
    //        fn++;
    //    }
    //}

    uint64_t pid  = xapi_Fork();

    if (pid == 0)
    {
        // 是子进程！
        // 等待命令行初始化
        while (true)
        {
            // 这时命令行肯定是他的父进程，循环检查是否标记为命令行，检测到标记即代表可以吃了
            if (enter_syscall(SXAH_CHECK_TERMINAL_INIT_STATUS, 0, 0, 0, 0, 0, 0)) break;
        }
        // return; // 国企鱼是超级大傻逼
        exit(main(argc, argv, envp));
    }

    char *execve_argv[2] = {"-xj380sys-terminal-app-mode"};
    xapi_Execve("/system/shell.elf", execve_argv, NULL);

    exit(main(argc,argv,envp));
}
