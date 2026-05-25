#include "./include/xposix.h"
#include "./include/libsys.h"
#include "./include/xposix/syscall_ret.h"

extern "C" int kill(pid_t pid, int sig)
{
    return (int)__xposix_ret(enter_syscall(SYS_KILL, pid, sig, 0, 0, 0, 0));
}

extern "C" int tgkill(int tgid, int tid, int sig)
{
    return (int)__xposix_ret(enter_syscall(SYS_TGKILL, tgid, tid, sig, 0, 0, 0));
}

extern "C" int sigaction(int sig, const struct sigaction *act, struct sigaction *oldact)
{
    return (int)__xposix_ret(enter_syscall(SYS_RT_SIGACTION, sig, (uint64_t)act, (uint64_t)oldact, sizeof(sigset_t), 0, 0));
}

extern "C" int sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
    return (int)__xposix_ret(enter_syscall(SYS_RT_SIGPROCMASK, how, (uint64_t)set, (uint64_t)oldset, 0, 0, 0));
}
