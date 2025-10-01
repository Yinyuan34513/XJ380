#include "errno.h"
#include <fs/fs_syscall.h>
#include <fs/vfs/vfs.h>
#include <task/pcb.h>

static int eventfd_id = 0;

char *at_resolve_pathname(int dirfd, char *pathname)
{
    if (pathname[0] == '/')
    { // by absolute pathname
        return strdup(pathname);
    }
    else if (pathname[0] != '/')
    {
        if (dirfd == AT_FDCWD)
        { // relative to cwd
            return strdup(pathname);
        }
        else
        { // relative to dirfd, resolve accordingly
            vfs_node_t node = get_current_task()->parent_group->fds[dirfd];
            if (!node) return (char *)-EBADF;
            if (node->type != file_dir) return (char *)-ENOTDIR;

            char *dirname = vfs_get_fullpath(node);

            char *prefix = vfs_get_fullpath(node->root);

            int prefixLen   = strlen(prefix);
            int rootDirLen  = strlen(dirname);
            int pathnameLen = strlen(pathname) + 1;

            char *out = (char *)malloc(prefixLen + rootDirLen + 1 + pathnameLen + 1);

            memcpy(out, prefix, prefixLen);
            memcpy(&out[prefixLen], dirname, rootDirLen);
            out[prefixLen + rootDirLen] = '/';
            memcpy(&out[prefixLen + rootDirLen + 1], pathname, pathnameLen);

            free(dirname);
            free(prefix);

            return out;
        }
    }

    return NULL;
}

uint64_t sys_open(const char *name, uint64_t flags, uint64_t mode)
{
    if (!name) { return (uint64_t)-EFAULT; }

    uint64_t i = 0;
    for (i = 3; i < 256; i++)
    {
        if (get_current_task()->parent_group->fds[i] == NULL) { break; }
    }

    if (i == 256) { return (uint64_t)-EBADF; }

    int create_mode = (flags & O_CREAT);

    // write_serial_fmt("Opening file %s\n", name);

    vfs_node_t node = vfs_open(name);
    if (!node && !create_mode) { return (uint64_t)-ENOENT; }

    if (!node)
    {
        int ret = 0;
        if (mode & O_DIRECTORY) { ret = vfs_mkdir(name); }
        else { ret = vfs_mkfile(name); }
        if (ret < 0) return (uint64_t)-ENOSPC;

        node = vfs_open(name);
        if (!node) return (uint64_t)-ENOENT;
    }

    node->flags = flags;

    get_current_task()->parent_group->fds[i] = node;

    return i;
}

uint64_t sys_openat(uint64_t dirfd, const char *name, uint64_t flags, uint64_t mode)
{
    char *path = at_resolve_pathname(dirfd, (char *)name);
    if (!path) return (uint64_t)-ENOMEM;

    uint64_t ret = sys_open(path, flags, mode);

    free(path);

    return ret;
}

uint64_t sys_close(uint64_t fd)
{
    if (fd >= 256 || get_current_task()->parent_group->fds[fd] == NULL) { return (uint64_t)-EBADF; }

    get_current_task()->parent_group->fds[fd]->offset = 0;
    if (get_current_task()->parent_group->fds[fd]->lock.l_pid == get_current_task()->parent_group->pid)
    {
        get_current_task()->parent_group->fds[fd]->lock.l_type = F_UNLCK;
        get_current_task()->parent_group->fds[fd]->lock.l_pid  = 0;
    }

    vfs_close(get_current_task()->parent_group->fds[fd]);

    get_current_task()->parent_group->fds[fd] = NULL;

    return 0;
}

uint64_t sys_read(uint64_t fd, void *buf, uint64_t len)
{
    if (fd >= 256 || get_current_task()->parent_group->fds[fd] == NULL) { return (uint64_t)-EBADF; }

    if (get_current_task()->parent_group->fds[fd]->type & file_dir)
    {
        return (uint64_t)-EISDIR; // 读取目录时返回正确错误码
    }

    if (!buf) { return (uint64_t)-EFAULT; }

    ssize_t ret = vfs_read(get_current_task()->parent_group->fds[fd], buf, get_current_task()->parent_group->fds[fd]->offset, len);

    if (ret > 0) { get_current_task()->parent_group->fds[fd]->offset += ret; }

    if (ret == -EAGAIN)
    {
        return (uint64_t)-EAGAIN; // 保持非阻塞I/O语义
    }

    return ret;
}

uint64_t sys_write(uint64_t fd, const void *buf, uint64_t len)
{
    if (fd == 1)
    {
        write_serial_fmt((char *)buf);
        return 0;
    }
    if (fd >= 256 || get_current_task()->parent_group->fds[fd] == NULL) { return (uint64_t)-EBADF; }

    if (get_current_task()->parent_group->fds[fd]->type & file_dir)
    {
        return (uint64_t)-EISDIR; // 读取目录时返回正确错误码
    }

    if (!buf) { return (uint64_t)-EFAULT; }

    ssize_t ret = vfs_write(get_current_task()->parent_group->fds[fd], buf, get_current_task()->parent_group->fds[fd]->offset, len);

    if (ret > 0) { get_current_task()->parent_group->fds[fd]->offset += ret; }

    if (ret == -EAGAIN)
    {
        return (uint64_t)-EAGAIN; // 保持非阻塞I/O语义
    }

    return ret;
}

uint64_t sys_lseek(uint64_t fd, uint64_t offset, uint64_t whence)
{
    if (fd >= 256 || get_current_task()->parent_group->fds[fd] == NULL) { return (uint64_t)-EBADF; }

    int64_t real_offset = offset;
    if (real_offset < 0 && get_current_task()->parent_group->fds[fd]->type & file_none && whence != SEEK_CUR) return (uint64_t)-EBADF;

    switch (whence)
    {
    case SEEK_SET: get_current_task()->parent_group->fds[fd]->offset = real_offset; break;
    case SEEK_CUR:
        get_current_task()->parent_group->fds[fd]->offset += real_offset;
        if ((int64_t)get_current_task()->parent_group->fds[fd]->offset < 0) { get_current_task()->parent_group->fds[fd]->offset = 0; }
        else if (get_current_task()->parent_group->fds[fd]->offset > get_current_task()->parent_group->fds[fd]->size)
        {
            get_current_task()->parent_group->fds[fd]->offset = get_current_task()->parent_group->fds[fd]->size;
        }

        break;
    case SEEK_END: get_current_task()->parent_group->fds[fd]->offset = get_current_task()->parent_group->fds[fd]->size - real_offset; break;

    default: return (uint64_t)-ENOSYS; break;
    }

    return get_current_task()->parent_group->fds[fd]->offset;
}

uint64_t sys_ioctl(uint64_t fd, uint64_t cmd, uint64_t arg)
{
    if (fd >= 256 || get_current_task()->parent_group->fds[fd] == NULL) { return (uint64_t)-EBADF; }

    return vfs_ioctl(get_current_task()->parent_group->fds[fd], cmd, arg);
}

uint64_t sys_readv(uint64_t fd, struct iovec *iovec, uint64_t count)
{
    if ((uint64_t)iovec == 0) { return -EINVAL; }

    ssize_t total_read = 0;
    for (uint64_t i = 0; i < count; i++)
    {
        if (iovec[i].len == 0) continue;

        ssize_t ret = sys_read(fd, iovec[i].iov_base, iovec[i].len);
        if (ret < 0) { return (uint64_t)ret; }
        total_read += ret;
        if ((size_t)ret < iovec[i].len) break;
    }
    return total_read;
}

uint64_t sys_writev(uint64_t fd, struct iovec *iovec, uint64_t count)
{
    if ((uint64_t)iovec == 0) { return -EINVAL; }

    ssize_t total_written = 0;
    for (uint64_t i = 0; i < count; i++)
    {
        if (iovec[i].len == 0) continue;

        ssize_t ret = sys_write(fd, iovec[i].iov_base, iovec[i].len);
        if (ret < 0) { return (uint64_t)ret; }
        total_written += ret;
        if ((size_t)ret < iovec[i].len) break;
    }
    return total_written;
}

uint64_t sys_getdents(uint64_t fd, uint64_t buf, uint64_t size)
{
    if (fd >= 256) return (uint64_t)-EBADF;
    if (!get_current_task()->parent_group->fds[fd]) return (uint64_t)-EBADF;
    if (get_current_task()->parent_group->fds[fd]->type != file_dir) return (uint64_t)-ENOTDIR;

    struct dirent *dents = (struct dirent *)buf;
    vfs_node_t     node  = get_current_task()->parent_group->fds[fd];

    uint64_t child_count = (uint64_t)list_length(node->child);

    int64_t max_dents_num = size / sizeof(struct dirent);

    int64_t read_count = 0;

    uint64_t offset = 0;
    list_foreach(node->child, i)
    {
        vfs_node_t child_node = NULL;
        if (offset < node->offset) goto next;
        if (node->offset >= (child_count * sizeof(struct dirent))) break;
        if (read_count >= max_dents_num) break;
        child_node                 = (vfs_node_t)i->data;
        dents[read_count].d_ino    = child_node->inode;
        dents[read_count].d_off    = node->offset;
        dents[read_count].d_reclen = sizeof(struct dirent);
        switch (child_node->type)
        {
        case file_dir: dents[read_count].d_type = DT_DIR; break;
        case file_none: dents[read_count].d_type = DT_REG; break;
        case file_symlink: dents[read_count].d_type = DT_LNK; break;
        default: dents[read_count].d_type = DT_UNKNOWN; break;
        }
        strncpy(dents[read_count].d_name, child_node->name, 1024);
        node->offset += sizeof(struct dirent);
        read_count++;
    next:
        offset += sizeof(struct dirent);
    }

    return read_count * sizeof(struct dirent);
}

uint64_t sys_chdir(const char *dirname)
{
    vfs_node_t new_cwd = vfs_open(dirname);
    if (!new_cwd) return (uint64_t)-ENOENT;
    if (new_cwd->type != file_dir) return (uint64_t)-ENOTDIR;

    get_current_task()->parent_group->cwd = new_cwd;

    return 0;
}

uint64_t sys_getcwd(char *cwd, uint64_t size)
{
    char *str = vfs_get_fullpath(get_current_task()->parent_group->cwd);
    if (size < (uint64_t)strlen(str)) { return (uint64_t)-ERANGE; }
    strncpy(cwd, str, size);
    free(str);
    return (uint64_t)strlen(str);
}

extern int unix_socket_fsid;
extern int unix_accept_fsid;

// Implement the sys_dup3 function
uint64_t sys_dup3(uint64_t oldfd, uint64_t newfd, uint64_t flags)
{
    if (oldfd >= 256 || get_current_task()->parent_group->fds[oldfd] == NULL) { return -EBADF; }

    if (newfd >= 256) { return -EBADF; }

    if (flags & ~O_CLOEXEC) { return -EINVAL; }

    if (oldfd == newfd) { return -EBADF; }

    if (get_current_task()->parent_group->fds[newfd] != NULL) { sys_close(newfd); }

    vfs_node_t new_node = vfs_dup(get_current_task()->parent_group->fds[oldfd]);
    if (new_node == NULL) { return -EMFILE; }

    get_current_task()->parent_group->fds[newfd] = new_node;

    if (flags & O_CLOEXEC) { new_node->flags |= O_CLOEXEC; }

    return newfd;
}

uint64_t sys_dup2(uint64_t fd, uint64_t newfd)
{
    vfs_node_t node = get_current_task()->parent_group->fds[fd];
    if (!node) return (uint64_t)-EBADF;

    vfs_node_t nw = vfs_dup(node);
    if (!nw) return (uint64_t)-ENOSPC;

    if (get_current_task()->parent_group->fds[newfd]) { vfs_close(get_current_task()->parent_group->fds[newfd]); }
    get_current_task()->parent_group->fds[newfd] = nw;

    return newfd;
}

uint64_t sys_dup(uint64_t fd)
{
    vfs_node_t node = get_current_task()->parent_group->fds[fd];
    if (!node) return (uint64_t)-EBADF;

    uint64_t i;
    for (i = 3; i < 256; i++)
    {
        if (get_current_task()->parent_group->fds[i] == NULL) { break; }
    }

    if (i == 256) { return (uint64_t)-EBADF; }

    return sys_dup2(fd, i);
}

uint64_t sys_fcntl(uint64_t fd, uint64_t command, uint64_t arg)
{
    if (!get_current_task()->parent_group->fds[fd]) return (uint64_t)-EBADF;

    switch (command)
    {
    case F_GETFD: return !!(get_current_task()->parent_group->fds[fd]->flags & O_CLOEXEC);
    case F_SETFD: return get_current_task()->parent_group->fds[fd]->flags |= O_CLOEXEC;
    case F_DUPFD_CLOEXEC: {
        uint64_t newfd                         = sys_dup(fd);
        get_current_task()->parent_group->fds[newfd]->flags |= O_CLOEXEC;
        return newfd;
    }
    case F_DUPFD: return sys_dup(fd);
    case F_GETFL: return get_current_task()->parent_group->fds[fd]->flags;
    case F_SETFL: {
        uint32_t valid_flags                = O_APPEND | O_DIRECT | O_NOATIME | O_NONBLOCK;
        get_current_task()->parent_group->fds[fd]->flags &= ~valid_flags;
        get_current_task()->parent_group->fds[fd]->flags |= arg & valid_flags;
        return 0;
    }
    }

    return (uint64_t)-ENOSYS;
}

uint64_t sys_stat(const char *fd, struct stat *buf)
{
    vfs_node_t node = vfs_open(fd);
    if (!node) { return (uint64_t)-ENOENT; }

    buf->st_dev   = 0;
    buf->st_ino   = node->inode;
    buf->st_nlink = 1;
    buf->st_mode  = node->mode | ((node->type & file_symlink) ? S_IFLNK : (node->type & file_dir ? S_IFDIR : S_IFREG));
    buf->st_uid   = 0;
    buf->st_uid   = 0;
    buf->st_gid   = 0;
    if (node->type & file_stream) { buf->st_rdev = (4 << 8) | 1; }
    else if (node->type & file_fbdev) { buf->st_rdev = (29 << 8) | 0; }
    else if (node->type & file_keyboard) { buf->st_rdev = (13 << 8) | 0; }
    else if (node->type & file_mouse) { buf->st_rdev = (13 << 8) | 1; }
    else { buf->st_rdev = 0; }
    buf->st_blksize = node->blksz;
    buf->st_size    = node->size;
    buf->st_blocks  = (buf->st_size + buf->st_blksize - 1) / buf->st_blksize;

    vfs_close(node);

    return 0;
}

uint64_t sys_fstat(uint64_t fd, struct stat *buf)
{
    if (fd >= 256 || get_current_task()->parent_group->fds[fd] == NULL) { return (uint64_t)-EBADF; }

    buf->st_dev   = 0;
    buf->st_ino   = get_current_task()->parent_group->fds[fd]->inode;
    buf->st_nlink = 1;
    buf->st_mode =
        get_current_task()->parent_group->fds[fd]->mode | ((get_current_task()->parent_group->fds[fd]->type & file_symlink)
                                                 ? S_IFLNK
                                                 : (get_current_task()->parent_group->fds[fd]->type & file_dir ? S_IFDIR : S_IFREG));
    buf->st_uid = 0;
    buf->st_gid = 0;
    if (get_current_task()->parent_group->fds[fd]->type & file_stream) { buf->st_rdev = (4 << 8) | 1; }
    else if (get_current_task()->parent_group->fds[fd]->type & file_fbdev) { buf->st_rdev = (29 << 8) | 0; }
    else if (get_current_task()->parent_group->fds[fd]->type & file_keyboard) { buf->st_rdev = (13 << 8) | 0; }
    else if (get_current_task()->parent_group->fds[fd]->type & file_mouse) { buf->st_rdev = (13 << 8) | 1; }
    else { buf->st_rdev = 0; }
    buf->st_blksize = get_current_task()->parent_group->fds[fd]->blksz;
    buf->st_size    = get_current_task()->parent_group->fds[fd]->size;
    buf->st_blocks  = (buf->st_size + buf->st_blksize - 1) / buf->st_blksize;

    return 0;
}

uint64_t sys_newfstatat(uint64_t dirfd, const char *pathname, struct stat *buf, uint64_t flags)
{
    char *resolved = at_resolve_pathname(dirfd, (char *)pathname);

    uint64_t ret = sys_stat(pathname, buf);

    free(resolved);

    return ret;
}

uint64_t sys_statx(uint64_t dirfd, const char *pathname, uint64_t flags, uint64_t mask, struct statx *buff)
{
    struct stat simple;
    memset(&simple, 0, sizeof(struct stat));
    uint64_t ret = sys_newfstatat(dirfd, pathname, &simple, flags);
    if ((int64_t)ret < 0) return ret;

    buff->stx_mask            = mask;
    buff->stx_blksize         = simple.st_blksize;
    buff->stx_attributes      = 0;
    buff->stx_nlink           = simple.st_nlink;
    buff->stx_uid             = simple.st_uid;
    buff->stx_gid             = simple.st_gid;
    buff->stx_mode            = simple.st_mode;
    buff->stx_ino             = simple.st_ino;
    buff->stx_size            = simple.st_size;
    buff->stx_blocks          = simple.st_blocks;
    buff->stx_attributes_mask = 0;

    buff->stx_atime.tv_sec  = simple.st_atim.tv_sec;
    buff->stx_atime.tv_nsec = simple.st_atim.tv_nsec;

    buff->stx_btime.tv_sec  = simple.st_ctim.tv_sec;
    buff->stx_btime.tv_nsec = simple.st_ctim.tv_nsec;

    buff->stx_ctime.tv_sec  = simple.st_ctim.tv_sec;
    buff->stx_ctime.tv_nsec = simple.st_ctim.tv_nsec;

    buff->stx_mtime.tv_sec  = simple.st_mtim.tv_sec;
    buff->stx_mtime.tv_nsec = simple.st_mtim.tv_nsec;

    // todo: special devices

    return 0;
}
// i hate this obsolete system call and do not plan on making it efficient
static inline bool select_bitmap(uint8_t *map, int index)
{
    int div = index / 8;
    int mod = index % 8;
    return map[div] & (1 << mod);
}

static inline void select_bitmap_set(uint8_t *map, int index)
{
    int div   = index / 8;
    int mod   = index % 8;
    map[div] |= 1 << mod;
}

size_t sys_access(char *filename, int mode)
{
    (void)mode;
    struct stat buf;
    return sys_stat(filename, &buf);
}

uint64_t sys_faccessat(uint64_t dirfd, const char *pathname, uint64_t mode)
{
    if (pathname[0] == '\0')
    { // by fd
        return 0;
    }

    char *resolved = at_resolve_pathname(dirfd, (char *)pathname);
    if (resolved == NULL) return (uint64_t)-ENOENT;

    size_t ret = sys_access(resolved, mode);

    free(resolved);

    return ret;
}

uint64_t sys_faccessat2(uint64_t dirfd, const char *pathname, uint64_t mode, uint64_t flags)
{
    if (pathname[0] == '\0')
    { // by fd
        return 0;
    }

    char *resolved = at_resolve_pathname(dirfd, (char *)pathname);
    if (resolved == NULL) return (uint64_t)-ENOENT;

    size_t ret = sys_access(resolved, mode);

    free(resolved);

    return ret;
}

uint64_t sys_link(const char *old, const char *nw)
{
    vfs_node_t old_node = vfs_open(old);
    if (!old_node) { return (uint64_t)-ENOENT; }

    int ret = 0;
    if (old_node->type & file_dir)
    {
        ret = vfs_mkdir(nw);
        if (ret < 0) { return (uint64_t)-EEXIST; }
    }
    else
    {
        ret = vfs_mkfile(nw);
        if (ret < 0) { return (uint64_t)-EEXIST; }
    }

    return 0;
}

uint64_t sys_readlink(char *path, char *buf, uint64_t size)
{
    if (path == NULL || buf == NULL || size == 0) { return (uint64_t)-EINVAL; }

    vfs_node_t node = vfs_open_at(get_current_task()->parent_group->cwd, path, true);
    if (node == NULL) { return (uint64_t)-ENOENT; }

    ssize_t result = vfs_readlink(node, buf, (size_t)size);
    vfs_close(node);

    if (result < 0)
    {
        switch (-result)
        {
        case 1: return (uint64_t)-ENOLINK;
        default: return (uint64_t)-EIO;
        }
    }

    return (uint64_t)result;
}

uint64_t sys_rmdir(const char *name)
{
    vfs_node_t node = vfs_open(name);
    if (!node) return -ENOENT;
    if (!(node->type & file_dir)) return -EBADF;

    uint64_t ret = vfs_delete(node);

    return ret;
}

uint64_t sys_unlink(const char *name)
{
    vfs_node_t node = vfs_open(name);
    if (!node) return -ENOENT;

    uint64_t ret = vfs_delete(node);

    return ret;
}

uint64_t sys_unlinkat(uint64_t dirfd, const char *name, uint64_t flags)
{
    char *path = at_resolve_pathname(dirfd, (char *)name);
    if (!path) return -ENOENT;

    uint64_t ret = sys_unlink((const char *)path);

    free(path);

    return ret;
}

static vfs_node_t eventfdfs_root = NULL;
static int        eventfdfs_id   = 0;

uint64_t sys_eventfd2(uint64_t initial_val, uint64_t flags)
{
    // 参数校验
    if (flags & ~(EFD_CLOEXEC | EFD_NONBLOCK | EFD_SEMAPHORE)) return (uint64_t)-EINVAL;

    // 分配文件描述符
    int fd = -1;
    for (int i = 3; i < 256; i++)
    {
        if (!get_current_task()->parent_group->fds[i])
        {
            fd = i;
            break;
        }
    }

    if (fd == -1) { return (uint64_t)-EMFILE; }

    // 分配eventfd结构体
    eventfd_t *efd = (eventfd_t *)malloc(sizeof(eventfd_t));
    if (!efd) return (uint64_t)-ENOMEM;

    efd->count = initial_val;
    efd->flags = flags;

    // 创建VFS节点
    char buf[256];
    sprintf(buf, "eventfd%d", eventfd_id++);
    vfs_node_t node = vfs_node_alloc(eventfdfs_root, buf);
    node->mode      = 0700;
    node->type      = file_stream;
    node->fsid      = eventfdfs_id;
    node->handle    = efd;

    get_current_task()->parent_group->fds[fd] = node;

    return fd;
}

static int signalfd_poll(void *file, size_t event)
{
    return -EOPNOTSUPP;
}

// 实现读写操作
static ssize_t eventfd_read(vfs_node_t node, void *buf, size_t offset, size_t len)
{
    eventfd_t *efd = (eventfd_t *)node->handle;
    uint64_t   value;

    while (efd->count == 0)
    {
        if (efd->flags & EFD_NONBLOCK) return -EAGAIN;

        open_interrupt;

        __asm__ volatile("pause");
    }

    value = (efd->flags & EFD_SEMAPHORE) ? 1 : efd->count;
    memcpy(buf, &value, sizeof(uint64_t));

    efd->count -= value;
    return sizeof(uint64_t);
}

static ssize_t eventfd_write(vfs_node_t node, const void *buf, size_t offset, size_t len)
{
    eventfd_t *efd = (eventfd_t *)node->handle;
    uint64_t   value;
    memcpy(&value, buf, sizeof(uint64_t));

    if (__UINT64_MAX__ - efd->count < value) return -EINVAL;

    efd->count += value;

    return sizeof(uint64_t);
}

static int eventfd_poll(void *file, size_t event)
{
    return -EOPNOTSUPP;
}

static int epoll_poll(void *file, size_t event)
{
    return -EOPNOTSUPP;
}
uint64_t sys_rename(const char *old, const char *nw)
{
    vfs_node_t node = vfs_open(old);
    int        ret  = vfs_rename(node, nw);
    if (ret < 0) return -ENOENT;

    return 0;
}

uint64_t sys_fchdir(uint64_t fd)
{
    if (fd >= 256 || !get_current_task()->parent_group->fds[fd]) return -EBADF;

    vfs_node_t node = get_current_task()->parent_group->fds[fd];
    if (node->type != file_dir) return -ENOTDIR;

    get_current_task()->parent_group->cwd = node;

    return 0;
}

static int dummy()
{
    return -ENOSYS;
}

vfs_node_t eventfd_dup(vfs_node_t node)
{
    if (!node || !node->handle) return NULL;

    eventfd_t *efd = (eventfd_t *)node->handle;

    // 创建新的vfs节点
    vfs_node_t new_node = vfs_node_alloc(node->parent, node->name);
    if (!new_node) return NULL;

    // 复制节点属性
    memcpy(new_node, node, sizeof(struct vfs_node));

    // 创建新的eventfd上下文
    eventfd_t *new_efd = (eventfd_t *)malloc(sizeof(eventfd_t));
    if (!new_efd)
    {
        vfs_free(new_node);
        return NULL;
    }

    // 复制上下文
    memcpy(new_efd, efd, sizeof(eventfd_t));
    new_node->handle = new_efd;

    return new_node;
}

static struct vfs_callback epoll_callbacks = {
    .mount   = (vfs_mount_t)dummy,
    .unmount = (vfs_unmount_t)dummy,
    .open    = (vfs_open_t)dummy,
    .close   = (vfs_close_t)dummy,
    .read    = (vfs_read_t)dummy,
    .write   = (vfs_write_t)dummy,
    .mkdir   = (vfs_mk_t)dummy,
    .mkfile  = (vfs_mk_t)dummy,
    .del     = (vfs_del_t)dummy,
    .rename  = (vfs_rename_t)dummy,
    .stat    = (vfs_stat_t)dummy,
    .ioctl   = (vfs_ioctl_t)dummy,
    .poll    = epoll_poll,
    .dup     = (vfs_dup_t)dummy,
};

static struct vfs_callback eventfd_callbacks = {
    .mount   = (vfs_mount_t)dummy,
    .unmount = (vfs_unmount_t)dummy,
    .open    = (vfs_open_t)dummy,
    .close   = (vfs_close_t)dummy,
    .read    = (vfs_read_t)eventfd_read,
    .write   = (vfs_write_t)eventfd_write,
    .mkdir   = (vfs_mk_t)dummy,
    .mkfile  = (vfs_mk_t)dummy,
    .del     = (vfs_del_t)dummy,
    .rename  = (vfs_rename_t)dummy,
    .stat    = (vfs_stat_t)dummy,
    .ioctl   = (vfs_ioctl_t)dummy,
    .poll    = eventfd_poll,
    .dup     = (vfs_dup_t)eventfd_dup,
};

static struct vfs_callback signalfd_callbacks = {
    .mount   = (vfs_mount_t)dummy,
    .unmount = (vfs_unmount_t)dummy,
    .open    = (vfs_open_t)dummy,
    .close   = (vfs_close_t)dummy,
    .read    = (vfs_read_t)dummy,
    .write   = (vfs_write_t)dummy,
    .mkdir   = (vfs_mk_t)dummy,
    .mkfile  = (vfs_mk_t)dummy,
    .del     = (vfs_del_t)dummy,
    .rename  = (vfs_rename_t)dummy,
    .stat    = (vfs_stat_t)dummy,
    .ioctl   = (vfs_ioctl_t)dummy,
    .poll    = signalfd_poll,
    .dup     = (vfs_dup_t)eventfd_dup,
};

static struct vfs_callback timerfd_callbacks = {
    .mount   = (vfs_mount_t)dummy,
    .unmount = (vfs_unmount_t)dummy,
    .open    = (vfs_open_t)dummy,
    .close   = (vfs_close_t)dummy,
    .read    = (vfs_read_t)dummy,
    .write   = (vfs_write_t)dummy,
    .mkdir   = (vfs_mk_t)dummy,
    .mkfile  = (vfs_mk_t)dummy,
    .del     = (vfs_del_t)dummy,
    .rename  = (vfs_rename_t)dummy,
    .stat    = (vfs_stat_t)dummy,
    .ioctl   = (vfs_ioctl_t)dummy,
    .poll    = (vfs_poll_t)dummy,
    .dup     = (vfs_dup_t)dummy,
};

void fs_syscall_init()
{
    // epollfs_id = vfs_regist("epollfs", &epoll_callbacks);
    // epollfs_root = vfs_node_alloc(rootdir, "epoll");
    // epollfs_root->type = file_dir;
    // epollfs_root->mode = 0644;
    // epollfs_root->fsid = epollfs_id;

    eventfdfs_id         = vfs_regist("eventfdfs", &eventfd_callbacks);
    eventfdfs_root       = vfs_node_alloc(rootdir, "event");
    eventfdfs_root->type = file_dir;
    eventfdfs_root->mode = 0644;
    eventfdfs_root->fsid = eventfdfs_id;

    //长大了再学习吧
    // signalfdfs_id = vfs_regist("signalfdfs", &signalfd_callbacks);
    // signalfdfs_root = vfs_node_alloc(rootdir, "signal");
    // signalfdfs_root->type = file_dir;
    // signalfdfs_root->mode = 0644;
    // signalfdfs_root->fsid = signalfdfs_id;

    // timerfdfs_id = vfs_regist("timerfd", &timerfd_callbacks);
    // timerfdfs_root = vfs_node_alloc(rootdir, "timer");
    // timerfdfs_root->type = file_dir;
    // timerfdfs_root->mode = 0644;
    // timerfdfs_root->fsid = timerfdfs_id;
}

uint64_t sys_flock(int fd, uint64_t operation)
{
    if (fd < 0 || fd >= 256 || !get_current_task()->parent_group->fds[fd]) return -EBADF;

    vfs_node_t    node = get_current_task()->parent_group->fds[fd];
    struct flock *lock = &node->lock;
    uint64_t      pid  = get_current_task()->parent_group->pid;

    // 提前检查参数有效性
    switch (operation & ~LOCK_NB)
    {
    case LOCK_SH:
    case LOCK_EX:
    case LOCK_UN: break;
    default: return -EINVAL;
    }

    // 非阻塞模式下立即检查冲突
    if (operation & LOCK_NB)
    {
        if ((operation & LOCK_SH) && lock->l_type == F_WRLCK) return -EWOULDBLOCK;
        if ((operation & LOCK_EX) && lock->l_type != F_UNLCK) return -EWOULDBLOCK;
    }

    // 实际加锁逻辑
    switch (operation & ~LOCK_NB)
    {
    case LOCK_SH:
    case LOCK_EX:
        while (lock->l_type != F_UNLCK && lock->l_pid != pid)
        {
            if (operation & LOCK_NB) return -EWOULDBLOCK;

            while (lock->lock)
            {
                open_interrupt;

                __asm__ volatile("pause");
            }

            close_interrupt;
        }
        lock->l_type = (operation & LOCK_EX) ? F_WRLCK : F_RDLCK;
        lock->l_pid  = pid;
        break;

    case LOCK_UN:
        if (lock->l_pid != pid) return -EACCES;
        lock->l_type = F_UNLCK;
        lock->l_pid  = 0;
        lock->lock   = 1;
        break;
    }

    return 0;
}

uint64_t sys_mkdir(const char *name, uint64_t mode)
{
    int ret = vfs_mkdir(name);
    if (ret < 0) { return (uint64_t)-EEXIST; }
    return 0;
}
