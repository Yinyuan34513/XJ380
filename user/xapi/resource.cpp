#include "./include/libsys.h"
#include "./include/xposix/sys/resource.h"
#include "./include/xposix/syscall_ret.h"

extern "C" int getrlimit(int resource, struct rlimit *limit)
{
    return (int)__xposix_ret(enter_syscall(SYS_GETRLIMIT, resource, (uint64_t)limit, 0, 0, 0, 0));
}

extern "C" int getpriority(int which, int who)
{
    long priority = __xposix_ret(enter_syscall(SYS_GETPRIORITY, which, who, 0, 0, 0, 0));
    if (priority < 0) return -1;
    return 20 - (int)priority;
}

extern "C" int setpriority(int which, int who, int nice)
{
    return (int)__xposix_ret(enter_syscall(SYS_SETPRIORITY, which, who, nice, 0, 0, 0));
}
