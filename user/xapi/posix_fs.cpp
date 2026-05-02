#include "./include/fcntl.h"
#include "./include/dirent.h"
#include "./include/unistd.h"
#include "./include/sys/stat.h"
#include "./include/xposix/syscall_ret.h"
#include "./include/libsys.h"
#include "./include/krlibc.h"
#include "./include/liballoc/alloc.h"
#include "./include/xposix/stdarg.h"
#include "./include/sys/ioctl.h"

#define TIOCGPTN   0x80045430
#define TIOCSPTLCK 0x40045431

extern "C" int open(const char *path, int flags, ...)
{
    mode_t mode = 0644;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = (mode_t)va_arg(ap, int);
        va_end(ap);
    }
    return (int)__xposix_ret(enter_syscall(SYS_OPEN, (uint64_t)path, flags, mode, 0, 0, 0));
}

extern "C" int openat(int dirfd, const char *path, int flags, ...)
{
    mode_t mode = 0644;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = (mode_t)va_arg(ap, int);
        va_end(ap);
    }
    return (int)__xposix_ret(enter_syscall(SYS_OPENAT, dirfd, (uint64_t)path, flags, mode, 0, 0));
}

extern "C" int creat(const char *path, unsigned int mode)
{
    return open(path, O_CREAT | O_TRUNC | O_WRONLY, mode);
}

extern "C" int posix_openpt(int flags)
{
    return open("/dev/ptmx", flags);
}

extern "C" int grantpt(int fd)
{
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    return 0;
}

extern "C" int unlockpt(int fd)
{
    int lock = 0;
    return ioctl(fd, TIOCSPTLCK, &lock);
}

extern "C" char *ptsname(int fd)
{
    static char path[32];
    int id = -1;
    if (ioctl(fd, TIOCGPTN, &id) < 0) return NULL;
    snprintf(path, sizeof(path), "/dev/pts/%d", id);
    return path;
}

extern "C" int stat(const char *path, struct stat *buf)
{
    return (int)__xposix_ret(enter_syscall(SYS_STAT, (uint64_t)path, (uint64_t)buf, 0, 0, 0, 0));
}

extern "C" int lstat(const char *path, struct stat *buf)
{
    return (int)__xposix_ret(enter_syscall(SYS_LSTAT, (uint64_t)path, (uint64_t)buf, 0, 0, 0, 0));
}

extern "C" int fstat(int fd, struct stat *buf)
{
    return (int)__xposix_ret(enter_syscall(SYS_FSTAT, fd, (uint64_t)buf, 0, 0, 0, 0));
}

extern "C" int fstatat(int dirfd, const char *path, struct stat *buf, int flags)
{
    // fstatat 是 stat/openat 系工具的公共入口；内核暂不完整解释 flags，用户态原样转发。
    return (int)__xposix_ret(enter_syscall(SYS_NEWFSTATAT, dirfd, (uint64_t)path, (uint64_t)buf, flags, 0, 0));
}

extern "C" int mkdir(const char *path, mode_t mode)
{
    return (int)__xposix_ret(enter_syscall(SYS_MKDIR, (uint64_t)path, mode, 0, 0, 0, 0));
}

extern "C" int chmod(const char *path, mode_t mode)
{
    // chmod 先接到内核 syscall；若文件系统忽略权限位，也应由内核保持兼容返回。
    return (int)__xposix_ret(enter_syscall(SYS_CHMOD, (uint64_t)path, mode, 0, 0, 0, 0));
}

extern "C" int fchmod(int fd, mode_t mode)
{
    // fchmod 是 fd 版本权限设置，供 cp/install 等工具链接使用。
    return (int)__xposix_ret(enter_syscall(SYS_FCHMOD, fd, mode, 0, 0, 0, 0));
}

extern "C" int chown(const char *path, uid_t owner, gid_t group)
{
    // 当前系统没有真实属主模型；只验证参数形态并把 chown 视为成功的兼容空操作。
    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }
    (void)owner;
    (void)group;
    return 0;
}

extern "C" int lchown(const char *path, uid_t owner, gid_t group)
{
    // 还没有符号链接属主差异，lchown 与 chown 使用相同兼容行为。
    return chown(path, owner, group);
}

extern "C" int fchown(int fd, uid_t owner, gid_t group)
{
    // fd 有效性先做最小检查；属主变更仍是兼容空操作。
    if (fd < 0) {
        errno = EBADF;
        return -1;
    }
    (void)owner;
    (void)group;
    return 0;
}

extern "C" int truncate(const char *path, off_t length)
{
    // 还没有通用截断 syscall；明确返回 ENOSYS，方便上层降级或报错。
    (void)path;
    (void)length;
    errno = ENOSYS;
    return -1;
}

extern "C" int ftruncate(int fd, off_t length)
{
    // fd 截断同样等待 VFS 支持；当前只提供可链接的 POSIX 入口。
    (void)fd;
    (void)length;
    errno = ENOSYS;
    return -1;
}

extern "C" int utimensat(int dirfd, const char *path, const struct timespec times[2], int flags)
{
    // FAT/当前 VFS 时间戳写入尚不完整；先作为成功空操作兼容 cp -p/touch 的普通路径。
    (void)dirfd;
    (void)path;
    (void)times;
    (void)flags;
    return 0;
}

extern "C" int getdents(int fd, struct dirent *dents, size_t size)
{
    return (int)__xposix_ret(enter_syscall(SYS_GETDENTS, fd, (uint64_t)dents, size, 0, 0, 0));
}

extern "C" DIR *opendir(const char *path)
{
    int fd = open(path, O_RDONLY | O_DIRECTORY);
    if (fd < 0) return NULL;

    DIR *dir = (DIR *)calloc(1, sizeof(DIR));
    if (dir == NULL) {
        close(fd);
        errno = ENOMEM;
        return NULL;
    }
    dir->fd = fd;
    return dir;
}

extern "C" struct dirent *readdir(DIR *dirp)
{
    if (dirp == NULL) {
        errno = EINVAL;
        return NULL;
    }
    if (dirp->pos >= dirp->len) {
        int got = getdents(dirp->fd, (struct dirent *)dirp->buf, sizeof(dirp->buf));
        if (got <= 0) return NULL;
        dirp->pos = 0;
        dirp->len = got;
    }

    struct dirent *ent = (struct dirent *)(dirp->buf + dirp->pos);
    if (ent->d_reclen == 0 || dirp->pos + ent->d_reclen > dirp->len) {
        errno = EINVAL;
        return NULL;
    }
    dirp->pos += ent->d_reclen;
    return ent;
}

extern "C" int closedir(DIR *dirp)
{
    if (dirp == NULL) {
        errno = EINVAL;
        return -1;
    }
    int ret = close(dirp->fd);
    free(dirp);
    return ret;
}
