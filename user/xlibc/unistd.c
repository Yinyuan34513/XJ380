#include "./include/unistd.h"
#include "./include/libsys.h"

uint64_t fork()
{
    return enter_syscall(SYS_FORK, 0, 0, 0, 0, 0, 0);
}

uint64_t vfork()
{
    return enter_syscall(SYS_VFORK, 0, 0, 0, 0, 0, 0);
}
