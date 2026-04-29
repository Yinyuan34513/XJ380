#include "./include/fcntl.h"
#include "./include/dirent.h"
#include "./include/unistd.h"
#include "./include/sys/stat.h"
#include "./include/xposix/syscall_ret.h"
#include "./include/libsys.h"
#include "./include/krlibc.h"
#include "./include/liballoc/alloc.h"
#include "./include/xposix/stdarg.h"

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

extern "C" int mkdir(const char *path, mode_t mode)
{
    return (int)__xposix_ret(enter_syscall(SYS_MKDIR, (uint64_t)path, mode, 0, 0, 0, 0));
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
