#include <syscall/syscall.h>
#include <proto.hpp>
#include <fs/vfs/vfs.h>
#include <ioctl.h>
#include <cpu/fsgsbase.h>
#include <task/poll.h>
#include <mm/lazyalloc.h>
#include <mm/frame.h>
#include <mm/uaccess.h>
#include "../build_settings.h"
#include <net/socket.h>
#include <pipe.h>
#include <rtc.h>
#include <tty.h>
#include <task/ipc.h>
#include <math.hpp>
#include <user/user.h>

extern lock_queue *pcb_group_queue;

static constexpr size_t USER_IO_BOUNCE_BYTES = 0x10000UL;
static constexpr size_t USER_PATH_MAX        = 4096UL;
static constexpr size_t USER_IOV_MAX         = 1024UL;
static constexpr size_t USER_POLL_MAX        = FD_SETSIZE;

static bool checked_add_size(size_t a, size_t b, size_t *out)
{
    if (out == NULL) return false;
    if (a > (size_t)-1 - b) return false;
    *out = a + b;
    return true;
}

static bool checked_mul_size(size_t a, size_t b, size_t *out)
{
    if (out == NULL) return false;
    if (a != 0 && b > (size_t)-1 / a) return false;
    *out = a * b;
    return true;
}

static bool align_up_u64(uint64_t value, uint64_t align, uint64_t *out)
{
    if (out == NULL || align == 0) return false;
    uint64_t addend = align - 1;
    if (value > UINT64_MAX - addend) return false;
    *out = (value + addend) & ~(align - 1);
    return true;
}

static size_t bounded_strlen(const char *str, size_t max_len)
{
    if (str == NULL) return 0;
    size_t len = 0;
    while (len < max_len && str[len] != '\0')
        len++;
    return len;
}

static bool user_bitmap_bytes(uint64_t nfds, size_t *out)
{
    if (out == NULL) return false;
    if (nfds > FD_SETSIZE) return false;
    *out = ((size_t)nfds + 7) / 8;
    return true;
}

static uint8_t dirent_type_from_node(vfs_node_t node)
{
    if (node->type & file_symlink) return DT_LNK;
    if (node->type & file_none) return DT_REG;
    if (node->type & file_block) return DT_BLK;
    if (node->type & file_stream) return DT_CHR;
    if (node->type & file_socket) return DT_SOCK;
    if (node->type & file_dir) return DT_DIR;
    return DT_UNKNOWN;
}

static page_directory_t *current_user_pagedir()
{
    tcb_t task = get_current_task();
    if (task == NULL || task->parent_group == NULL) return NULL;
    return task->parent_group->pagedir;
}

static int copy_string_from_user(char **out, const char *src, size_t max_len)
{
    if (out == NULL) return -EINVAL;
    *out = NULL;
    if (src == NULL || max_len == 0) return -EINVAL;

    page_directory_t *pagedir = current_user_pagedir();
    if (pagedir == NULL) return -EFAULT;

    char *tmp = (char *)malloc(max_len);
    if (tmp == NULL) return -ENOMEM;

    for (size_t i = 0; i < max_len; i++)
    {
        char c = 0;
        if (!copy_from_user_pagedir(pagedir, &c, src + i, sizeof(c)))
        {
            free(tmp);
            return -EFAULT;
        }
        tmp[i] = c;
        if (c == '\0')
        {
            *out = tmp;
            return 0;
        }
    }

    free(tmp);
    return -ENAMETOOLONG;
}

static uint64_t fill_stat_from_node(vfs_node_t node, struct stat *out)
{
    if (node == NULL || out == NULL) return SYSCALL_FAULT_(EINVAL);
    memset(out, 0, sizeof(*out));
    out->st_gid     = (int)node->group;
    out->st_uid     = (int)node->owner;
    out->st_ino     = node->inode;
    out->st_size    = node->size == (uint64_t)-1 ? 0 : (long long int)node->size;
    out->st_mode    = node->mode | (node->type == file_symlink  ? S_IFLNK
                                   : node->type == file_dir    ? S_IFDIR
                                   : node->type == file_block  ? S_IFBLK
                                   : node->type == file_socket ? S_IFSOCK
                                   : node->type == file_none   ? S_IFREG
                                   : node->type == file_stream ? S_IFCHR
                                                               : S_IFREG);
    out->st_nlink   = 1;
    out->st_dev     = (long)node->dev;
    out->st_rdev    = (long)node->rdev;
    out->st_blksize = PAGE_SIZE;
    out->st_blocks  = (node->size == (uint64_t)-1) ? 0 : (node->size + PAGE_SIZE - 1) / PAGE_SIZE;
    out->st_ctim = out->st_atim = out->st_mtim =
        (struct timespec){.tv_sec = node->createtime / 1000000000ULL, .tv_nsec = node->createtime % 1000000000ULL};
    return 0;
}

static uint32_t current_uid()
{
    return user_uid(task_effective_user());
}

static uint32_t current_gid()
{
    return user_gid(task_effective_user());
}

static void stamp_node_owner(vfs_node_t node)
{
    if (node == NULL) return;
    node->owner = current_uid();
    node->group = current_gid();
}

static uint64_t stat_kernel_path(const char *path, struct stat *out)
{
    if (path == NULL || out == NULL) return SYSCALL_FAULT_(EINVAL);
    vfs_node_t node = vfs_open(path);
    if (node == NULL) return SYSCALL_FAULT_(ENOENT);
    uint64_t ret = fill_stat_from_node(node, out);
    vfs_close(node);
    return ret;
}

static bool busybox_debug_current_linux_abi()
{
    tcb_t task = get_current_task();
    return task != NULL && task->parent_group != NULL && task->parent_group->linux_abi;
}

static uint64_t copy_stat_to_user(struct stat *user_stat, const struct stat *kstat)
{
    if (user_stat == NULL || kstat == NULL) return SYSCALL_FAULT_(EINVAL);
    page_directory_t *pagedir = current_user_pagedir();
    if (!copy_to_user_pagedir(pagedir, user_stat, kstat, sizeof(*kstat))) return SYSCALL_FAULT_(EFAULT);
    return 0;
}

static int resolve_user_path_at(int dirfd, char *pathname, char **resolved)
{
    if (resolved == NULL) return -EINVAL;
    *resolved = NULL;

    char *kpath = NULL;
    int ret = copy_string_from_user(&kpath, pathname, USER_PATH_MAX);
    if (ret < 0) return ret;

    char *path = at_resolve_pathname(dirfd, kpath);
    free(kpath);
    if (path == NULL) return -ENOENT;

    *resolved = path;
    return 0;
}

static int64_t write_from_user_buffer(fd_file_handle *handle, const uint8_t *buffer, size_t size, bool socket_mode)
{
    if (handle == NULL || buffer == NULL) return -EINVAL;
    if (size == 0) return 0;

    pcb_t process = get_current_task()->parent_group;
    size_t bounce_bytes = MIN(size, USER_IO_BOUNCE_BYTES);
    uint8_t *bounce = (uint8_t *)malloc(bounce_bytes);
    if (bounce == NULL) return -ENOMEM;

    size_t total = 0;
    while (total < size)
    {
        size_t chunk = MIN(size - total, bounce_bytes);
        if (!copy_from_user_pagedir(process->pagedir, bounce, buffer + total, chunk))
        {
            free(bounce);
            return total > 0 ? (int64_t)total : -EFAULT;
        }

        if (socket_mode)
        {
            ssize_t wrote = socket_write(handle->node, bounce, chunk);
            if (wrote < 0)
            {
                free(bounce);
                return total > 0 ? (int64_t)total : wrote;
            }
            total += (size_t)wrote;
            if ((size_t)wrote < chunk) break;
            continue;
        }

        size_t write_offset = (handle->node->type & file_pipe) ? 0 : handle->offset;
        size_t wrote        = vfs_write(handle->node, bounce, write_offset, chunk);
        if (wrote == (size_t)VFS_STATUS_FAILED)
        {
            free(bounce);
            return total > 0 ? (int64_t)total : -EIO;
        }
        if (!(handle->node->type & file_pipe) && handle->node->size != (uint64_t)-1) handle->offset += wrote;
        total += wrote;
        if (wrote < chunk) break;
    }

    free(bounce);
    if (!socket_mode) vfs_update(handle->node);
    return (int64_t)total;
}

static size_t ioctl_arg_size(int request)
{
    switch (request)
    {
    case TCGETS:
    case TCSETS:
    case TCSETSW:
    case TCSETSF:
        return sizeof(struct termios);
    case TIOCGWINSZ:
    case TIOCSWINSZ:
        return sizeof(struct winsize);
    case TIOCGPTN:
    case TIOCSPTLCK:
    case TIOCGPTLCK:
    case TIOCGPGRP:
    case TIOCSPGRP:
    case KDGETMODE:
    case KDSETMODE:
    case KDGKBMODE:
    case KDSKBMODE:
    case VT_OPENQRY:
        return sizeof(int);
    case VT_GETMODE:
    case VT_SETMODE:
        return sizeof(struct vt_mode);
    default:
        return 0;
    }
}

static bool ioctl_writes_user(int request)
{
    switch (request)
    {
    case TCGETS:
    case TIOCGWINSZ:
    case TIOCGPTN:
    case TIOCGPTLCK:
    case TIOCGPGRP:
    case KDGETMODE:
    case KDGKBMODE:
    case VT_OPENQRY:
    case VT_GETMODE:
        return true;
    default:
        return false;
    }
}

static bool ioctl_reads_user(int request)
{
    switch (request)
    {
    case TCSETS:
    case TCSETSW:
    case TCSETSF:
    case TIOCSWINSZ:
    case TIOCSPTLCK:
    case TIOCSPGRP:
    case KDSETMODE:
    case KDSKBMODE:
    case VT_SETMODE:
        return true;
    default:
        return false;
    }
}

static int64_t read_to_user_buffer(fd_file_handle *handle, uint8_t *buffer, size_t size, bool socket_mode,
                                   size_t offset, bool update_offset)
{
    if (handle == NULL || buffer == NULL) return -EINVAL;
    if (size == 0) return 0;

    page_directory_t *pagedir = current_user_pagedir();
    size_t bounce_bytes = MIN(size, USER_IO_BOUNCE_BYTES);
    uint8_t *bounce = (uint8_t *)malloc(bounce_bytes);
    if (bounce == NULL) return -ENOMEM;

    size_t total = 0;
    while (total < size)
    {
        size_t chunk = MIN(size - total, bounce_bytes);
        ssize_t read_now;
        if (socket_mode)
        {
            read_now = socket_read(handle->node, bounce, chunk);
            if (read_now < 0)
            {
                free(bounce);
                return total > 0 ? (int64_t)total : read_now;
            }
        }
        else
        {
            size_t got = vfs_read(handle->node, bounce, offset + total, chunk);
            if (got == (size_t)VFS_STATUS_FAILED)
            {
                free(bounce);
                return total > 0 ? (int64_t)total : -EIO;
            }
            read_now = (ssize_t)got;
        }

        if (read_now == 0) break;
        if (!copy_to_user_pagedir(pagedir, buffer + total, bounce, (size_t)read_now))
        {
            free(bounce);
            return total > 0 ? (int64_t)total : -EFAULT;
        }

        total += (size_t)read_now;
        if ((size_t)read_now < chunk) break;
    }

    free(bounce);
    if (update_offset && handle->node->size != (uint64_t)-1) handle->offset += total;
    return (int64_t)total;
}

static bool wait4_pid_matches(pcb_t child, long pid)
{
    if (child == NULL) return false;
    return pid == -1 ? true : (long)child->pid == pid;
}

static pcb_t wait4_find_child(pcb_t parent, long pid)
{
    if (parent == NULL || parent->child_pcb == NULL) return NULL;

    queue_foreach(parent->child_pcb, node)
    {
        pcb_t child = (pcb_t)node->data;
        if (wait4_pid_matches(child, pid)) return child;
    }

    return NULL;
}

static pcb_t wait4_find_zombie_child(pcb_t parent, long pid)
{
    if (parent == NULL || parent->child_pcb == NULL) return NULL;

    queue_foreach(parent->child_pcb, node)
    {
        pcb_t child = (pcb_t)node->data;
        if (child != NULL && child->status == ZOMBIE && wait4_pid_matches(child, pid)) return child;
    }

    return NULL;
}

static ipc_message_t wait4_pop_exit_message(pcb_t parent, long pid, bool wait)
{
    if (parent == NULL || parent->ipc_queue == NULL) return NULL;

    while (true)
    {
        size_t remaining = parent->ipc_queue->size;
        for (size_t i = 0; i < remaining; ++i)
        {
            ipc_message_t message = (ipc_message_t)queue_dequeue(parent->ipc_queue);
            if (message == NULL) break;

            if (message->type == IPC_MSG_TYPE_EPID && (pid == -1 || message->pid == pid)) return message;

            message->index = lock_queue_enqueue(parent->ipc_queue, message);
        }

        if (!wait) return NULL;

        __asm__ volatile("pause");
        scheduler_yield();
    }
}

static void wait4_discard_exit_message(pcb_t parent, int pid)
{
    ipc_message_t message = wait4_pop_exit_message(parent, pid, false);
    if (message != NULL) free(message);
}

static uint64_t wait4_reap_child(pcb_t parent, pcb_t child, int *status)
{
    if (parent == NULL || child == NULL) return SYSCALL_FAULT_(ECHILD);

    if (status != NULL)
    {
        int wait_status = (child->exit_code & 0xff) << 8;
        if (!copy_to_user_pagedir(parent->pagedir, status, &wait_status, sizeof(wait_status)))
        {
            return SYSCALL_FAULT_(EFAULT);
        }
    }

    wait4_discard_exit_message(parent, child->pid);
    kill_proc(child, child->exit_code, false);
    return (uint64_t)child->pid;
}

sys_(exit, int exit_code)
{
    tcb_t exit_thread = get_current_task();
    write_serial_fmt("sys_exit: Thread %s exit with code %d.\n", exit_thread->name, exit_code);
    kill_thread(exit_thread);
    open_interrupt;
    cpu_hlt;
    return 0;
}

static uint64_t open_kernel_path(char *path0, uint64_t flags, uint64_t mode)
{
    char *normalized_path = strdup(path0);
    if (normalized_path == NULL) return SYSCALL_FAULT_(ENOMEM);

    vfs_node_t node = vfs_open(path0);
    if (node == NULL)
    {
        if (flags & O_CREAT)
        {
            if (mode & O_DIRECTORY) { vfs_mkdir(normalized_path); }
            else
                vfs_mkfile(normalized_path);
            node = vfs_open(normalized_path);
            if (node == NULL)
                goto err;
            else {
                stamp_node_owner(node);
                if ((mode & 07777) != 0) node->mode = (uint16_t)(mode & 07777);
                goto next;
            }
        }
        else
        err:
            free(normalized_path);
        return SYSCALL_FAULT_(ENOENT);
    }
next:
    vfs_update(node);
    if (!(node->type & (file_dir | file_symlink)) && node->handle == NULL)
    {
        vfs_close(node);
        free(normalized_path);
        return SYSCALL_FAULT_(ENODEV);
    }

    fd_file_handle *fd_handle = (fd_file_handle *)calloc(1, sizeof(fd_file_handle));
    // not_null_assets(fd_handle, "sys_open: null alloc fd");
    fd_handle->offset = flags & O_APPEND ? node->size : 0;
    fd_handle->node   = node;
    fd_handle->flags  = flags;
    fd_handle->node->flags = flags;
    int index         = (int)lock_queue_enqueue(get_current_task()->parent_group->file_open, fd_handle);
    fd_handle->fd     = index;
    if (index == -1)
    {
        write_serial_fmt("sys_open: open %s failed.\n", normalized_path);
        vfs_close(node);
        free(fd_handle);
        free(normalized_path);
        return SYSCALL_FAULT_(ENOENT);
    }
    free(normalized_path);
    return index;
}

sys_(open, char *path0, uint64_t flags, uint64_t mode)
{
    if (unlikely(path0 == NULL)) return SYSCALL_FAULT_(EINVAL);

    char *kpath = NULL;
    int ret = copy_string_from_user(&kpath, path0, USER_PATH_MAX);
    if (ret < 0) return SYSCALL_FAULT_((int)-ret);

    uint64_t open_ret = open_kernel_path(kpath, flags, mode);
    free(kpath);
    return open_ret;
}

sys_(close, int fd)
{
    if (unlikely(fd < 0)) return SYSCALL_FAULT_(EINVAL);
    fd_file_handle *handle = (fd_file_handle *)queue_remove_at(get_current_task()->parent_group->file_open, fd);
    if (handle == NULL) return SYSCALL_FAULT_(EBADF);
    vfs_close(handle->node);
    free(handle);
    return 0;
}

sys_(write, int fd, uint8_t *buffer, size_t size)
{
    pcb_t process = get_current_task()->parent_group;
    if (process != NULL && process->linux_abi)
    {
        // write_serial_fmt("[busybox-debug] sys_write fd=%d size=%llu\n", fd, (unsigned long long)size);
    }
    if (unlikely(fd < 0 || buffer == NULL)) return SYSCALL_FAULT_(EINVAL);
    if (unlikely(size == 0)) return 0;
    fd_file_handle *handle = (fd_file_handle *)queue_get(get_current_task()->parent_group->file_open, fd);
    if (!handle) return SYSCALL_FAULT_(EBADF);
    if (handle->node->type & file_socket)
    {
        int64_t ret = write_from_user_buffer(handle, buffer, size, true);
        if (ret < 0) return SYSCALL_FAULT_((int)-ret);
        return (uint64_t)ret;
    }
    int64_t ret = write_from_user_buffer(handle, buffer, size, false);
    if (ret < 0) return SYSCALL_FAULT_((int)-ret);
    return (uint64_t)ret;
}

sys_(read, int fd, uint8_t *buffer, size_t size)
{
    pcb_t process = get_current_task()->parent_group;
    if (process != NULL && process->linux_abi)
    {
        // write_serial_fmt("[busybox-debug] sys_read fd=%d size=%llu\n", fd, (unsigned long long)size);
    }
    if (unlikely(fd < 0 || buffer == NULL)) return SYSCALL_FAULT_(EINVAL);
    if (unlikely(size == 0)) return 0;
    fd_file_handle *handle = (fd_file_handle *)queue_get(get_current_task()->parent_group->file_open, fd);
    if (!handle) return SYSCALL_FAULT_(EBADF);
    if (handle->node->type & file_socket)
    {
        int64_t ret = read_to_user_buffer(handle, buffer, size, true, 0, false);
        return ret < 0 ? SYSCALL_FAULT_((int)-ret) : (uint64_t)ret;
    }
    if (handle->node->type & file_pipe)
    {
        vfs_update(handle->node);
        if (handle->node->size == 0 && handle->flags & O_NONBLOCK) return SYSCALL_FAULT_(EWOULDBLOCK);
        int64_t ret = read_to_user_buffer(handle, buffer, size, false, 0, false);
        return ret < 0 ? SYSCALL_FAULT_((int)-ret) : (uint64_t)ret;
    }
    if (handle->node->type & file_pipe && handle->node->size == 0 && handle->flags & O_NONBLOCK)
    {
        return SYSCALL_FAULT_(EWOULDBLOCK);
    }
    if (handle->node->size != (uint64_t)-1)
    {
        if (handle->offset >= handle->node->size)
        {
            if (handle->node->type & file_pipe) { goto pipe; }
            return 0;
        }
    }
read:;
    {
        int64_t read_ret = read_to_user_buffer(handle, buffer, size, false, handle->offset, true);
        return read_ret < 0 ? SYSCALL_FAULT_((int)-read_ret) : (uint64_t)read_ret;
    }
pipe:;
    while (handle->offset >= handle->node->size)
    {
        vfs_update(handle->node);
        scheduler_yield();
    }
    goto read;
}

//要IPC

// sys_(waitpid, int pid, int *status, uint64_t options)

// {
//     pcb_t wait_p;
//     if (get_current_task()->parent_group->child_pcb->size == 0) return SYSCALL_FAULT_(ECHILD);
//     if (pid == -1) goto wait;
//     wait_p = found_pcb(pid);
//     if (wait_p == NULL) return SYSCALL_FAULT_(ECHILD);
// wait:
//     int ret_pid = 0;
//     int   status0 = waitpid(pid, &ret_pid);
//     if (status) *status = status0;
//     return ret_pid;
// }

sys_(wait4, unsigned long pid_value, int *status, int options, void *rusage)
{
    static constexpr int LINUX_WNOHANG    = 1;
    static constexpr int LINUX_WUNTRACED  = 2;
    static constexpr int LINUX_WCONTINUED = 8;

    pcb_t parent = get_current_task()->parent_group;
    long  pid    = (long)(int32_t)(uint32_t)pid_value;
    (void)rusage;
    if (parent == NULL || parent->child_pcb == NULL) return SYSCALL_FAULT_(ECHILD);
    if ((options & ~(LINUX_WNOHANG | LINUX_WUNTRACED | LINUX_WCONTINUED)) != 0) return SYSCALL_FAULT_(EINVAL);
    if (pid < -1 || pid == 0) return SYSCALL_FAULT_(EINVAL);
    if (wait4_find_child(parent, pid) == NULL) return SYSCALL_FAULT_(ECHILD);

    pcb_t zombie = wait4_find_zombie_child(parent, pid);
    if (zombie != NULL) return wait4_reap_child(parent, zombie, status);
    if (options & LINUX_WNOHANG) return 0;

    while (true)
    {
        zombie = wait4_find_zombie_child(parent, pid);
        if (zombie != NULL) return wait4_reap_child(parent, zombie, status);

        if (wait4_find_child(parent, pid) == NULL) return SYSCALL_FAULT_(ECHILD);

        ipc_message_t message = wait4_pop_exit_message(parent, pid, false);
        if (message == NULL)
        {
            __asm__ volatile("pause");
            scheduler_yield();
            continue;
        }

        int exit_code = (int)message->data[0] | ((int)message->data[1] << 8) | ((int)message->data[2] << 16) |
                        ((int)message->data[3] << 24);
        int child_pid = message->pid;
        free(message);

        pcb_t child = wait4_find_child(parent, child_pid);
        if (child == NULL)
        {
            if (wait4_find_child(parent, pid) == NULL) return SYSCALL_FAULT_(ECHILD);
            continue;
        }

        child->exit_code = exit_code;
        child->status    = ZOMBIE;
        return wait4_reap_child(parent, child, status);
    }
}

sys_(sigret)
{
    return signal_return(regs);
}

sys_(getpid)
{
    if (unlikely(arg0 == UINT64_MAX || arg1 == UINT64_MAX || arg2 == UINT64_MAX || arg3 == UINT64_MAX ||
                 arg4 == UINT64_MAX))
        return 1;
    return get_current_task()->parent_group->pid;
}

sys_(socket, int domain, int type, int protocol)
{
    int ret = socket_open(domain, type, protocol);
    if (ret < 0) return SYSCALL_FAULT_(-ret);
    return ret;
}

sys_(connect, int fd, const struct sockaddr *addr, socklen_t addrlen)
{
    if (addr == NULL) return SYSCALL_FAULT_(EINVAL);
    if (addrlen < sizeof(struct sockaddr) || addrlen > sizeof(struct sockaddr_in6)) return SYSCALL_FAULT_(EINVAL);
    struct sockaddr_in6 kaddr;
    memset(&kaddr, 0, sizeof(kaddr));
    if (!copy_from_user_pagedir(current_user_pagedir(), &kaddr, addr, addrlen)) return SYSCALL_FAULT_(EFAULT);
    int ret = socket_connect(fd, (const struct sockaddr *)&kaddr, addrlen);
    if (ret < 0) return SYSCALL_FAULT_(-ret);
    return ret;
}

sys_(accept, int fd, struct sockaddr *addr, socklen_t *addrlen)
{
    socklen_t kaddrlen = sizeof(struct sockaddr_in6);
    if (addrlen != NULL && !copy_from_user_pagedir(current_user_pagedir(), &kaddrlen, addrlen, sizeof(kaddrlen)))
        return SYSCALL_FAULT_(EFAULT);
    if (addr != NULL && addrlen == NULL) return SYSCALL_FAULT_(EINVAL);
    if (kaddrlen > sizeof(struct sockaddr_in6)) kaddrlen = sizeof(struct sockaddr_in6);

    struct sockaddr_in6 kaddr;
    memset(&kaddr, 0, sizeof(kaddr));
    int ret = socket_accept(fd, addr ? (struct sockaddr *)&kaddr : NULL, addr ? &kaddrlen : NULL);
    if (ret < 0) return SYSCALL_FAULT_(-ret);
    if (addr != NULL && !copy_to_user_pagedir(current_user_pagedir(), addr, &kaddr, kaddrlen))
        return SYSCALL_FAULT_(EFAULT);
    if (addrlen != NULL && !copy_to_user_pagedir(current_user_pagedir(), addrlen, &kaddrlen, sizeof(kaddrlen)))
        return SYSCALL_FAULT_(EFAULT);
    return ret;
}


sys_(stat, char *fn, struct stat *buf)
{
    if (unlikely(fn == NULL || buf == NULL)) return SYSCALL_FAULT_(EINVAL);

    char *user_path = NULL;
    int ret = copy_string_from_user(&user_path, fn, USER_PATH_MAX);
    if (ret < 0) return SYSCALL_FAULT_((int)-ret);

    char *path = vfs_cwd_path_build(user_path);
    free(user_path);
    if (path == NULL) return SYSCALL_FAULT_(ENOMEM);

    struct stat kstat;
    uint64_t stat_ret = stat_kernel_path(path, &kstat);
    free(path);
    if ((int64_t)stat_ret < 0) return stat_ret;
    return copy_stat_to_user(buf, &kstat);
}


sys_(arch_prctl, uint64_t code, uint64_t addr)
{
    tcb_t thread = get_current_task();
    switch (code)
    {
    case ARCH_SET_FS:
        thread->fs_base = addr;
        write_fsbase(thread->fs_base);
        break;
    case ARCH_GET_FS: return thread->fs_base;
    // case ARCH_SET_GS:
    //     thread->gs_base = addr;
    //     write_gsbase(thread->gs_base);
    //     break;
    // case ARCH_GET_GS: return thread->gs_base;
    default: return SYSCALL_FAULT;
    }
    return 0;
}

sys_(yield)
{
    scheduler_yield();
    return 0;
}

sys_(uname, struct utsname *utsname)
{
    if (unlikely(utsname == NULL)) return SYSCALL_FAULT_(EINVAL);
    struct utsname kname;
    memset(&kname, 0, sizeof(kname));
    char sysname[] = "Linux";
    char machine[] = "x86_64";
    char release[] = "3.2.0";
    char version[] = "XJ380OS 2.1";
    memcpy(kname.sysname, sysname, sizeof(sysname));
    memcpy(kname.nodename, "localhost", 10);
    memcpy(kname.release, release, sizeof(release));
    memcpy(kname.version, version, sizeof(version));
    memcpy(kname.machine, machine, sizeof(machine));
    if (!copy_to_user_pagedir(current_user_pagedir(), utsname, &kname, sizeof(kname))) return SYSCALL_FAULT_(EFAULT);
    return 0;
}
void scheduler_nano_sleep(uint64_t nano) {
    uint64_t targetTime        = nanoTime();
    uint64_t after             = 0;
    get_current_task()->status = WAIT;
    while(1) {
        uint64_t n = nanoTime();
        if (n < targetTime) {
            after      += UINT64_MAX - targetTime + n;
            targetTime  = n;
        } else {
            after      += n - targetTime;
            targetTime  = n;
        }
        if (after >= nano) {
            get_current_task()->status = RUNNING;
            return;
        }
        if (nano > 10) {
            scheduler_yield(); // 让出CPU时间片
        }
    }
}
sys_(nano_sleep, void *time_handle)
{
    struct timespec k_req;
    if (unlikely(time_handle == NULL)) return SYSCALL_FAULT_(EINVAL);
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &k_req, time_handle, sizeof(k_req)))
    {
        return SYSCALL_FAULT_(EFAULT);
    }
    if (unlikely(k_req.tv_nsec >= 1000000000L)) return SYSCALL_FAULT_(EINVAL);
    if (k_req.tv_sec > (UINT64_MAX - k_req.tv_nsec) / 1000000000ULL) return SYSCALL_FAULT_(EINVAL);
    uint64_t nsec = k_req.tv_sec * 1000000000 + k_req.tv_nsec;
    scheduler_nano_sleep(nsec);
    return 0;
}

sys_(ioctl, int fd, int options, void *arg2)
{
    pcb_t process = get_current_task()->parent_group;
    if (process != NULL && process->linux_abi)
    {
        // write_serial_fmt("[busybox-debug] sys_ioctl fd=%d req=0x%x arg=0x%llx\n",
        //                  fd,
        //                  options,
        //                  (unsigned long long)(uintptr_t)arg2);
    }
    if (unlikely(fd < 0)) return SYSCALL_FAULT_(EINVAL);
    fd_file_handle *handle =  (fd_file_handle*)queue_get(get_current_task()->parent_group->file_open, fd);
    if (handle == NULL) return SYSCALL_FAULT_(EBADF);

    size_t arg_size = ioctl_arg_size(options);
    void *karg = arg2;
    if (arg_size > 0)
    {
        if (arg2 == NULL) return SYSCALL_FAULT_(EFAULT);
        karg = calloc(1, arg_size);
        if (karg == NULL) return SYSCALL_FAULT_(ENOMEM);
        if (ioctl_reads_user(options) && !copy_from_user_pagedir(current_user_pagedir(), karg, arg2, arg_size))
        {
            free(karg);
            return SYSCALL_FAULT_(EFAULT);
        }
    }
    else if (arg2 == NULL)
    {
        karg = NULL;
    }

    int ret = vfs_ioctl(handle->node, options, karg);
    if (ret == EOK && arg_size > 0 && ioctl_writes_user(options) &&
        !copy_to_user_pagedir(current_user_pagedir(), arg2, karg, arg_size))
    {
        free(karg);
        return SYSCALL_FAULT_(EFAULT);
    }
    if (arg_size > 0) free(karg);
    if (process != NULL && process->linux_abi)
    {
        // write_serial_fmt("[busybox-debug] sys_ioctl ret=%d\n", ret);
    }
    return ret;
}

sys_(writev, int fd, struct iovec *iov, int iovcnt)
{
    if (unlikely(fd < 0 || iov == NULL)) return SYSCALL_FAULT_(EINVAL);
    if (unlikely(iovcnt < 0 || (size_t)iovcnt > USER_IOV_MAX)) return SYSCALL_FAULT_(EINVAL);
    if (iovcnt == 0) return 0;
    size_t iov_bytes = 0;
    if (!checked_mul_size((size_t)iovcnt, sizeof(struct iovec), &iov_bytes) ||
        !user_range_mapped(current_user_pagedir(), iov, iov_bytes))
    {
        return SYSCALL_FAULT_(EFAULT);
    }
    page_directory_t *pagedir = get_current_task()->parent_group->pagedir;
    fd_file_handle *handle =  (fd_file_handle*)queue_get(get_current_task()->parent_group->file_open, fd);
    if (handle == NULL) return SYSCALL_FAULT_(EBADF);
    if (handle->node->type & file_socket)
    {
        size_t total = 0;
        for (int i = 0; i < iovcnt; i++)
        {
            struct iovec kiov;
            if (!copy_from_user_pagedir(pagedir, &kiov, &iov[i], sizeof(kiov)))
            {
                return total > 0 ? total : SYSCALL_FAULT_(EFAULT);
            }
            int64_t ret = write_from_user_buffer(handle, (const uint8_t *)kiov.iov_base, kiov.iov_len, true);
            if (ret < 0)
            {
                if (total > 0) return total;
                return SYSCALL_FAULT_((int)-ret);
            }
            if (!checked_add_size(total, (size_t)ret, &total)) return SYSCALL_FAULT_(EINVAL);
            if ((size_t)ret < kiov.iov_len) break;
        }
        return total;
    }
    size_t          total  = 0;
    for (int i = 0; i < iovcnt; i++)
    {
        struct iovec kiov;
        if (!copy_from_user_pagedir(pagedir, &kiov, &iov[i], sizeof(kiov)))
        {
            return total > 0 ? total : SYSCALL_FAULT_(EFAULT);
        }
        int64_t status = write_from_user_buffer(handle, (const uint8_t *)kiov.iov_base, kiov.iov_len, false);
        if (status < 0) return total > 0 ? total : SYSCALL_FAULT_((int)-status);
        if (!checked_add_size(total, (size_t)status, &total)) return SYSCALL_FAULT_(EINVAL);
        if ((size_t)status < kiov.iov_len) break;
    }
    return total;
}

sys_(readv, int fd, struct iovec *iov, int iovcnt0)
{
    if (unlikely(fd < 0 || iov == NULL)) return SYSCALL_FAULT_(EINVAL);
    if (unlikely(iovcnt0 < 0 || (size_t)iovcnt0 > USER_IOV_MAX)) return SYSCALL_FAULT_(EINVAL);
    if (iovcnt0 == 0) return 0;
    size_t          iovcnt = (size_t)iovcnt0;
    fd_file_handle *handle =  (fd_file_handle*)queue_get(get_current_task()->parent_group->file_open, fd);
    if (handle == NULL) return SYSCALL_FAULT_(EBADF);

    size_t iov_bytes = 0;
    if (!checked_mul_size(iovcnt, sizeof(struct iovec), &iov_bytes)) return SYSCALL_FAULT_(EFAULT);
    struct iovec *kiov = (struct iovec *)malloc(iov_bytes);
    if (kiov == NULL) return SYSCALL_FAULT_(ENOMEM);
    if (!copy_from_user_pagedir(current_user_pagedir(), kiov, iov, iov_bytes))
    {
        free(kiov);
        return SYSCALL_FAULT_(EFAULT);
    }

    size_t total = 0;
    for (size_t i = 0; i < iovcnt; i++)
    {
        if (kiov[i].iov_len == 0) continue;
        uint64_t ret = sys_read(fd, (uint8_t *)kiov[i].iov_base, kiov[i].iov_len, 0, 0, 0, regs);
        if ((int64_t)ret < 0)
        {
            free(kiov);
            return total > 0 ? total : ret;
        }
        if (!checked_add_size(total, (size_t)ret, &total))
        {
            free(kiov);
            return SYSCALL_FAULT_(EINVAL);
        }
        if ((size_t)ret < kiov[i].iov_len) break;
    }

    free(kiov);
    return total;
}
spin_t mm_op_lock=SPIN_INIT;

uint64_t sys_brk(uint64_t brk) {
    pcb_t process = get_current_task()->parent_group;

    // Linux 语义里 brk(0) 用来查询当前 program break，不能返回堆起点。
    if (!brk) {
        return process->brk_current;
    }

    if (!align_up_u64(brk, PAGE_SIZE, &brk)) return process->brk_current;

    // 用户堆只能在装载器预留的 brk 区间里伸缩，越界时保持原 break 不变。
    if (brk < process->brk_start || brk > process->brk_end) {
        return process->brk_current;
    }

    if (brk == process->brk_current) {
        return process->brk_current;
    }

    // VMA 始终描述完整 heap 区间 [brk_start, brk)，避免增长/收缩后留下碎片 VMA。
    vma_t *new_heap_vma = NULL;
    if (brk > process->brk_start) {
        new_heap_vma = vma_alloc();
        if (!new_heap_vma) return process->brk_current;

        new_heap_vma->vm_start = process->brk_start;
        new_heap_vma->vm_end = brk;
        new_heap_vma->vm_flags = VMA_READ | VMA_WRITE | VMA_ANON;
        new_heap_vma->vm_type = VMA_TYPE_ANON;
        new_heap_vma->vm_name = strdup("[heap]");
        if (!new_heap_vma->vm_name) {
            vma_free(new_heap_vma);
            return process->brk_current;
        }
    }

    spin_lock(&mm_op_lock);

    if (brk > process->brk_current) {
        if (vma_find_intersection(&process->vma_manager, process->brk_current, brk)) {
            spin_unlock(&mm_op_lock);
            if (new_heap_vma) vma_free(new_heap_vma);
            return process->brk_current;
        }

        // brk 分配的是用户态堆页，必须带 PTE_USER，否则用户态 malloc 会页故障。
        page_map_range_to_random(get_current_directory(),
                       process->brk_current,
                       brk - process->brk_current,
                       PTE_USER | PTE_PRESENT | PTE_WRITEABLE);
    } else {
        unmap_page_range(get_current_directory(), brk, process->brk_current - brk);
    }

    vma_t *old_heap_vma =
        vma_find_intersection(&process->vma_manager, process->brk_start, process->brk_end);

    if (old_heap_vma) {
        vma_remove(&process->vma_manager, old_heap_vma);
        vma_free(old_heap_vma);
    }

    if (new_heap_vma && vma_insert(&process->vma_manager, new_heap_vma) != 0) {
        vma_free(new_heap_vma);
        spin_unlock(&mm_op_lock);
        return process->brk_current;
    }

    process->brk_current = brk;

    spin_unlock(&mm_op_lock);

    return process->brk_current;
}

sys_(mmap, uint64_t addr, size_t length, uint64_t prot, uint64_t flags, int fd,
         uint64_t offset) {

    addr = addr & (~(PAGE_SIZE - 1));

    uint64_t aligned_len = 0;
    if (!align_up_u64(length, PAGE_SIZE, &aligned_len)) { return SYSCALL_FAULT_(EINVAL); }

    if (check_user_overflow(addr, aligned_len)) { return -EFAULT; }

    if (aligned_len == 0) { return SYSCALL_FAULT_(EINVAL); }
    pcb_t process = get_current_task()->parent_group;

    vma_manager_t *mgr        = &process->vma_manager;
    uint64_t       start_addr = 0;
    if (flags & MAP_FIXED) {
        if (!addr) return SYSCALL_FAULT_(EINVAL);

        start_addr = addr;
        // 检查地址是否可用
        if (vma_find_intersection(mgr, start_addr, start_addr + aligned_len)) {
            vma_unmap_range(mgr, start_addr, start_addr + aligned_len);
        }
    } else {
        if (addr) {
            start_addr = addr;
            // 检查地址是否可用
            if (vma_find_intersection(mgr, start_addr, start_addr + aligned_len)) {
                return SYSCALL_FAULT_(ENOMEM);
            }
        } else {
            start_addr = USER_MMAP_START;
            while (true) {
                if (check_user_overflow(start_addr, aligned_len)) return SYSCALL_FAULT_(ENOMEM);
                if (!vma_find_intersection(mgr, start_addr, start_addr + aligned_len)) break;
                start_addr += PAGE_SIZE;
                if (start_addr > KERNEL_AREA_MEM) return SYSCALL_FAULT_(ENOMEM);
            }
        }
    }

    if (!(flags & MAP_ANONYMOUS)) {
        if (queue_get(process->file_open, fd) == NULL) return SYSCALL_FAULT_(EBADF);
    }

    spin_lock(&mm_op_lock);

    vma_t *vma = vma_alloc();
    if (!vma) {
        spin_unlock(&mm_op_lock);
        return SYSCALL_FAULT_(ENOMEM);
    }

    vma->vm_start = start_addr;
    vma->vm_end   = start_addr + aligned_len;
    vma->vm_flags = 0;

    if (prot & PROT_READ) vma->vm_flags |= VMA_READ;
    if (prot & PROT_WRITE) vma->vm_flags |= VMA_WRITE;
    if (prot & PROT_EXEC) vma->vm_flags |= VMA_EXEC;
    if (flags & MAP_SHARED) vma->vm_flags |= VMA_SHARED;

    if (flags & MAP_ANONYMOUS) {
        vma->vm_type   = VMA_TYPE_ANON;
        vma->vm_flags |= VMA_ANON;
        vma->vm_fd     = -1;
    } else {
        vma->vm_type   = VMA_TYPE_FILE;
        vma->vm_fd     = fd;
        vma->vm_offset = (int64_t)offset;
    }

    vma_t *region = vma_find_intersection(mgr, start_addr, start_addr + aligned_len);
    if (region) {
        vma_remove(mgr, region);
        vma_free(region);
    }

    if (vma_insert(mgr, vma) != 0) {
        vma_free(vma);
        spin_unlock(&mm_op_lock);
        return SYSCALL_FAULT_(ENOMEM);
    }

    if (!(flags & MAP_ANONYMOUS)) {
        uint64_t ret = (uint64_t)vfs_map((vfs_node_t)queue_get(process->file_open, fd), start_addr, aligned_len,
                                         prot, flags, offset);
        spin_unlock(&mm_op_lock);
        return ret;
    }

    uint64_t pt_flags = PTE_USER | PTE_PRESENT | PTE_WRITEABLE;

    if (prot != PROT_NONE) {
        if (prot & PROT_READ) pt_flags |= PTE_PRESENT;
        if (prot & PROT_WRITE) pt_flags |= PTE_WRITEABLE;
        if (!(prot & PROT_EXEC)) pt_flags |= PTE_NO_EXECUTE;
    }

    lazy_infoalloc(process, start_addr, aligned_len, pt_flags, flags);
    spin_unlock(&mm_op_lock);

    return start_addr;

    return addr;
}

sys_(mprotect, uint64_t addr, size_t length, uint64_t prot)
{
    if (length == 0) return 0;

    addr = addr & (~(PAGE_SIZE - 1));
    uint64_t aligned_len = 0;
    if (!align_up_u64(length, PAGE_SIZE, &aligned_len)) return SYSCALL_FAULT_(EINVAL);
    if (check_user_overflow(addr, aligned_len)) return SYSCALL_FAULT_(EFAULT);

    uint64_t start = addr;
    uint64_t end   = addr + aligned_len;
    vma_manager_t *mgr = &get_current_task()->parent_group->vma_manager;

    spin_lock(&mm_op_lock);

    uint64_t cursor = start;
    while (cursor < end) {
        vma_t *vma = vma_find(mgr, cursor);
        if (vma == NULL) {
            spin_unlock(&mm_op_lock);
            return SYSCALL_FAULT_(ENOMEM);
        }
        cursor = vma->vm_end < end ? vma->vm_end : end;
    }

    cursor = start;
    while (cursor < end) {
        vma_t *vma = vma_find(mgr, cursor);
        if (vma == NULL) break;

        if (vma->vm_start < start) {
            vma_split(vma, start);
            vma = vma_find(mgr, start);
        }
        if (vma != NULL && vma->vm_end > end) {
            vma_split(vma, end);
        }
        if (vma != NULL) {
            vma->vm_flags &= ~(VMA_READ | VMA_WRITE | VMA_EXEC);
            if (prot & PROT_READ) vma->vm_flags |= VMA_READ;
            if (prot & PROT_WRITE) vma->vm_flags |= VMA_WRITE;
            if (prot & PROT_EXEC) vma->vm_flags |= VMA_EXEC;
            cursor = vma->vm_end;
        }
    }

    spin_unlock(&mm_op_lock);
    return 0;
}

sys_(munmap, uint64_t addr, size_t size) {
    if (size == 0) return 0;

    addr = addr & (~(PAGE_SIZE - 1));
    uint64_t aligned_size = 0;
    if (!align_up_u64(size, PAGE_SIZE, &aligned_size)) return SYSCALL_FAULT_(EINVAL);
    size = (size_t)aligned_size;

    if (check_user_overflow(addr, size)) { return -EFAULT; }

    vma_manager_t *mgr  = &get_current_task()->parent_group->vma_manager;
    vma_t         *vma  = mgr->vma_list;
    vma_t         *next = NULL;

    uint64_t start = addr;
    uint64_t end   = addr + size;

    while (vma) {
        next = vma->vm_next;

        // 完全包含在要取消映射的范围内
        if (vma->vm_start >= start && vma->vm_end <= end) {
            vma_remove(mgr, vma);
            vma_free(vma);
        }
        // 部分重叠 - 需要分割
        else if (!(vma->vm_end <= start || vma->vm_start >= end)) {
            if (vma->vm_start < start && vma->vm_end > end) {
                // VMA跨越整个取消映射范围 - 分割成两部分
                vma_split(vma, end);
                vma_split(vma, start);
                // 移除中间部分
                vma_t *middle = vma->vm_next;
                vma_remove(mgr, middle);
                vma_free(middle);
            } else if (vma->vm_start < start) {
                // 截断VMA的末尾
                vma->vm_end = start;
            } else if (vma->vm_end > end) {
                // 截断VMA的开头
                vma->vm_start = end;
                if (vma->vm_type == VMA_TYPE_FILE) { vma->vm_offset += end - vma->vm_start; }
            }
        }

        vma = next;
    }

    unmap_virtual_page(get_current_task()->parent_group, addr, size);
    unmap_page_range(get_current_directory(), addr, size);
    return 0;
}

sys_(mremap, uint64_t old_addr, uint64_t old_size, uint64_t new_size, uint64_t flags,
         uint64_t new_addr) {
    old_addr = old_addr & (~(PAGE_SIZE - 1));
    new_addr = new_addr & (~(PAGE_SIZE - 1));
    if (!align_up_u64(old_size, PAGE_SIZE, &old_size) ||
        !align_up_u64(new_size, PAGE_SIZE, &new_size))
    {
        return SYSCALL_FAULT_(EINVAL);
    }
    if (check_user_overflow(old_addr, old_size) || check_user_overflow(new_addr, new_size))
        return SYSCALL_FAULT_(EFAULT);

    vma_manager_t *mgr = &get_current_task()->parent_group->vma_manager;

    vma_t *vma = vma_find(mgr, (unsigned long)old_addr);
    if (!vma || vma->vm_start != (unsigned long)old_addr) { return SYSCALL_FAULT_(EINVAL); }

    uint64_t old_addr_phys = page_virt_to_phys(old_addr);

    // 如果新大小更小，直接截断
    if (new_size <= vma->vm_end - vma->vm_start) {
        unmap_page_range(get_current_directory(), vma->vm_end,
                         vma->vm_start + new_size - vma->vm_end);
        vma->vm_end = vma->vm_start + new_size;
        return old_addr;
    }

    // 如果需要扩大，检查是否有足够空间
    if (check_user_overflow(vma->vm_start, new_size)) return SYSCALL_FAULT_(ENOMEM);
    uint64_t new_end = vma->vm_start + new_size;
    if (!vma_find_intersection(mgr, vma->vm_end, new_end)) {
        uint64_t pt_flags = PTE_USER | PTE_PRESENT | PTE_WRITEABLE;

        if (vma->vm_flags & VMA_READ) pt_flags |= PTE_PRESENT;
        if (vma->vm_flags & VMA_WRITE) pt_flags |= PTE_WRITEABLE;
        if (!(vma->vm_flags & VMA_EXEC)) pt_flags |= PTE_NO_EXECUTE;

        page_map_range(get_current_directory(), vma->vm_end,
                       old_addr_phys + vma->vm_end - vma->vm_start, new_end - vma->vm_end,
                       vma->vm_flags);

        vma->vm_end = new_end;
        return old_addr;
    }

    if (flags & MREMAP_MAYMOVE) {
        // 简单的地址分配策略：从高地址开始
        uint64_t start_addr = USER_MMAP_START;
        while (true) {
            if (check_user_overflow(start_addr, new_size)) return SYSCALL_FAULT_(ENOMEM);
            if (!vma_find_intersection(mgr, start_addr, start_addr + new_size)) break;
            start_addr += PAGE_SIZE;
            if (start_addr > KERNEL_AREA_MEM) return SYSCALL_FAULT_(ENOMEM);
        }

        vma_t *new_vma = vma_alloc();
        if (!new_vma) return SYSCALL_FAULT_(ENOMEM);

        memcpy(new_vma, vma, sizeof(vma_t));
        new_vma->vm_start = start_addr;
        new_vma->vm_end   = start_addr + new_size;
        new_vma->vm_flags = 0;

        if (vma_insert(mgr, new_vma) != 0) {
            vma_free(new_vma);
            return SYSCALL_FAULT_(ENOMEM);
        }

        uint64_t pt_flags = PTE_USER | PTE_PRESENT | PTE_WRITEABLE;

        if (new_vma->vm_flags & VMA_READ) pt_flags |= PTE_PRESENT;
        if (new_vma->vm_flags & VMA_WRITE) pt_flags |= PTE_WRITEABLE;
        if (!(new_vma->vm_flags & VMA_EXEC)) pt_flags |= PTE_NO_EXECUTE;

        page_map_range(get_current_directory(), start_addr, old_addr_phys, new_size, pt_flags);

        sys_munmap(old_addr, old_size, 0, 0, 0, 0, regs);
        return start_addr;
    }

    return (uint64_t)-ENOMEM;
}

sys_(getcwd, char *buffer, size_t length)
{
    if (unlikely(buffer == NULL)) return SYSCALL_FAULT_(EINVAL);
    if (unlikely(length == 0)) return 0;
    if (!user_range_mapped(current_user_pagedir(), buffer, length)) return SYSCALL_FAULT_(EFAULT);

    tcb_t  process  = get_current_task();
    char  *cwd      = vfs_get_fullpath(process->cwd);
    if (unlikely(cwd == NULL)) return SYSCALL_FAULT_(ENOENT);

    size_t cwd_leng = strlen(cwd);
    size_t copy_len = cwd_leng;
    if (copy_len >= length) copy_len = length - 1;
    if (!copy_to_user_pagedir(current_user_pagedir(), buffer, cwd, copy_len) ||
        !copy_to_user_pagedir(current_user_pagedir(), buffer + copy_len, "", 1))
    {
        free(cwd);
        return SYSCALL_FAULT_(EFAULT);
    }
    free(cwd);
    return copy_len;
}

sys_(chdir, char *s)
{
    if (unlikely(s == NULL)) return SYSCALL_FAULT_(EINVAL);
    tcb_t current = get_current_task();

    char *user_path = NULL;
    int copy_ret = copy_string_from_user(&user_path, s, USER_PATH_MAX);
    if (copy_ret < 0) return SYSCALL_FAULT_((int)-copy_ret);

    char *path;
    char *bpath = NULL;
    if (user_path[0] == '/') { path = strdup(user_path); }
    else
    {
        bpath = vfs_get_fullpath(current->cwd);
        path  = pathacat(bpath, user_path);
    }
    free(user_path);

    if (unlikely(path == NULL))
    {
        free(bpath);
        return SYSCALL_FAULT_(ENOMEM);
    }

    char *normalized_path = normalize_path(path);
    free(path);
    free(bpath);

    if (unlikely(normalized_path == NULL)) { return SYSCALL_FAULT_(ENOMEM); }

    vfs_node_t node;
    if ((node = vfs_open(normalized_path)) == NULL)
    {
        free(normalized_path);
        return SYSCALL_FAULT_(ENOENT);
    }

    if (!(node->type & file_dir))
    {
        vfs_close(node);
        free(normalized_path);
        return SYSCALL_FAULT_(ENOTDIR);
    }

    if (current->cwd != NULL) vfs_close(current->cwd);
    current->cwd = node;
    current->cwd->refcount++;

    if (current->str_cwd != NULL) free(current->str_cwd);
    current->str_cwd = normalized_path;

    return 0;
}

sys_(exit_group, int exit_code)
{
    pcb_t exit_process = get_current_task()->parent_group;
    write_serial_fmt("task: Process %s exit with code %d.\n", exit_process->name, exit_code);
    close_interrupt;//applets 已经走到这个
    kill_proc(exit_process, exit_code,
              true); // 子进程调用，is_zombie = true，不能在child_pcb中删除当前进程
    open_interrupt;
    scheduler_yield();
    cpu_hlt;
}

static uint64_t poll_kernel_fds(struct pollfd *fds_user, size_t nfds, size_t timeout)
{
    int      ready      = 0;
    uint64_t start_time = nanoTime();
    uint64_t timeout_ns = timeout == (size_t)-1 ? (uint64_t)-1 : timeout * 1000000ULL;
    bool     sigexit    = false;

    extern vfs_callback_t fs_callbacks[256];

    do
    {
        ready = 0;
        for (size_t i = 0; i < nfds; i++)
        {
            fds_user[i].revents = 0;
            if (fds_user[i].fd < 0) continue;
            fd_file_handle *handle =
                (fd_file_handle *)queue_get(get_current_task()->parent_group->file_open, fds_user[i].fd);
            if (handle == NULL)
            {
                fds_user[i].revents = POLLNVAL;
                ready++;
                continue;
            }
            vfs_node_t      node   = handle->node;
            if (fs_callbacks[node->fsid]->poll == (void *)empty)
            {
                if (fds_user[i].events & POLLIN || fds_user[i].events & POLLOUT)
                {
                    fds_user[i].revents = fds_user[i].events & POLLIN ? POLLIN : POLLOUT;
                    ready++;
                }
                continue;
            }
            int revents = (int)epoll_to_poll_comp(vfs_poll(node, poll_to_epoll_comp(fds_user[i].events)));
            if (revents > 0)
            {
                fds_user[i].revents = (short)revents;
                ready++;
            }
        }

        // sigexit = signals_pending_quick(get_current_task());

        if (ready > 0 || sigexit) break;

        open_interrupt;
        __asm__("pause");
    } while (timeout != 0 && ((int)timeout == -1 || (nanoTime() - start_time) < timeout_ns));

    close_interrupt;

    if (!ready && sigexit) return (size_t)-EINTR;
    return ready;
}

sys_(poll, struct pollfd *fds_user, size_t nfds, size_t timeout)
{
    if (nfds == 0) return 0;
    if (fds_user == NULL) return SYSCALL_FAULT_(EINVAL);
    if (nfds > USER_POLL_MAX) return SYSCALL_FAULT_(EINVAL);

    size_t fds_bytes = 0;
    if (!checked_mul_size(nfds, sizeof(struct pollfd), &fds_bytes)) return SYSCALL_FAULT_(EFAULT);
    struct pollfd *fds = (struct pollfd *)malloc(fds_bytes);
    if (fds == NULL) return SYSCALL_FAULT_(ENOMEM);
    if (!copy_from_user_pagedir(current_user_pagedir(), fds, fds_user, fds_bytes))
    {
        free(fds);
        return SYSCALL_FAULT_(EFAULT);
    }

    uint64_t ret = poll_kernel_fds(fds, nfds, timeout);
    if ((int64_t)ret >= 0 && !copy_to_user_pagedir(current_user_pagedir(), fds_user, fds, fds_bytes))
    {
        free(fds);
        return SYSCALL_FAULT_(EFAULT);
    }

    free(fds);
    return ret;
}

sys_(sched_getaffinity)
{
    int  pid      = (int)arg0;
    size_t set_size = (size_t)arg1;
    return SYSCALL_FAULT_(ENOSYS);
}

sys_(rt_sigprocmask, int how, sigset_t *set, sigset_t *oldset)
{
    return syscall_ssetmask(how, set, oldset);
}

sys_(rt_sigaction, int sig, sigaction_t *action, sigaction_t *oldaction, size_t sigsetsize)
{
    if (sigsetsize < sizeof(sigset_t)) return SYSCALL_FAULT_(EINVAL);
    int ret = syscall_sig_action(sig, action, oldaction);
    return ret < 0 ? (uint64_t)ret : (uint64_t)ret;
}

sys_(kill, int pid, int sig)
{
    pcb_t target = NULL;
    if (pid > 0) target = found_pcb(pid);
    else if (pid == 0) target = get_current_task()->parent_group;
    else return SYSCALL_FAULT_(ENOSYS);

    if (target == NULL) return SYSCALL_FAULT_(ESRCH);
    int ret = signal_send_process(target, sig);
    return ret < 0 ? (uint64_t)ret : (uint64_t)ret;
}

sys_(tgkill, int tgid, int tid, int sig)
{
    pcb_t target = found_pcb(tgid);
    if (target == NULL) return SYSCALL_FAULT_(ESRCH);
    if (tid <= 0 || found_thread(target, tid) == NULL) return SYSCALL_FAULT_(ESRCH);

    int ret = signal_send_process(target, sig);
    return ret < 0 ? (uint64_t)ret : (uint64_t)ret;
}

extern fd_file_handle *fd_dup(fd_file_handle *src);

sys_(dup2, int fd, int newfd)
{
    fd_file_handle *handle =  (fd_file_handle*)queue_get(get_current_task()->parent_group->file_open, fd);
    if (unlikely(handle == NULL)) return SYSCALL_FAULT_(EBADF);

    fd_file_handle *old_handle =  (fd_file_handle*)queue_get(get_current_task()->parent_group->file_open, newfd);
    if (old_handle != NULL)
    {
        queue_remove_at(get_current_task()->parent_group->file_open, newfd);
        vfs_close(old_handle->node);
        free(old_handle);
    }

    fd_file_handle *new_handle = fd_dup(handle);
    if (new_handle == NULL) return SYSCALL_FAULT_(ENOMEM);
    queue_enqueue_id(get_current_task()->parent_group->file_open, new_handle, newfd);
    return newfd;
}

sys_(dup, int fd)
{
    if (unlikely(fd < 0)) return SYSCALL_FAULT_(EINVAL);
    fd_file_handle *handle =  (fd_file_handle*)queue_get(get_current_task()->parent_group->file_open, fd);
    if (handle == NULL) return SYSCALL_FAULT_(EBADF);
    fd_file_handle *new_handle = fd_dup(handle);
    return new_handle->fd      = queue_enqueue(get_current_task()->parent_group->file_open, new_handle);
}

sys_(fcntl, int fd, int cmd, uint64_t arg)
{
    if (fd < 0 || cmd < 0) return SYSCALL_FAULT_(EINVAL);
    fd_file_handle *handle =  (fd_file_handle*)queue_get(get_current_task()->parent_group->file_open, fd);
    if (handle == NULL) return SYSCALL_FAULT_(EBADF);

    switch (cmd)
    {
    case F_GETFD: return (handle->node->flags & O_CLOEXEC) != 0;
    case F_SETFD: return handle->node->flags |= O_CLOEXEC;
    case F_DUPFD_CLOEXEC:
        {uint64_t newfd       = sys_dup(fd, 0, 0, 0, 0, 0, regs);
        handle->node->flags |= O_CLOEXEC;
        return newfd;}
    case F_DUPFD: return sys_dup(fd, 0, 0, 0, 0, 0, regs);
    case F_GETFL: return handle->node->flags;
    case F_SETFL:
        {uint32_t valid_flags  = O_APPEND | O_DIRECT | O_NOATIME | O_NONBLOCK;
        handle->node->flags  &= ~valid_flags;
        handle->node->flags  |= arg & valid_flags;
        handle->flags        &= ~valid_flags;
        handle->flags        |= arg & valid_flags;
        if (handle->node->type & file_socket) {
            int ret = socket_apply_flags(handle->node, handle->node->flags);
            if (ret < 0) return SYSCALL_FAULT_(-ret);
        }}
    default: break;
    }
    return 0;
}

sys_(fork)
{
    return process_fork(regs, false, 0);
}

sys_(clone, uint64_t flags, uint64_t stack, int *parent_tid, int *child_tid, uint64_t tls)
{
    return thread_clone(regs, flags, stack, parent_tid, child_tid, tls);
}

sys_(get_tid)
{
    return get_current_task()->tid;
}

sys_(set_tid_address, int *tidptr)
{
    tcb_t task = get_current_task();
    if (task == NULL) return SYSCALL_FAULT_(ESRCH);

    task->clear_child_tid = (uint64_t)tidptr;
    task->tid_directory   = task->parent_group != NULL ? task->parent_group->pagedir : NULL;
    return task->tid;
}

sys_(vfork)
{
    return process_fork(regs, true, 0);
}

sys_(execve, char *path, char **argv, char **envp)
{
    if (unlikely(path == NULL)) return SYSCALL_FAULT_(EINVAL);
    return process_execve(path, argv, envp);
}

sys_(fstat, int fd, struct stat *buf)
{
    if (unlikely(buf == NULL)) return SYSCALL_FAULT_(EINVAL);

    fd_file_handle *handle =  (fd_file_handle*)queue_get(get_current_task()->parent_group->file_open, fd);
    if (unlikely(handle == NULL)) return SYSCALL_FAULT_(EBADF);
    struct stat kstat;
    uint64_t ret = fill_stat_from_node(handle->node, &kstat);
    if ((int64_t)ret < 0) return ret;
    return copy_stat_to_user(buf, &kstat);
}

sys_(time, int64_t *timer)
{
    tm tm;
    time_read(&tm);
    int64_t timestamp = mktime(&tm);
    if (timer != NULL && !copy_to_user_pagedir(current_user_pagedir(), timer, &timestamp, sizeof(timestamp)))
        return SYSCALL_FAULT_(EFAULT);
    return (uint64_t)timestamp;
}

sys_(clock_gettime, uint64_t arg0, struct timespec *ts)
{
    struct timespec kts;
    memset(&kts, 0, sizeof(kts));

    switch (arg0)
    {
    case 1:
    case 6:
    case 4: {
        uint64_t nano = nanoTime();
        kts.tv_sec    = nano / 1000000000ULL;
        kts.tv_nsec   = nano % 1000000000ULL;
        if (ts != NULL && !copy_to_user_pagedir(current_user_pagedir(), ts, &kts, sizeof(kts)))
            return SYSCALL_FAULT_(EFAULT);
        return 0;
    }
    case 0: {
        tm tm;
        time_read(&tm);
        uint64_t timestamp = mktime(&tm);
        kts.tv_sec  = timestamp;
        kts.tv_nsec = 0;
        if (ts != NULL && !copy_to_user_pagedir(current_user_pagedir(), ts, &kts, sizeof(kts)))
            return SYSCALL_FAULT_(EFAULT);
        return 0;
    }
    case 7: {
        // if (ts != NULL)
        // {
        //     uint64_t nano = sched_clock();
        //     ts->tv_sec    = nano / 1000000000ULL;
        //     ts->tv_nsec   = nano % 1000000000ULL;
        // }
        return 0;
    }
    default: write_serial_fmt("clock not supported(%d)\n", arg0); return SYSCALL_FAULT_(EINVAL);
    }
}

sys_(clock_getres)
{
    struct timespec *ts = (struct timespec *)arg1;
    if (ts == NULL) return SYSCALL_FAULT_(EINVAL);
    struct timespec kts;
    memset(&kts, 0, sizeof(kts));
    kts.tv_nsec = 1000000;
    if (!copy_to_user_pagedir(current_user_pagedir(), ts, &kts, sizeof(kts))) return SYSCALL_FAULT_(EFAULT);
    return 0;
}

sys_(shutdown, int fd, int how)
{
    int ret = socket_shutdown(fd, how);
    if (ret < 0) return SYSCALL_FAULT_(-ret);
    return ret;
}


sys_(mkdir, char *name, uint64_t mode)
{
    if (name == NULL) return SYSCALL_FAULT_(EINVAL);
    char *user_path = NULL;
    int copy_ret = copy_string_from_user(&user_path, name, USER_PATH_MAX);
    if (copy_ret < 0) return SYSCALL_FAULT_((int)-copy_ret);

    char  *npath = vfs_cwd_path_build(user_path);
    free(user_path);
    if (npath == NULL) return SYSCALL_FAULT_(ENOMEM);
    size_t ret   = vfs_mkdir(npath) == VFS_STATUS_FAILED ? SYSCALL_FAULT_(EIO) : 0;
    if (ret == 0)
    {
        vfs_node_t node = vfs_open(npath);
        if (node != NULL)
        {
            stamp_node_owner(node);
            if ((mode & 07777) != 0) node->mode = (uint16_t)(mode & 07777);
            vfs_close(node);
        }
    }
    free(npath);
    return ret;
}

sys_(bind, int fd, const struct sockaddr *addr, socklen_t addrlen)
{
    if (addr == NULL) return SYSCALL_FAULT_(EINVAL);
    if (addrlen < sizeof(struct sockaddr) || addrlen > sizeof(struct sockaddr_in6)) return SYSCALL_FAULT_(EINVAL);
    struct sockaddr_in6 kaddr;
    memset(&kaddr, 0, sizeof(kaddr));
    if (!copy_from_user_pagedir(current_user_pagedir(), &kaddr, addr, addrlen)) return SYSCALL_FAULT_(EFAULT);
    int ret = socket_bind(fd, (const struct sockaddr *)&kaddr, addrlen);
    if (ret < 0) return SYSCALL_FAULT_(-ret);
    return ret;
}

sys_(listen, int fd, int backlog)
{
    int ret = socket_listen(fd, backlog);
    if (ret < 0) return SYSCALL_FAULT_(-ret);
    return ret;
}

sys_(lseek, int fd, size_t offset, size_t whence)
{
    fd_file_handle *handle =  (fd_file_handle*)queue_get(get_current_task()->parent_group->file_open, fd);
    if (unlikely(handle == NULL)) return SYSCALL_FAULT_(EBADF);
    if (handle->node->type & file_socket) return SYSCALL_FAULT_(ESPIPE);

    int64_t real_offset = (int64_t)offset;
    if (real_offset < 0 && handle->node->type & file_none && whence != SEEK_CUR) return SYSCALL_FAULT_(EBADF);
    switch (whence)
    {
    case SEEK_SET: handle->offset = real_offset; break;
    case SEEK_CUR:
        handle->offset += real_offset;
        if ((int64_t)handle->offset < 0) { handle->offset = 0; }
        else if (handle->offset > handle->node->size) { handle->offset = handle->node->size; }
        break;
    case SEEK_END: handle->offset = handle->node->size - real_offset; break;
    case SEEK_DATA:
        if (offset >= handle->node->size) return SYSCALL_FAULT_(ENXIO);
        break;
    case SEEK_HOLE:
        if (offset >= handle->node->size) return SYSCALL_FAULT_(ENXIO);
        return handle->node->size;
    default: return SYSCALL_FAULT_(ENXIO);
    }

    return handle->offset;
}

static uint64_t select_common(int nfds, uint8_t *read, uint8_t *write, uint8_t *except,
                              const struct timeval *ktimeout)
{
    if (nfds < 0) return SYSCALL_FAULT_(EINVAL);
    size_t bitmap_bytes = 0;
    if (!user_bitmap_bytes((uint64_t)nfds, &bitmap_bytes)) return SYSCALL_FAULT_(EINVAL);

    page_directory_t *pagedir = current_user_pagedir();
    uint8_t *kread = NULL;
    uint8_t *kwrite = NULL;
    uint8_t *kexcept = NULL;
    if (bitmap_bytes > 0)
    {
        if (read)
        {
            kread = (uint8_t *)malloc(bitmap_bytes);
            if (kread == NULL) return SYSCALL_FAULT_(ENOMEM);
            if (!copy_from_user_pagedir(pagedir, kread, read, bitmap_bytes)) { free(kread); return SYSCALL_FAULT_(EFAULT); }
        }
        if (write)
        {
            kwrite = (uint8_t *)malloc(bitmap_bytes);
            if (kwrite == NULL) { free(kread); return SYSCALL_FAULT_(ENOMEM); }
            if (!copy_from_user_pagedir(pagedir, kwrite, write, bitmap_bytes)) { free(kread); free(kwrite); return SYSCALL_FAULT_(EFAULT); }
        }
        if (except)
        {
            kexcept = (uint8_t *)malloc(bitmap_bytes);
            if (kexcept == NULL) { free(kread); free(kwrite); return SYSCALL_FAULT_(ENOMEM); }
            if (!copy_from_user_pagedir(pagedir, kexcept, except, bitmap_bytes)) { free(kread); free(kwrite); free(kexcept); return SYSCALL_FAULT_(EFAULT); }
        }
    }

    size_t         complength = sizeof(struct pollfd);
    struct pollfd *comp       = (struct pollfd *)malloc(complength);
    if (comp == NULL) { free(kread); free(kwrite); free(kexcept); return SYSCALL_FAULT_(ENOMEM); }
    memset(comp, 0, complength);
    size_t compIndex = 0;
    if (kread)
    {
        for (int i = 0; i < nfds; i++)
        {
            if (select_bitmap(kread, i)) select_add(&comp, &compIndex, &complength, i, POLLIN);
        }
    }
    if (kwrite)
    {
        for (int i = 0; i < nfds; i++)
        {
            if (select_bitmap(kwrite, i)) select_add(&comp, &compIndex, &complength, i, POLLOUT);
        }
    }
    if (kexcept)
    {
        for (int i = 0; i < nfds; i++)
        {
            if (select_bitmap(kexcept, i)) select_add(&comp, &compIndex, &complength, i, POLLPRI | POLLERR);
        }
    }

    if (kread) memset(kread, 0, bitmap_bytes);
    if (kwrite) memset(kwrite, 0, bitmap_bytes);
    if (kexcept) memset(kexcept, 0, bitmap_bytes);

    size_t time = 0;
    if (ktimeout == NULL) { time = -1; }
    else if (ktimeout->tv_sec == -1 || ktimeout->tv_usec == -1) { time = -1; }
    else
    {
        if (ktimeout->tv_sec < 0 || ktimeout->tv_usec < 0 || ktimeout->tv_usec >= 1000000)
        {
            free(kread); free(kwrite); free(kexcept); free(comp);
            return SYSCALL_FAULT_(EINVAL);
        }
        size_t sec_ms = 0;
        size_t usec_with_rounding = 0;
        size_t usec_ms = 0;
        if (!checked_mul_size((size_t)ktimeout->tv_sec, 1000, &sec_ms) ||
            !checked_add_size((size_t)ktimeout->tv_usec, 999, &usec_with_rounding) ||
            !checked_add_size(sec_ms, usec_with_rounding / 1000, &usec_ms))
        {
            free(kread); free(kwrite); free(kexcept); free(comp);
            return SYSCALL_FAULT_(EINVAL);
        }
        time = usec_ms;
    }

    size_t res = poll_kernel_fds(comp, compIndex, time);

    if ((int64_t)res < 0)
    {
        free(kread); free(kwrite); free(kexcept); free(comp);
        return res;
    }

    size_t verify = 0;
    for (size_t i = 0; i < compIndex; i++)
    {
        if (!comp[i].revents) continue;
        if (comp[i].events & POLLIN && comp[i].revents & POLLIN)
        {
            if (kread) select_bitmap_set(kread, comp[i].fd);
            verify++;
        }
        if (comp[i].events & POLLOUT && comp[i].revents & POLLOUT)
        {
            if (kwrite) select_bitmap_set(kwrite, comp[i].fd);
            verify++;
        }
        if ((comp[i].events & POLLPRI && comp[i].revents & POLLPRI))
        {
            if (kexcept) select_bitmap_set(kexcept, comp[i].fd);
            verify++;
        }
    }

    if (kread && !copy_to_user_pagedir(pagedir, read, kread, bitmap_bytes))
    {
        free(kread); free(kwrite); free(kexcept); free(comp);
        return SYSCALL_FAULT_(EFAULT);
    }
    if (kwrite && !copy_to_user_pagedir(pagedir, write, kwrite, bitmap_bytes))
    {
        free(kread); free(kwrite); free(kexcept); free(comp);
        return SYSCALL_FAULT_(EFAULT);
    }
    if (kexcept && !copy_to_user_pagedir(pagedir, except, kexcept, bitmap_bytes))
    {
        free(kread); free(kwrite); free(kexcept); free(comp);
        return SYSCALL_FAULT_(EFAULT);
    }

    free(kread);
    free(kwrite);
    free(kexcept);
    free(comp);
    return verify;
}

sys_(select, int nfds, uint8_t *read, uint8_t *write, uint8_t *except, struct timeval *timeout)
{
    struct timeval ktimeout;
    const struct timeval *ktimeout_ptr = NULL;
    if (timeout != NULL)
    {
        if (!copy_from_user_pagedir(current_user_pagedir(), &ktimeout, timeout, sizeof(ktimeout)))
            return SYSCALL_FAULT_(EFAULT);
        ktimeout_ptr = &ktimeout;
    }

    return select_common(nfds, read, write, except, ktimeout_ptr);
}

sys_(pselect6, uint64_t nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timespec *timeout,
     WeirdPselect6 *weirdPselect6)
{
    size_t bitmap_bytes = 0;
    if (!user_bitmap_bytes(nfds, &bitmap_bytes)) return SYSCALL_FAULT_(EINVAL);
    page_directory_t *pagedir = current_user_pagedir();
    if (readfds && !user_range_mapped(pagedir, readfds, bitmap_bytes)) { return SYSCALL_FAULT_(EFAULT); }
    if (writefds && !user_range_mapped(pagedir, writefds, bitmap_bytes)) { return SYSCALL_FAULT_(EFAULT); }
    if (exceptfds && !user_range_mapped(pagedir, exceptfds, bitmap_bytes)) { return SYSCALL_FAULT_(EFAULT); }

    WeirdPselect6 kweird;
    memset(&kweird, 0, sizeof(kweird));
    if (weirdPselect6 != NULL &&
        !copy_from_user_pagedir(pagedir, &kweird, weirdPselect6, sizeof(kweird)))
    {
        return SYSCALL_FAULT_(EFAULT);
    }
    size_t    sigsetsize = kweird.ss_len;
    sigset_t *sigmask    = kweird.ss;
    if (sigmask != NULL && sigsetsize < sizeof(sigset_t)) { return SYSCALL_FAULT_(EINVAL); }
    sigset_t origmask = 0;
    if (sigmask)
    {
        sigset_t requested_mask = 0;
        if (!copy_from_user_pagedir(pagedir, &requested_mask, sigmask, sizeof(requested_mask)))
        {
            return SYSCALL_FAULT_(EFAULT);
        }
        signal_setmask(SIG_SETMASK, &requested_mask, &origmask);
    }
    struct timeval timeoutConv;
    const struct timeval *timeoutPtr = NULL;
    if (timeout)
    {
        struct timespec ktimeout;
        if (!copy_from_user_pagedir(pagedir, &ktimeout, timeout, sizeof(ktimeout)))
        {
            if (sigmask) { signal_setmask(SIG_SETMASK, &origmask, NULL); }
            return SYSCALL_FAULT_(EFAULT);
        }
        if (ktimeout.tv_sec > (uint64_t)LONG_MAX || ktimeout.tv_nsec >= 1000000000ULL)
        {
            if (sigmask) { signal_setmask(SIG_SETMASK, &origmask, NULL); }
            return SYSCALL_FAULT_(EINVAL);
        }
        timeoutConv =
            (struct timeval){.tv_sec = (long)ktimeout.tv_sec, .tv_usec = (long)(ktimeout.tv_nsec / 1000)};
        timeoutPtr = &timeoutConv;
    }

    size_t ret = select_common((int)nfds, (uint8_t *)readfds, (uint8_t *)writefds, (uint8_t *)exceptfds,
                               timeoutPtr);
    if (sigmask) { signal_setmask(SIG_SETMASK, &origmask, NULL); }
    return ret;
}


sys_(getdents, int fd, struct dirent *dents, size_t size)
{
    if (size == 0) return 0;
    if (dents == NULL) return SYSCALL_FAULT_(EINVAL);
    page_directory_t *pagedir = current_user_pagedir();
    if (!user_range_mapped(pagedir, dents, size)) return SYSCALL_FAULT_(EFAULT);

    fd_file_handle *handle =  (fd_file_handle*)queue_get(get_current_task()->parent_group->file_open, fd);
    if (unlikely(handle == NULL)) { return SYSCALL_FAULT_(EBADF); }
    if (handle->node->type != file_dir) { return SYSCALL_FAULT_(ENOTDIR); }
    size_t   child_count   = (uint64_t)list_length(handle->node->child);
    size_t   max_dents_num = size / sizeof(struct dirent);
    size_t   read_count    = 0;
    uint64_t offset        = 0;
    vfs_node_t child_node;
    vfs_child_lock();
    list_foreach(handle->node->child, i)
    {
        if (offset < handle->offset) { goto next; }
        if (handle->offset >= (child_count * sizeof(struct dirent))) { break; }
        if (read_count >= max_dents_num) { break; }
        child_node = (vfs_node_t)i->data;
        struct dirent kd;
        memset(&kd, 0, sizeof(kd));
        kd.d_ino    = (long)child_node->inode;
        kd.d_off    = (long)handle->offset;
        kd.d_reclen = sizeof(struct dirent);
        kd.d_type = dirent_type_from_node(child_node);
        strncpy(kd.d_name, child_node->name, sizeof(kd.d_name) - 1);
        if (!copy_to_user_pagedir(pagedir, &dents[read_count], &kd, sizeof(kd)))
        {
            vfs_child_unlock();
            return read_count > 0 ? read_count * sizeof(struct dirent) : SYSCALL_FAULT_(EFAULT);
        }
        handle->offset += sizeof(struct dirent);
        read_count++;
    next:
        offset += sizeof(struct dirent);
    }
    vfs_child_unlock();
    return read_count * sizeof(struct dirent);
}

struct linux_dirent64 {
    uint64_t       d_ino;
    int64_t        d_off;
    unsigned short d_reclen;
    unsigned char  d_type;
    char           d_name[1];
} __attribute__((packed));

sys_(getdents64, int fd, void *dents, size_t size)
{
    if (size == 0) return 0;
    if (dents == NULL) return SYSCALL_FAULT_(EINVAL);
    page_directory_t *pagedir = current_user_pagedir();
    if (!user_range_mapped(pagedir, dents, size)) return SYSCALL_FAULT_(EFAULT);

    fd_file_handle *handle = (fd_file_handle *)queue_get(get_current_task()->parent_group->file_open, fd);
    if (unlikely(handle == NULL)) { return SYSCALL_FAULT_(EBADF); }
    if (handle->node->type != file_dir) { return SYSCALL_FAULT_(ENOTDIR); }

    size_t written = 0;
    size_t offset = 0;
    vfs_child_lock();
    list_foreach(handle->node->child, i)
    {
        vfs_node_t child_node = (vfs_node_t)i->data;
        size_t name_len = strlen(child_node->name);
        if (name_len > 255) name_len = 255;
        size_t reclen = offsetof(struct linux_dirent64, d_name) + name_len + 1;
        uint64_t aligned_reclen = 0;
        if (!align_up_u64(reclen, 8, &aligned_reclen) || aligned_reclen > sizeof(struct dirent))
        {
            vfs_child_unlock();
            return written > 0 ? written : SYSCALL_FAULT_(EINVAL);
        }
        reclen = (size_t)aligned_reclen;

        if (offset < handle->offset)
        {
            offset += reclen;
            continue;
        }
        if (written + reclen > size)
        {
            vfs_child_unlock();
            return written > 0 ? written : SYSCALL_FAULT_(EINVAL);
        }

        char record[sizeof(struct dirent)];
        memset(record, 0, sizeof(record));
        struct linux_dirent64 *kd = (struct linux_dirent64 *)record;
        kd->d_ino = (uint64_t)child_node->inode;
        kd->d_off = (int64_t)(offset + reclen);
        kd->d_reclen = (unsigned short)reclen;
        kd->d_type = dirent_type_from_node(child_node);
        strncpy(kd->d_name, child_node->name, name_len);

        if (!copy_to_user_pagedir(pagedir, (uint8_t *)dents + written, record, reclen))
        {
            vfs_child_unlock();
            return written > 0 ? written : SYSCALL_FAULT_(EFAULT);
        }
        written += reclen;
        offset += reclen;
        handle->offset = offset;
    }
    vfs_child_unlock();
    return written;
}

sys_(newfstatat, int dirfd, char *pathname, struct stat *buf, uint64_t flags)
{
    if (buf == NULL) return SYSCALL_FAULT_(EINVAL);
    char *resolved = NULL;
    int path_ret = resolve_user_path_at(dirfd, pathname, &resolved);
    if (path_ret < 0) return SYSCALL_FAULT_((int)-path_ret);

    struct stat kstat;
    uint64_t ret = stat_kernel_path(resolved, &kstat);
    if (busybox_debug_current_linux_abi())
    {
        write_serial_fmt("[busybox-debug] newfstatat dirfd=%d path=%s flags=0x%llx ret=%lld\n",
                         dirfd,
                         resolved,
                         (unsigned long long)flags,
                         (long long)ret);
    }
    free(resolved);
    if ((int64_t)ret < 0) return ret;
    return copy_stat_to_user(buf, &kstat);
}

sys_(statx, int dirfd, char *pathname, uint64_t flags, uint64_t mask, struct statx *buff)
{
    if (pathname == NULL || buff == NULL) return SYSCALL_FAULT_(EINVAL);

    struct stat simple;
    char *resolved = NULL;
    int path_ret = resolve_user_path_at(dirfd, pathname, &resolved);
    if (path_ret < 0) return SYSCALL_FAULT_((int)-path_ret);

    uint64_t ret = stat_kernel_path(resolved, &simple);
    free(resolved);
    if ((int64_t)ret < 0) return ret;

    struct statx kstatx;
    memset(&kstatx, 0, sizeof(kstatx));
    kstatx.stx_mask            = mask;
    kstatx.stx_blksize         = simple.st_blksize;
    kstatx.stx_attributes      = 0;
    kstatx.stx_nlink           = simple.st_nlink;
    kstatx.stx_uid             = simple.st_uid;
    kstatx.stx_gid             = simple.st_gid;
    kstatx.stx_mode            = simple.st_mode;
    kstatx.stx_ino             = simple.st_ino;
    kstatx.stx_size            = simple.st_size;
    kstatx.stx_blocks          = simple.st_blocks;
    kstatx.stx_attributes_mask = 0;

    kstatx.stx_atime.tv_sec  = (long)simple.st_atim.tv_sec;
    kstatx.stx_atime.tv_nsec = simple.st_atim.tv_nsec;

    kstatx.stx_btime.tv_sec  = (long)simple.st_ctim.tv_sec;
    kstatx.stx_btime.tv_nsec = simple.st_ctim.tv_nsec;

    kstatx.stx_ctime.tv_sec  = (long)simple.st_ctim.tv_sec;
    kstatx.stx_ctime.tv_nsec = simple.st_ctim.tv_nsec;

    kstatx.stx_mtime.tv_sec  = (long)simple.st_mtim.tv_sec;
    kstatx.stx_mtime.tv_nsec = simple.st_mtim.tv_nsec;

    if (!copy_to_user_pagedir(current_user_pagedir(), buff, &kstatx, sizeof(kstatx))) return SYSCALL_FAULT_(EFAULT);
    return 0;
}

sys_(pipe2, int *pipefd, uint64_t flags)
{
    /* fs/pipefs.c */
    extern vfs_node_t pipefs_root;
    extern int        pipefd_id;
    extern int        pipefs_id;

    if (pipefs_root == NULL) return SYSCALL_FAULT_(ENOSYS);
    if (pipefd == NULL) return SYSCALL_FAULT_(EINVAL);
    bool kernel_pipefd = regs == NULL;
    if (!kernel_pipefd && !user_range_mapped(current_user_pagedir(), pipefd, sizeof(int) * 2))
        return SYSCALL_FAULT_(EFAULT);

    char buf[16];
    sprintf(buf, "pipe%d", pipefd_id++);

    vfs_node_t node_input = vfs_node_alloc(pipefs_root, buf);
    node_input->type      = file_pipe;
    node_input->fsid      = pipefs_id;
    node_input->refcount++;
    pipefs_root->mode = 0700;

    sprintf(buf, "pipe%d", pipefd_id++);
    vfs_node_t node_output = vfs_node_alloc(pipefs_root, buf);
    node_output->type      = file_pipe;
    node_output->fsid      = pipefs_id;
    node_output->refcount++;
    pipefs_root->mode = 0700;

    pipe_info_t *info = (pipe_info_t *)malloc(sizeof(pipe_info_t));
    memset(info, 0, sizeof(pipe_info_t));
    info->buf       = (char*)calloc(1, PIPE_BUFF);
    info->read_fds  = 1;
    info->write_fds = 1;
    info->ptr       = 0;
    info->lock      = SPIN_INIT;

    pipe_specific_t *read_spec = (pipe_specific_t *)malloc(sizeof(pipe_specific_t));
    read_spec->write           = false;
    read_spec->info            = info;
    read_spec->node            = node_input;

    pipe_specific_t *write_spec = (pipe_specific_t *)malloc(sizeof(pipe_specific_t));
    write_spec->write           = true;
    write_spec->info            = info;
    write_spec->node            = node_output;

    node_input->handle  = read_spec;
    node_output->handle = write_spec;

    lock_queue     *queue     = get_current_task()->parent_group->file_open;
    fd_file_handle *handle_in = (fd_file_handle*)malloc(sizeof(fd_file_handle));
    handle_in->node           = node_input;
    handle_in->offset         = 0;
    handle_in->flags          = flags;
    handle_in->fd             = queue_enqueue(queue, handle_in);

    fd_file_handle *handle_out = (fd_file_handle*)malloc(sizeof(fd_file_handle));
    handle_out->node           = node_output;
    handle_out->offset         = 0;
    handle_out->flags          = flags;
    handle_out->fd             = queue_enqueue(queue, handle_out);

    int kpipefd[2] = {(int)handle_in->fd, (int)handle_out->fd};
    if (kernel_pipefd)
    {
        memcpy(pipefd, kpipefd, sizeof(kpipefd));
    }
    else if (!copy_to_user_pagedir(current_user_pagedir(), pipefd, kpipefd, sizeof(kpipefd)))
    {
        return SYSCALL_FAULT_(EFAULT);
    }

    return 0;
}

sys_(pipe, int *pipefd)
{
    return sys_pipe2(pipefd, 0, 0, 0, 0, 0, regs);
}

sys_(unlink)
{
    char *name = (char *)arg0;
    if (name == NULL) return SYSCALL_FAULT_(EINVAL);
    char *user_path = NULL;
    int copy_ret = copy_string_from_user(&user_path, name, USER_PATH_MAX);
    if (copy_ret < 0) return SYSCALL_FAULT_((int)-copy_ret);

    char      *npath = vfs_cwd_path_build(user_path);
    free(user_path);
    if (npath == NULL) return SYSCALL_FAULT_(ENOMEM);
    vfs_node_t node  = vfs_open(npath);
    if (node == NULL) { free(npath); return SYSCALL_FAULT_(ENOENT); }
    if (node->type != file_none && node->type != file_symlink)
    {
        vfs_close(node);
        free(npath);
        return SYSCALL_FAULT_(ENOTDIR);
    }
    size_t ret;
    if (node->refcount > 1)
    {
        node->refcount--;
        free(npath);
        return 0;
    }
    else
    {
        ret = vfs_delete(node) == VFS_STATUS_SUCCESS ? 0 : SYSCALL_FAULT_(ENOENT);
    }
    vfs_close(node);
    free(npath);
    return ret;
}

sys_(rmdir)
{
    char *name = (char *)arg0;
    if (name == NULL) return SYSCALL_FAULT_(EINVAL);
    char *user_path = NULL;
    int copy_ret = copy_string_from_user(&user_path, name, USER_PATH_MAX);
    if (copy_ret < 0) return SYSCALL_FAULT_((int)-copy_ret);

    char      *n_name = vfs_cwd_path_build(user_path);
    free(user_path);
    if (n_name == NULL) return SYSCALL_FAULT_(ENOMEM);
    vfs_node_t node   = vfs_open(n_name);
    if (node == NULL) { free(n_name); return SYSCALL_FAULT_(ENOENT); }
    if (node->type != file_dir)
    {
        vfs_close(node);
        free(n_name);
        return SYSCALL_FAULT_(ENOTDIR);
    }
    size_t ret;
    if (node->refcount > 1)
    {
        node->refcount--;
        ret = 0;
    }
    else
    {
        ret = vfs_delete(node);
    }
    free(n_name);
    return ret;
}

sys_(unlinkat)
{
    int   dirfd = (int)arg0;
    char *name  = (char *)arg1;
    if (name == NULL) return SYSCALL_FAULT_(EINVAL);
    char *path = NULL;
    int path_ret = resolve_user_path_at(dirfd, name, &path);
    if (path_ret < 0) return SYSCALL_FAULT_((int)-path_ret);
    if (!path) return -ENOENT;

    vfs_node_t node = vfs_open(path);
    if (node == NULL)
    {
        free(path);
        return SYSCALL_FAULT_(ENOENT);
    }
    if (node->type != file_none && node->type != file_symlink)
    {
        vfs_close(node);
        free(path);
        return SYSCALL_FAULT_(ENOTDIR);
    }

    uint64_t ret;
    if (node->refcount > 1)
    {
        node->refcount--;
        ret = 0;
    }
    else
    {
        ret = vfs_delete(node) == VFS_STATUS_SUCCESS ? 0 : SYSCALL_FAULT_(ENOENT);
    }

    vfs_close(node);
    free(path);
    return ret;
}

sys_(access, char *filename)
{
    if (filename == NULL) return SYSCALL_FAULT_(EINVAL);
    char *user_path = NULL;
    int ret = copy_string_from_user(&user_path, filename, USER_PATH_MAX);
    if (ret < 0) return SYSCALL_FAULT_((int)-ret);

    char *path = vfs_cwd_path_build(user_path);
    free(user_path);
    if (path == NULL) return SYSCALL_FAULT_(ENOMEM);

    struct stat buf;
    uint64_t stat_ret = stat_kernel_path(path, &buf);
    if (busybox_debug_current_linux_abi())
    {
        write_serial_fmt("[busybox-debug] access path=%s ret=%lld\n", path, (long long)stat_ret);
    }
    free(path);
    return stat_ret;
}

sys_(faccessat)
{
    int      dirfd    = arg0;
    char    *pathname = (char *)arg1;
    uint64_t mode     = arg2;
    (void)mode;
    if (pathname == NULL) return SYSCALL_FAULT_(EINVAL);

    char *resolved = NULL;
    int path_ret = resolve_user_path_at(dirfd, pathname, &resolved);
    if (path_ret < 0) return SYSCALL_FAULT_((int)-path_ret);

    struct stat buf;
    uint64_t ret = stat_kernel_path(resolved, &buf);
    if (busybox_debug_current_linux_abi())
    {
        write_serial_fmt("[busybox-debug] faccessat dirfd=%d path=%s mode=0x%llx ret=%lld\n",
                         dirfd,
                         resolved,
                         (unsigned long long)mode,
                         (long long)ret);
    }

    free(resolved);

    return ret;
}

sys_(faccessat2)
{
    uint64_t dirfd    = arg0;
    char    *pathname = (char *)arg1;
    uint64_t mode     = arg2;
    uint64_t flag     = arg3;
    (void)mode;
    (void)flag;
    if (pathname == NULL) return SYSCALL_FAULT_(EINVAL);

    char *resolved = NULL;
    int path_ret = resolve_user_path_at(dirfd, pathname, &resolved);
    if (path_ret < 0) return SYSCALL_FAULT_((int)-path_ret);

    struct stat buf;
    uint64_t ret = stat_kernel_path(resolved, &buf);
    if (busybox_debug_current_linux_abi())
    {
        write_serial_fmt("[busybox-debug] faccessat2 dirfd=%llu path=%s mode=0x%llx flags=0x%llx ret=%lld\n",
                         (unsigned long long)dirfd,
                         resolved,
                         (unsigned long long)mode,
                         (unsigned long long)flag,
                         (long long)ret);
    }

    free(resolved);

    return ret;
}

sys_(openat)
{
    int      dirfd = arg0;
    char    *name  = (char *)arg1;
    uint64_t flags = arg2;
    uint64_t mode  = arg3;

    char *path = NULL;
    int path_ret = resolve_user_path_at(dirfd, name, &path);
    if (path_ret < 0) return SYSCALL_FAULT_((int)-path_ret);

    uint64_t ret = open_kernel_path(path, flags, mode);

    free(path);

    return ret;
}



sys_(copy_file_range, int fd_in, uint64_t *off_in, int fd_out, uint64_t *off_out, size_t len, uint64_t flags)
{
    if (flags != 0) { return SYSCALL_FAULT_(EINVAL); }
    fd_file_handle *src_handle =  (fd_file_handle*)queue_get(get_current_task()->parent_group->file_open, fd_in);
    fd_file_handle *dst_handle =  (fd_file_handle*)queue_get(get_current_task()->parent_group->file_open, fd_out);
    if (src_handle == NULL || dst_handle == NULL) { return SYSCALL_FAULT_(EBADF); }
    if (dst_handle->offset >= dst_handle->node->size && dst_handle->node->size > 0) return 0;
    uint64_t src_offset = off_in ? *off_in : src_handle->offset;
    uint64_t dst_offset = off_out ? *off_out : dst_handle->offset;

    uint64_t length     = src_handle->node->size > len ? len : src_handle->node->size;
    uint8_t *buffer     = (uint8_t *)malloc(length);
    size_t   copy_total = 0;
    if (vfs_read(src_handle->node, buffer, src_offset, length) == (size_t)VFS_STATUS_FAILED) { goto errno_; }
    copy_total = vfs_write(dst_handle->node, buffer, dst_offset, length);
    if (copy_total == (size_t)VFS_STATUS_FAILED) { goto errno_; }
    vfs_update(dst_handle->node);
    free(buffer);
    dst_handle->offset += copy_total;
    return copy_total;
errno_:
    free(buffer);
    return SYSCALL_FAULT_(EFAULT);
}

sys_(pread, int fd, uint8_t *buffer)
{
    sys_lseek(fd, arg3, SEEK_SET, 0, 0, 0, regs);
    return sys_read(fd, buffer, arg2, 0, 0, 0, regs);
}

sys_(pwrite, int fd, uint8_t *buffer)
{
    sys_lseek(fd, arg3, SEEK_SET, 0, 0, 0, regs);
    return sys_write(fd, buffer, arg2, 0, 0, 0, regs);
}

sys_(mount, char *dev_name, char *dir_name, char *type, uint64_t flags, void *data)
{
    if (dir_name == NULL) return SYSCALL_FAULT_(EINVAL);

    char *udir_name = NULL;
    int copy_ret = copy_string_from_user(&udir_name, dir_name, USER_PATH_MAX);
    if (copy_ret < 0) return SYSCALL_FAULT_((int)-copy_ret);

    char      *ndir_name = vfs_cwd_path_build(udir_name);
    free(udir_name);
    if (ndir_name == NULL) return SYSCALL_FAULT_(ENOMEM);
    vfs_node_t dir       = vfs_open((const char *)ndir_name);
    if (!dir)
    {
        free(ndir_name);
        return SYSCALL_FAULT_(ENOENT);
    }

    if (flags & MS_MOVE)
    {
        if (flags & (MS_REMOUNT | MS_BIND))
        {
            free(ndir_name);
            return SYSCALL_FAULT_(EINVAL);
        }
        if (dev_name == NULL) { free(ndir_name); return SYSCALL_FAULT_(EINVAL); }
        char *udev_name = NULL;
        copy_ret = copy_string_from_user(&udev_name, dev_name, USER_PATH_MAX);
        if (copy_ret < 0) { free(ndir_name); return SYSCALL_FAULT_((int)-copy_ret); }
        char      *old_root_p = vfs_cwd_path_build(udev_name);
        free(udev_name);
        if (old_root_p == NULL) { free(ndir_name); return SYSCALL_FAULT_(ENOMEM); }
        vfs_node_t old_root   = vfs_open(old_root_p);
        free(old_root_p);
        if (old_root == NULL || !old_root->is_mount) return SYSCALL_FAULT_(EINVAL);
        if (dir != rootdir) list_append(dir->parent->child, old_root);
        char *nb       = old_root->name;
        old_root->name = dir->name;
        dir->name      = nb;
        list_append(old_root->parent->child, dir);

        list_delete(old_root->parent->child, old_root);
        if (dir != rootdir)
            list_delete(dir->parent->child, dir);
        else
            rootdir = old_root;

        vfs_node_t parent = dir->parent;
        dir->parent       = old_root->parent;
        old_root->parent  = parent;

        vfs_close(old_root);
        vfs_close(dir);
        return 0;
    }

    if (type == NULL) return SYSCALL_FAULT_(EINVAL);

    char            *ndev_name = NULL;
    char            *udev_name = NULL;
    char            *ktype = NULL;
    copy_ret = copy_string_from_user(&ktype, type, USER_PATH_MAX);
    if (copy_ret < 0) { free(ndir_name); return SYSCALL_FAULT_((int)-copy_ret); }
    vfs_filesystem_t filesystem = get_filesystem(ktype);
    free(ktype);
    if (filesystem == NULL) { free(ndir_name); return SYSCALL_FAULT_(EINVAL); }
    if (filesystem->id != 0)
    {
        ndev_name = (char *)filesystem->id;
        goto mount;
    }

    if (dev_name == NULL) { free(ndir_name); return SYSCALL_FAULT_(EINVAL); }
    copy_ret = copy_string_from_user(&udev_name, dev_name, USER_PATH_MAX);
    if (copy_ret < 0) { free(ndir_name); return SYSCALL_FAULT_((int)-copy_ret); }
    ndev_name = vfs_cwd_path_build(udev_name);
    free(udev_name);
    if (ndev_name == NULL) { free(ndir_name); return SYSCALL_FAULT_(ENOMEM); }
mount:
    if (vfs_mount((const char *)ndev_name, dir) == VFS_STATUS_FAILED)
    {
        free(ndir_name);
        free(ndev_name);
        return SYSCALL_FAULT_(ENOENT);
    }
    free(ndir_name);
    if (filesystem->id == 0) free(ndev_name);
    return 0;
}

sys_(setpgid, int pid, int pgid)
{
    pcb_t process = pid == 0 ? get_current_task()->parent_group : found_pcb(pid);
    if (process == NULL || process->status == DEATH) { return SYSCALL_FAULT_(ESRCH); }
    if (pgid == 0) { pgid = process->pid; }
    process->pid = pgid;
    return 0;
}

sys_(getpgid)
{
    size_t pid     = arg0;
    pcb_t  process = pid == 0 ? get_current_task()->parent_group : found_pcb(pid);
    if (process == NULL || process->status == DEATH) { return SYSCALL_FAULT_(ESRCH); }
    return process->pid;
}


sys_(getgroups, int count, int *gid_list)
{
    if (count < 0) return SYSCALL_FAULT_(EINVAL);
    if (count > 0)
    {
        if (gid_list == NULL) return SYSCALL_FAULT_(EINVAL);
        int gid = (int)current_gid();
        if (!copy_to_user_pagedir(current_user_pagedir(), gid_list, &gid, sizeof(gid)))
            return SYSCALL_FAULT_(EFAULT);
        return 1;
    }
    return 1;
}
char *get_parent_path(const char *path) {
    if (!path || !*path) return strdup(".");

    char *copy = strdup(path);
    if (!copy) return NULL;

    char *last_slash = strrchr(copy, '/');

    if (last_slash && last_slash != copy) {
        *last_slash = '\0';
    } else if (last_slash == copy) {
        copy[1] = '\0';
    } else {
        free(copy);
        return strdup(".");
    }

    return copy;
}


sys_(rename, char *oldpath, char *newpath)
{
    if (!oldpath || !newpath) return SYSCALL_FAULT_(EINVAL);
    char *uoldpath = NULL;
    char *unewpath = NULL;
    int copy_ret = copy_string_from_user(&uoldpath, oldpath, USER_PATH_MAX);
    if (copy_ret < 0) return SYSCALL_FAULT_((int)-copy_ret);
    copy_ret = copy_string_from_user(&unewpath, newpath, USER_PATH_MAX);
    if (copy_ret < 0) { free(uoldpath); return SYSCALL_FAULT_((int)-copy_ret); }

    char      *noldpath = vfs_cwd_path_build(uoldpath);
    char      *nnewpath = vfs_cwd_path_build(unewpath);
    free(uoldpath);
    free(unewpath);
    if (noldpath == NULL || nnewpath == NULL)
    {
        free(noldpath);
        free(nnewpath);
        return SYSCALL_FAULT_(ENOMEM);
    }
    vfs_node_t oldnode  = vfs_open(noldpath);
    if (!oldnode)
    {
        free(noldpath);
        free(nnewpath);
        return SYSCALL_FAULT_(ENOENT);
    }
    vfs_node_t newnode = vfs_open(nnewpath);
    if (newnode) { vfs_delete(newnode); }
    size_t     ret    = vfs_rename(oldnode, nnewpath) == VFS_STATUS_SUCCESS ? 0 : SYSCALL_FAULT_(ENOENT);
    char      *parent = get_parent_path(nnewpath);
    vfs_node_t parent_dir = vfs_open(parent);
    if (!parent_dir)
    {
        free(parent);
        ret = SYSCALL_FAULT_(ENOENT);
        goto end_rename;
    }
    vfs_close(parent_dir); // 更新父目录的信息
    free(parent);
end_rename:
    free(noldpath);
    free(nnewpath);
    return ret;
}

sys_(symlink, char *name, char *nw)
{
    if (name == NULL || nw == NULL) return SYSCALL_FAULT_(EINVAL);
    char *kname = NULL;
    char *knw = NULL;
    int copy_ret = copy_string_from_user(&kname, name, USER_PATH_MAX);
    if (copy_ret < 0) return SYSCALL_FAULT_((int)-copy_ret);
    copy_ret = copy_string_from_user(&knw, nw, USER_PATH_MAX);
    if (copy_ret < 0) { free(kname); return SYSCALL_FAULT_((int)-copy_ret); }
    errno_t ret = vfs_symlink(kname, knw);
    free(kname);
    free(knw);
    return ret;
}

sys_(link, char *name, char *nw)
{
    if (name == NULL || nw == NULL) return SYSCALL_FAULT_(EINVAL);
    char *kname = NULL;
    char *knw = NULL;
    int copy_ret = copy_string_from_user(&kname, name, USER_PATH_MAX);
    if (copy_ret < 0) return SYSCALL_FAULT_((int)-copy_ret);
    copy_ret = copy_string_from_user(&knw, nw, USER_PATH_MAX);
    if (copy_ret < 0) { free(kname); return SYSCALL_FAULT_((int)-copy_ret); }
    errno_t ret = vfs_link(kname, knw);
    free(kname);
    free(knw);

    return ret;
}

sys_(umount2)
{
    char *user_path = NULL;
    int ret = copy_string_from_user(&user_path, (char *)arg0, USER_PATH_MAX);
    if (ret < 0) return SYSCALL_FAULT_((int)-ret);

    char *path = normalize_path(user_path);
    free(user_path);
    int flags = arg1;
    (void)flags;
    if (path == NULL) return SYSCALL_FAULT_(ENOMEM);

    uint64_t status = vfs_unmount(path);
    if (status == VFS_STATUS_FAILED) status = SYSCALL_FAULT_(EBUSY);
    free(path);
    return status;
}

sys_(readlink)
{
    char    *path = (char *)arg0;
    char    *buf  = (char *)arg1;
    uint64_t size = arg2;
    if (path == NULL || buf == NULL || size == 0) { return SYSCALL_FAULT_(EINVAL); }

    char *kpath = NULL;
    int ret = copy_string_from_user(&kpath, path, USER_PATH_MAX);
    if (ret < 0) return SYSCALL_FAULT_((int)-ret);

    vfs_node_t node = vfs_open(kpath);
    free(kpath);
    if (node == NULL) { return SYSCALL_FAULT_(ENOENT); }
    if (!node->linkto)
    {
        vfs_close(node);
        return SYSCALL_FAULT_(EINVAL);
    }

    size_t copy_size = MIN((size_t)size, USER_PATH_MAX);
    char *kbuf = (char *)malloc(copy_size);
    if (kbuf == NULL)
    {
        vfs_close(node);
        return SYSCALL_FAULT_(ENOMEM);
    }

    size_t read = vfs_readlink(node, kbuf, copy_size);
    vfs_close(node);
    if (read == (size_t)VFS_STATUS_FAILED)
    {
        free(kbuf);
        return SYSCALL_FAULT_(EIO);
    }
    if (!copy_to_user_pagedir(current_user_pagedir(), buf, kbuf, read))
    {
        free(kbuf);
        return SYSCALL_FAULT_(EFAULT);
    }
    free(kbuf);
    return read;
}


sys_(getuid)
{
    return current_uid();
}

sys_(getgid)
{
    return current_gid();
}

sys_(setuid, uint32_t uid)
{
    uint32_t euid = current_uid();
    if (uid == euid || euid == 0) return 0;
    return SYSCALL_FAULT_(EPERM);
}

sys_(setgid, uint32_t gid)
{
    uint32_t egid = current_gid();
    if (gid == egid || current_uid() == 0) return 0;
    return SYSCALL_FAULT_(EPERM);
}

sys_(geteuid)
{
    return current_uid();
}

sys_(getegid)
{
    return current_gid();
}

sys_(sysinfo, struct sysinfo *info)
{
    if (info == NULL) return SYSCALL_FAULT_(EINVAL);

    struct sysinfo kinfo;
    memset(&kinfo, 0, sizeof(kinfo));
    kinfo.uptime   = (int64_t)(nanoTime() / 1000000000ULL);
    kinfo.totalram = frame_allocator.origin_frames * PAGE_SIZE;
    kinfo.freeram  = frame_allocator.usable_frames * PAGE_SIZE;
    kinfo.procs    = pcb_group_queue != NULL && pcb_group_queue->size > UINT16_MAX ? UINT16_MAX :
                     pcb_group_queue != NULL                                      ? (uint16_t)pcb_group_queue->size
                                                                                  : 0;
    kinfo.mem_unit = 1;

    if (!copy_to_user_pagedir(current_user_pagedir(), info, &kinfo, sizeof(kinfo)))
        return SYSCALL_FAULT_(EFAULT);
    return 0;
}

sys_(chmod, char *path, uint64_t mode)
{
    if (path == NULL) return SYSCALL_FAULT_(EINVAL);
    char *resolved = NULL;
    int path_ret = resolve_user_path_at(AT_FDCWD, path, &resolved);
    if (path_ret < 0) return SYSCALL_FAULT_((int)-path_ret);

    vfs_node_t node = vfs_open(resolved);
    free(resolved);
    if (node == NULL) return SYSCALL_FAULT_(ENOENT);

    node->mode = (uint16_t)((node->mode & ~07777) | (mode & 07777));
    vfs_close(node);
    return 0;
}

sys_(fchmod, int fd, uint64_t mode)
{
    if (fd < 0) return SYSCALL_FAULT_(EBADF);
    fd_file_handle *handle = (fd_file_handle *)queue_get(get_current_task()->parent_group->file_open, fd);
    if (handle == NULL || handle->node == NULL) return SYSCALL_FAULT_(EBADF);
    handle->node->mode = (uint16_t)((handle->node->mode & ~07777) | (mode & 07777));
    return 0;
}

sys_(fsopen, char *fsname, uint32_t flags)
{

    vfs_filesystem_t filesystem = NULL;
    vfs_filesystem_t pos, n;
    llist_for_each(pos, n, &fs_metadata_list, node)
    {
        if (strcmp(pos->name, fsname) != 0)
        {
            filesystem = pos;
            break;
        }
    }

    if (filesystem == NULL) return SYSCALL_FAULT_(ENOENT);

    vfs_node_t node = vfs_node_alloc(NULL, NULL);
    node->type      = file_none;
    node->handle    = filesystem;
    node->refcount++;
    node->size = 0;

    fd_file_handle *handle =  (fd_file_handle*)calloc(1, sizeof(fd_file_handle));
    handle->node           = node;
    handle->fd             = queue_enqueue(get_current_task()->parent_group->file_open, handle);
    handle->flags          = flags;
    return handle->fd;
}

sys_(getppid)
{
    return get_current_task()->parent_group->parent_task->pid;
}

sys_(mincore, uint64_t addr, uint64_t size, uint64_t vec)
{
    if (size == 0) { return 0; }
    if (check_user_overflow(addr, size)) { return SYSCALL_FAULT_(EFAULT); }

    uint64_t start_page = addr & (~(PAGE_SIZE - 1));
    uint64_t end_page   = (addr + size - 1) & (~(PAGE_SIZE - 1));
    if (end_page < start_page) return SYSCALL_FAULT_(EINVAL);
    uint64_t num_pages  = ((end_page - start_page) / PAGE_SIZE) + 1;

    page_directory_t *pagedir = current_user_pagedir();
    if (!user_range_mapped(pagedir, (void *)vec, num_pages)) { return SYSCALL_FAULT_(EFAULT); }

    spin_lock(&mm_op_lock);
    uint64_t current_addr = start_page;

    for (uint64_t i = 0; i < num_pages; i++)
    {
        uint64_t phys_addr = page_virt_to_phys(current_addr);

        uint8_t resident = (phys_addr != 0) ? 1 : 0;

        if (!copy_to_user_pagedir(pagedir, (void *)(vec + i), &resident, sizeof(uint8_t)))
        {
            spin_unlock(&mm_op_lock);
            return SYSCALL_FAULT_(EFAULT);
        }

        current_addr += PAGE_SIZE;
    }

    spin_unlock(&mm_op_lock);
    return 0;
}

sys_(pivot_root, char *new_root, char *put_old)
{
    //TODO 未经测试和验证的实现
    vfs_node_t nroot = vfs_open(new_root);
    vfs_node_t src_root;
    if (nroot == NULL) return SYSCALL_FAULT_(ENOENT);
    vfs_node_t oroot;
    if (!nroot->is_mount)
    {
        vfs_close(nroot);
        goto error_pr;
    }
    oroot = vfs_open(put_old);
    if (oroot == NULL) return SYSCALL_FAULT_(ENOENT);
    if (!(oroot->type & file_dir)) goto error_pr;
    if (list_length(oroot->child) > 0) goto error_pr;
    if (oroot->root != nroot) goto error_pr;
    src_root = get_rootdir();
    set_rootdir(nroot);
    oroot->handle = src_root->handle;
    oroot->child  = src_root->child;
    return 0;
error_pr:
    return SYSCALL_FAULT_(EINVAL);
}


sys_(statfs, char *path, struct statfs *buf)
{
    if (path == NULL || buf == NULL) return SYSCALL_FAULT_(EINVAL);

    char *kpath = NULL;
    int ret = copy_string_from_user(&kpath, path, USER_PATH_MAX);
    if (ret < 0) return SYSCALL_FAULT_((int)-ret);

    vfs_node_t node = vfs_open(kpath);
    free(kpath);
    if (node == NULL) return SYSCALL_FAULT_(ENOENT);
    vfs_filesystem_t filesystem = get_filesystem_node(node);
    if (filesystem == NULL)
    {
        vfs_close(node);
        return SYSCALL_FAULT_(EINVAL);
    }

    struct statfs kbuf;
    memset(&kbuf, 0, sizeof(kbuf));
    kbuf.f_type = filesystem->magic;
    vfs_close(node);
    if (!copy_to_user_pagedir(current_user_pagedir(), buf, &kbuf, sizeof(kbuf))) return SYSCALL_FAULT_(EFAULT);
    return 0;
}


sys_(sendfile, int out_fd, int in_fd, uint64_t *offset_ptr, size_t count)
{
    pcb_t           process    = get_current_task()->parent_group;
    fd_file_handle *out_handle =  (fd_file_handle*)queue_get(process->file_open, out_fd);
    fd_file_handle *in_handle  =  (fd_file_handle*)queue_get(process->file_open, in_fd);
    if (out_handle == NULL || in_handle == NULL) return SYSCALL_FAULT_(EBADF);

    uint64_t current_offset = offset_ptr == NULL ? in_handle->offset : *offset_ptr;
    size_t   total_sent     = 0;

    size_t remaining = count;

    char *buffer = (char *)malloc(SENDFILE_BUF_SIZE);
    if (buffer == NULL) { return SYSCALL_FAULT_(ENOMEM); }

    while (remaining > 0)
    {
        size_t bytes_to_read = remaining < SENDFILE_BUF_SIZE ? remaining : SENDFILE_BUF_SIZE;
        size_t bytes_read;
        size_t bytes_written;
        bytes_read = vfs_read(in_handle->node, buffer, current_offset, bytes_to_read);
        if (bytes_read <= 0)
        {
            if (bytes_read == (size_t)VFS_STATUS_FAILED && total_sent == 0)
            {
                free(buffer);
                return SYSCALL_FAULT_(EIO);
            }
            break;
        }
        bytes_written = vfs_write(out_handle->node, buffer, out_handle->offset, bytes_read);
        if (bytes_written == (size_t)VFS_STATUS_FAILED)
        {
            if (total_sent == 0)
            {
                free(buffer);
                return SYSCALL_FAULT_(EIO);
            }
            break;
        }
        if (bytes_written < bytes_read) { bytes_read = bytes_written; }
        current_offset     += bytes_read;
        out_handle->offset += bytes_read;
        total_sent         += bytes_read;
        remaining          -= bytes_read;
    }
    free(buffer);
    if (offset_ptr != NULL) { *offset_ptr = current_offset; }
    else
    {
        in_handle->offset = current_offset;
    }
    return total_sent;
}
