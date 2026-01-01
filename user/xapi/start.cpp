/*
 *
 *
 *      XJ380 图形化应用程序启动框架
 *      2025/11/5 - GuoqiFish
 *      Copyright(C) XINGJI Interactive Software 2017 - 2026 All rights reserved.
 * 
 * 
 */

#include "./include/xposix.h"

extern int main(int argc, char *argv[], char *envp[]);
// extern void _init();
// extern void __preinit_array_start();
// extern void __preinit_array_end();
// extern void __init_array_start();
// extern void __init_array_end();
// extern void __fini_array_start();
// extern void __fini_array_end();

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

    exit(main(argc, argv, envp));
}
