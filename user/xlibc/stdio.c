#include "./include/unistd.h"
#include "./include/libsys.h"

int read(int fd, char *buf, uint64_t len)
{
    return enter_syscall(SYS_READ, fd, (uint64_t)buf, len, 0, 0, 0);
}

int write(int fd, char *buf, uint64_t len)
{
    return enter_syscall(SYS_WRITE, fd, (uint64_t)buf, len, 0, 0, 0);
}
