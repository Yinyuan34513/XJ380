#include "./include/xposix.h"
#include "./include/libsys.h"
#include "./include/xposix/syscall_ret.h"

extern "C" pid_t fork()
{
    return (pid_t)__xposix_ret(enter_syscall(SYS_FORK, 0, 0, 0, 0, 0, 0));
}

extern "C" pid_t vfork()
{
    return (pid_t)__xposix_ret(enter_syscall(SYS_VFORK, 0, 0, 0, 0, 0, 0));
}

extern "C" int wait(int *status)
{
    return (int)__xposix_ret(enter_syscall(SYS_WAIT4, (uint64_t)-1, (uint64_t)status, 0, 0, 0, 0));
}

extern "C" int waitpid(int pid, int *status, int options)
{
    return (int)__xposix_ret(enter_syscall(SYS_WAIT4, (uint64_t)pid, (uint64_t)status, (uint64_t)options, 0, 0, 0));
}

extern "C" int close(int fd)
{
    return (int)__xposix_ret(enter_syscall(SYS_CLOSE, fd, 0, 0, 0, 0, 0));
}

extern "C" int fcntl(int fd, int cmd, uint64_t arg)
{
    return (int)__xposix_ret(enter_syscall(SYS_FCNTL, fd, cmd, arg, 0, 0, 0));
}

extern "C" int execve(const char *filename, char *const argv[], char *const envp[])
{
    return (int)__xposix_ret(enter_syscall(SYS_EXECVE, (uint64_t)filename, (uint64_t)argv, (uint64_t)envp, 0, 0, 0));
}

extern "C" int chdir(const char *path)
{
    return (int)__xposix_ret(enter_syscall(SYS_CHDIR, (uint64_t)path, 0, 0, 0, 0, 0));
}

extern "C" char *getcwd(char *buffer, size_t length)
{
    if (buffer == NULL || length == 0) return NULL;
    if (__xposix_ret(enter_syscall(SYS_GETCWD, (uint64_t)buffer, length, 0, 0, 0, 0)) < 0) return NULL;
    return buffer;
}

extern "C" pid_t getpid(void)
{
    return (pid_t)__xposix_ret(enter_syscall(SYS_GETPID, 0, 0, 0, 0, 0, 0));
}

extern "C" pid_t getppid(void)
{
    return (pid_t)__xposix_ret(enter_syscall(SYS_GETPPID, 0, 0, 0, 0, 0, 0));
}

extern "C" pid_t gettid(void)
{
    return (pid_t)__xposix_ret(enter_syscall(SYS_GETTID, 0, 0, 0, 0, 0, 0));
}

extern "C" gid_t getgid(void)
{
    return (gid_t)__xposix_ret(enter_syscall(SYS_GETGID, 0, 0, 0, 0, 0, 0));
}

extern "C" uid_t getuid(void)
{
    // 目前 XJ380 还没有完整多用户权限模型；用户态先按 root 身份兼容 POSIX 工具。
    return 0;
}

extern "C" uid_t geteuid(void)
{
    // 有效 UID 与真实 UID 保持一致，避免 toybox 在权限检查路径上误判失败。
    return getuid();
}

extern "C" gid_t getegid(void)
{
    // 有效 GID 与真实 GID 保持一致；当前默认组就是 0。
    return getgid();
}

extern "C" int getgroups(int size, gid_t list[])
{
    // 暂无附加组概念；size 为 0 时返回组数量，非 0 时填入一个 root 组。
    if (size < 0) {
        errno = EINVAL;
        return -1;
    }
    if (size == 0) return 1;
    if (list == NULL) {
        errno = EINVAL;
        return -1;
    }
    list[0] = 0;
    return 1;
}

extern "C" int dup(int fd)
{
    return (int)__xposix_ret(enter_syscall(SYS_DUP, fd, 0, 0, 0, 0, 0));
}

extern "C" int dup2(int fd, int newfd)
{
    return (int)__xposix_ret(enter_syscall(SYS_DUP2, fd, newfd, 0, 0, 0, 0));
}

extern "C" int pipe(int pipefd[2])
{
    return (int)__xposix_ret(enter_syscall(SYS_PIPE, (uint64_t)pipefd, 0, 0, 0, 0, 0));
}

extern "C" int pipe2(int pipefd[2], int flags)
{
    return (int)__xposix_ret(enter_syscall(SYS_PIPE2, (uint64_t)pipefd, flags, 0, 0, 0, 0));
}

extern "C" int access(const char *path, int mode)
{
    (void)mode;
    return (int)__xposix_ret(enter_syscall(SYS_ACCESS, (uint64_t)path, 0, 0, 0, 0, 0));
}

extern "C" int unlink(const char *path)
{
    return (int)__xposix_ret(enter_syscall(SYS_UNLINK, (uint64_t)path, 0, 0, 0, 0, 0));
}

extern "C" int rmdir(const char *path)
{
    return (int)__xposix_ret(enter_syscall(SYS_RMDIR, (uint64_t)path, 0, 0, 0, 0, 0));
}

extern "C" int rename(const char *oldpath, const char *newpath)
{
    return (int)__xposix_ret(enter_syscall(SYS_RENAME, (uint64_t)oldpath, (uint64_t)newpath, 0, 0, 0, 0));
}

extern "C" int link(const char *oldpath, const char *newpath)
{
    return (int)__xposix_ret(enter_syscall(SYS_LINK, (uint64_t)oldpath, (uint64_t)newpath, 0, 0, 0, 0));
}

extern "C" int symlink(const char *target, const char *linkpath)
{
    return (int)__xposix_ret(enter_syscall(SYS_SYMLINK, (uint64_t)target, (uint64_t)linkpath, 0, 0, 0, 0));
}

extern "C" ssize_t readlink(const char *path, char *buf, size_t bufsiz)
{
    return (ssize_t)__xposix_ret(enter_syscall(SYS_READLINK, (uint64_t)path, (uint64_t)buf, bufsiz, 0, 0, 0));
}

extern "C" off_t lseek(int fd, off_t offset, int whence)
{
    return (off_t)__xposix_ret(enter_syscall(SYS_LSEEK, fd, (uint64_t)offset, whence, 0, 0, 0));
}

extern "C" int fsync(int fd)
{
    (void)fd;
    return 0;
}

extern "C" unsigned int sleep(unsigned int seconds)
{
    struct timespec ts = {(time_t)seconds, 0};
    nanosleep(&ts, NULL);
    return 0;
}

extern "C" int usleep(unsigned long usec)
{
    struct timespec ts = {(time_t)(usec / 1000000UL), (long)((usec % 1000000UL) * 1000UL)};
    return nanosleep(&ts, NULL);
}

extern "C" int isatty(int fd)
{
    // 早期终端兼容：标准输入/输出/错误视为 tty，其他 fd 先保守返回不是 tty。
    if (fd >= 0 && fd <= 2) return 1;
    errno = ENOTTY;
    return 0;
}

extern "C" char *ttyname(int fd)
{
    // toybox 只需要一个稳定名字用于提示/诊断；实际终端设备后续再接入 devfs。
    static char tty[] = "/dev/tty";
    if (!isatty(fd)) return NULL;
    return tty;
}
