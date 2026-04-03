#include "./include/xposix.h"
#include "./include/libsys.h"

uint64_t fork()
{
    return enter_syscall(SYS_FORK, 0, 0, 0, 0, 0, 0);
}

uint64_t vfork()
{
    return enter_syscall(SYS_VFORK, 0, 0, 0, 0, 0, 0);
}

int close(int fd)
{
    return enter_syscall(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
}

int fcntl(int fd, int cmd, uint64_t arg)
{
    return enter_syscall(SYS_FCNTL, fd, cmd, arg, 0, 0, 0);
}

uint64_t execve(char *filename, char *argv[], char *envp[])
{
    return enter_syscall(SYS_EXECVE, (uint64_t)filename, (uint64_t)argv, (uint64_t)envp, 0, 0, 0);
}

int chdir(char *path)
{
    return (int)enter_syscall(SYS_CHDIR, (uint64_t)path, 0, 0, 0, 0, 0);
}

char *getcwd(char *buffer, uint64_t length)
{
    if (buffer == NULL || length == 0) return NULL;
    int ret = (int)enter_syscall(SYS_GETCWD, (uint64_t)buffer, length, 0, 0, 0, 0);
    if (ret < 0) return NULL;
    return buffer;
}
