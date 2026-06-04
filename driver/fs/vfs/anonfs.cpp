#define ALL_IMPLEMENTATION

#include <errno.h>
#include <fs/vfs/anonfs.h>
#include <fs/vfs/vfs.h>
#include <krlibc.h>
#include <stdint.h>
#include <mm/hhdm.h>
#include <mm/page.h>
#include <mm/uaccess.h>
#include <proto.hpp>
#include <rtc.h>
#include <syscall/syscall.h>
#include <task/poll.h>
#include <task/pcb.h>

static int        anonfs_id           = 0;
static int        anonfs_next_inode   = 1;
static vfs_node_t anonfs_root         = NULL;
static spin_t     anonfs_alloc_lock   = SPIN_INIT;
static spin_t     anonfs_inotify_global_lock = SPIN_INIT;
static anonfs_handle_t *anonfs_inotify_handles[64];

static constexpr size_t ANONFS_PATH_MAX = 4096UL;

static int anonfs_dummy()
{
    return -ENOSYS;
}

static constexpr uint64_t ANONFS_U64_MAX = (uint64_t)-1;

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME         0
#define CLOCK_MONOTONIC        1
#define CLOCK_MONOTONIC_RAW    4
#define CLOCK_BOOTTIME         7
#endif

static size_t anonfs_bounded_strlen_user(const char *str, size_t max_len)
{
    if (str == NULL) return 0;
    size_t len = 0;
    while (len < max_len && str[len] != '\0')
        len++;
    return len;
}

static page_directory_t *anonfs_current_user_pagedir()
{
    tcb_t task = get_current_task();
    if (task == NULL || task->parent_group == NULL) return NULL;
    return task->parent_group->pagedir;
}

static int anonfs_copy_string_from_user(char **out, const char *src, size_t max_len)
{
    if (out == NULL) return -EINVAL;
    *out = NULL;
    if (src == NULL || max_len == 0) return -EFAULT;

    page_directory_t *pagedir = anonfs_current_user_pagedir();
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

static uint64_t anonfs_clock_now_ns(int clockid)
{
    switch (clockid)
    {
    case CLOCK_REALTIME: return realtime_ns();
    case CLOCK_MONOTONIC:
    case CLOCK_MONOTONIC_RAW:
    case CLOCK_BOOTTIME:
    default: return nanoTime();
    }
}

static bool anonfs_timerfd_refresh(anonfs_handle_t *handle)
{
    if (handle == NULL || handle->kind != ANONFS_TIMERFD) return false;
    anonfs_timerfd_t *timer = &handle->u.timerfd;

    spin_lock(&timer->lock);
    if (!timer->armed)
    {
        spin_unlock(&timer->lock);
        return timer->pending != 0;
    }

    uint64_t now = anonfs_clock_now_ns(timer->clockid);
    while (timer->armed && timer->next_expire_ns != 0 && now >= timer->next_expire_ns)
    {
        timer->pending++;
        if (timer->interval_ns == 0)
        {
            timer->armed = false;
            timer->next_expire_ns = 0;
            break;
        }
        timer->next_expire_ns += timer->interval_ns;
    }
    bool ready = timer->pending != 0;
    spin_unlock(&timer->lock);
    return ready;
}

static errno_t anonfs_mount(const char *handle, vfs_node_t node)
{
    if ((uint64_t)handle != TMPFS_REGISTER_ID + 1000) return VFS_STATUS_FAILED;
    node->fsid   = anonfs_id;
    node->type   = file_dir;
    node->handle = NULL;
    node->mode   = 0700;
    anonfs_root  = node;
    return VFS_STATUS_SUCCESS;
}

static void anonfs_open(void *parent, const char *name, vfs_node_t node)
{
    (void)parent;
    (void)name;
    if (node == anonfs_root)
    {
        node->type = file_dir;
        return;
    }
    if (node->handle == NULL) return;

    anonfs_handle_t *handle = (anonfs_handle_t *)node->handle;
    switch (handle->kind)
    {
    case ANONFS_EVENTFD:
        node->type = file_stream | file_anon;
        node->size = sizeof(uint64_t);
        break;
    case ANONFS_SIGNALFD:
        node->type = file_stream | file_anon;
        node->size = (uint64_t)-1;
        break;
    case ANONFS_TIMERFD:
        node->type = file_stream | file_anon;
        node->size = sizeof(uint64_t);
        break;
    case ANONFS_MEMFD:
        node->type = file_none | file_anon;
        node->size = handle->u.memfd.size;
        break;
    case ANONFS_INOTIFY:
        node->type = file_stream | file_anon;
        node->size = (uint64_t)-1;
        break;
    default:
        node->type = file_none;
        break;
    }
}

static errno_t anonfs_stat(void *file, vfs_node_t node)
{
    if (file == NULL || node == NULL) return -EINVAL;
    anonfs_handle_t *handle = (anonfs_handle_t *)file;
    node->fsid = anonfs_id;
    node->mode = 0600;

    switch (handle->kind)
    {
    case ANONFS_EVENTFD:
        node->type = file_stream | file_anon;
        node->size = sizeof(uint64_t);
        break;
    case ANONFS_SIGNALFD:
        node->type = file_stream | file_anon;
        node->size = (uint64_t)-1;
        break;
    case ANONFS_TIMERFD:
        node->type = file_stream | file_anon;
        node->size = sizeof(uint64_t);
        break;
    case ANONFS_MEMFD:
        node->type = file_none | file_anon;
        spin_lock(&handle->u.memfd.lock);
        node->size = handle->u.memfd.size;
        spin_unlock(&handle->u.memfd.lock);
        break;
    case ANONFS_INOTIFY:
        node->type = file_stream | file_anon;
        node->size = (uint64_t)-1;
        break;
    default:
        return -EINVAL;
    }
    return EOK;
}

static size_t anonfs_eventfd_read(anonfs_eventfd_t *efd, void *addr, size_t size)
{
    if (size < sizeof(uint64_t)) return VFS_STATUS_FAILED;
    uint64_t value = 0;
    bool ready = false;

    while (!ready)
    {
        spin_lock(&efd->lock);
        if (efd->counter != 0)
        {
            ready = true;
            if (efd->flags & EFD_SEMAPHORE)
            {
                value = 1;
                efd->counter--;
            }
            else
            {
                value = efd->counter;
                efd->counter = 0;
            }
        }
        spin_unlock(&efd->lock);
        if (ready) break;
        if (efd->flags & EFD_NONBLOCK) return (size_t)-EWOULDBLOCK;
        scheduler_sleep_ns(1000000ULL);
    }

    memcpy(addr, &value, sizeof(value));
    return sizeof(value);
}

static size_t anonfs_eventfd_write(anonfs_eventfd_t *efd, const void *addr, size_t size)
{
    if (size < sizeof(uint64_t)) return VFS_STATUS_FAILED;
    uint64_t value = 0;
    memcpy(&value, addr, sizeof(value));
    if (value == ANONFS_U64_MAX) return VFS_STATUS_FAILED;

    while (true)
    {
        spin_lock(&efd->lock);
        if (ANONFS_U64_MAX - 1 - efd->counter >= value)
        {
            efd->counter += value;
            spin_unlock(&efd->lock);
            return sizeof(value);
        }
        spin_unlock(&efd->lock);
        if (efd->flags & EFD_NONBLOCK) return (size_t)-EWOULDBLOCK;
        scheduler_sleep_ns(1000000ULL);
    }
}

static size_t anonfs_signalfd_read(anonfs_signalfd_t *sfd, void *addr, size_t size)
{
    if (size < sizeof(struct signalfd_siginfo)) return VFS_STATUS_FAILED;

    while (true)
    {
        spin_lock(&sfd->lock);
        if (sfd->count != 0)
        {
            struct signalfd_siginfo info = sfd->queue[sfd->head];
            sfd->head = (sfd->head + 1) % (sizeof(sfd->queue) / sizeof(sfd->queue[0]));
            sfd->count--;
            spin_unlock(&sfd->lock);
            memcpy(addr, &info, sizeof(info));
            return sizeof(info);
        }
        spin_unlock(&sfd->lock);
        if (sfd->flags & SFD_NONBLOCK) return (size_t)-EWOULDBLOCK;
        scheduler_sleep_ns(1000000ULL);
    }
}

static size_t anonfs_timerfd_read(anonfs_timerfd_t *timer, void *addr, size_t size)
{
    if (size < sizeof(uint64_t)) return VFS_STATUS_FAILED;

    while (true)
    {
        uint64_t value = 0;
        spin_lock(&timer->lock);
        if (timer->pending != 0)
        {
            value = timer->pending;
            timer->pending = 0;
            spin_unlock(&timer->lock);
            memcpy(addr, &value, sizeof(value));
            return sizeof(value);
        }
        spin_unlock(&timer->lock);
        if (timer->flags & TFD_NONBLOCK) return (size_t)-EWOULDBLOCK;
        scheduler_sleep_ns(1000000ULL);
        anonfs_timerfd_refresh((anonfs_handle_t *)((uintptr_t)timer - offsetof(anonfs_handle_t, u.timerfd)));
    }
}

static size_t anonfs_memfd_read(anonfs_memfd_t *memfd, void *addr, size_t offset, size_t size)
{
    spin_lock(&memfd->lock);
    if (offset >= memfd->size)
    {
        spin_unlock(&memfd->lock);
        return 0;
    }
    size_t available = memfd->size - offset;
    size_t copy = MIN(size, available);
    if (copy != 0) memcpy(addr, memfd->data + offset, copy);
    spin_unlock(&memfd->lock);
    return copy;
}

static bool anonfs_memfd_reserve(anonfs_memfd_t *memfd, size_t needed)
{
    if (needed <= memfd->capacity) return true;
    size_t new_capacity = memfd->capacity == 0 ? PAGE_SIZE : memfd->capacity;
    while (new_capacity < needed)
    {
        if (new_capacity > (size_t)-1 / 2) return false;
        new_capacity *= 2;
    }
    size_t old_pages = memfd->capacity == 0 ? 0 : (memfd->capacity + PAGE_SIZE - 1) / PAGE_SIZE;
    size_t new_pages = (new_capacity + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t new_phys = alloc_frames(new_pages);
    if (new_phys == 0) return false;
    uint8_t *new_data = (uint8_t *)phys_to_virt(new_phys);
    memset(new_data, 0, new_pages * PAGE_SIZE);
    if (memfd->data != NULL && memfd->size != 0) memcpy(new_data, memfd->data, memfd->size);
    if (memfd->phys != 0) free_frames(memfd->phys, old_pages);
    memfd->phys = new_phys;
    memfd->data = new_data;
    memfd->capacity = new_capacity;
    return true;
}

static size_t anonfs_memfd_write(anonfs_memfd_t *memfd, const void *addr, size_t offset, size_t size)
{
    if (size == 0) return 0;
    spin_lock(&memfd->lock);
    size_t end = 0;
    if (offset > (size_t)-1 - size)
    {
        spin_unlock(&memfd->lock);
        return VFS_STATUS_FAILED;
    }
    end = offset + size;
    if (!anonfs_memfd_reserve(memfd, end))
    {
        spin_unlock(&memfd->lock);
        return VFS_STATUS_FAILED;
    }
    memcpy(memfd->data + offset, addr, size);
    if (end > memfd->size) memfd->size = end;
    spin_unlock(&memfd->lock);
    return size;
}

static size_t anonfs_inotify_event_size(const anonfs_inotify_event_record_t *event)
{
    size_t name_len = event->name[0] == '\0' ? 0 : strlen(event->name) + 1;
    if (name_len != 0) name_len = (name_len + sizeof(uint32_t) - 1) & ~(sizeof(uint32_t) - 1);
    return sizeof(struct inotify_event) + name_len;
}

static size_t anonfs_inotify_read(anonfs_inotify_t *inotify, void *addr, size_t size)
{
    if (size < sizeof(struct inotify_event)) return (size_t)-EINVAL;

    while (true)
    {
        spin_lock(&inotify->lock);
        if (inotify->event_count != 0) break;
        spin_unlock(&inotify->lock);

        if (inotify->flags & IN_NONBLOCK) return (size_t)-EWOULDBLOCK;
        scheduler_sleep_ns(1000000ULL);
    }

    size_t written = 0;
    while (inotify->event_count != 0)
    {
        anonfs_inotify_event_record_t event = inotify->events[inotify->event_head];
        size_t event_size = anonfs_inotify_event_size(&event);
        if (written != 0 && written + event_size > size) break;
        if (event_size > size)
        {
            spin_unlock(&inotify->lock);
            return (size_t)-EINVAL;
        }

        inotify->event_head = (inotify->event_head + 1) % (sizeof(inotify->events) / sizeof(inotify->events[0]));
        inotify->event_count--;
        spin_unlock(&inotify->lock);

        struct inotify_event raw = {};
        raw.wd = event.wd;
        raw.mask = event.mask;
        raw.cookie = event.cookie;
        raw.len = event.name[0] == '\0' ? 0 : (uint32_t)(event_size - sizeof(raw));
        memcpy((uint8_t *)addr + written, &raw, sizeof(raw));
        if (raw.len != 0)
        {
            memset((uint8_t *)addr + written + sizeof(raw), 0, raw.len);
            strcpy((char *)addr + written + sizeof(raw), event.name);
        }
        written += event_size;

        spin_lock(&inotify->lock);
        if (written + sizeof(struct inotify_event) > size) break;
    }
    spin_unlock(&inotify->lock);
    return written;
}

static size_t anonfs_read(void *file, void *addr, size_t offset, size_t size)
{
    (void)offset;
    if (file == NULL || addr == NULL) return VFS_STATUS_FAILED;
    anonfs_handle_t *handle = (anonfs_handle_t *)file;

    switch (handle->kind)
    {
    case ANONFS_EVENTFD: return anonfs_eventfd_read(&handle->u.eventfd, addr, size);
    case ANONFS_SIGNALFD: return anonfs_signalfd_read(&handle->u.signalfd, addr, size);
    case ANONFS_TIMERFD:
        anonfs_timerfd_refresh(handle);
        return anonfs_timerfd_read(&handle->u.timerfd, addr, size);
    case ANONFS_MEMFD: return anonfs_memfd_read(&handle->u.memfd, addr, offset, size);
    case ANONFS_INOTIFY: return anonfs_inotify_read(&handle->u.inotify, addr, size);
    default: return VFS_STATUS_FAILED;
    }
}

static size_t anonfs_write(void *file, const void *addr, size_t offset, size_t size)
{
    if (file == NULL || addr == NULL) return VFS_STATUS_FAILED;
    anonfs_handle_t *handle = (anonfs_handle_t *)file;

    switch (handle->kind)
    {
    case ANONFS_EVENTFD: return anonfs_eventfd_write(&handle->u.eventfd, addr, size);
    case ANONFS_MEMFD: return anonfs_memfd_write(&handle->u.memfd, addr, offset, size);
    default: return VFS_STATUS_FAILED;
    }
}

static size_t anonfs_readlink(void *file, void *addr, size_t offset, size_t size)
{
    (void)file;
    (void)addr;
    (void)offset;
    (void)size;
    return VFS_STATUS_FAILED;
}

static errno_t anonfs_ioctl(void *file, size_t req, void *arg)
{
    (void)file;
    (void)req;
    (void)arg;
    return -ENOTTY;
}

static errno_t anonfs_poll(void *file, size_t events)
{
    if (file == NULL) return EPOLLERR;
    anonfs_handle_t *handle = (anonfs_handle_t *)file;
    int out = 0;

    switch (handle->kind)
    {
    case ANONFS_EVENTFD:
        spin_lock(&handle->u.eventfd.lock);
        if ((events & EPOLLIN) && handle->u.eventfd.counter != 0) out |= EPOLLIN;
        if (events & EPOLLOUT)
        {
            if (handle->u.eventfd.counter != ANONFS_U64_MAX - 1) out |= EPOLLOUT;
        }
        spin_unlock(&handle->u.eventfd.lock);
        break;
    case ANONFS_SIGNALFD:
        spin_lock(&handle->u.signalfd.lock);
        if ((events & EPOLLIN) && handle->u.signalfd.count != 0) out |= EPOLLIN;
        if (events & EPOLLOUT) out |= EPOLLOUT;
        spin_unlock(&handle->u.signalfd.lock);
        break;
    case ANONFS_TIMERFD:
        anonfs_timerfd_refresh(handle);
        spin_lock(&handle->u.timerfd.lock);
        if ((events & EPOLLIN) && handle->u.timerfd.pending != 0) out |= EPOLLIN;
        if (events & EPOLLOUT) out |= EPOLLOUT;
        spin_unlock(&handle->u.timerfd.lock);
        break;
    case ANONFS_MEMFD:
        if ((events & EPOLLIN) || (events & EPOLLOUT)) out |= (events & (EPOLLIN | EPOLLOUT));
        break;
    case ANONFS_INOTIFY:
        spin_lock(&handle->u.inotify.lock);
        if ((events & EPOLLIN) && handle->u.inotify.event_count != 0) out |= EPOLLIN;
        if (events & EPOLLOUT) out |= EPOLLOUT;
        spin_unlock(&handle->u.inotify.lock);
        break;
    default:
        out |= EPOLLERR;
        break;
    }

    return out;
}

static void *anonfs_map(void *file, void *addr, size_t offset, size_t size, size_t prot, size_t flags)
{
    (void)flags;
    anonfs_handle_t *handle = (anonfs_handle_t *)file;
    if (handle == NULL || handle->kind != ANONFS_MEMFD) return (void *)SYSCALL_FAULT_(ENODEV);

    anonfs_memfd_t *memfd = &handle->u.memfd;
    spin_lock(&memfd->lock);
    size_t end = 0;
    if (offset > (size_t)-1 - size)
    {
        spin_unlock(&memfd->lock);
        return (void *)SYSCALL_FAULT_(EINVAL);
    }
    end = offset + size;
    if (!anonfs_memfd_reserve(memfd, end))
    {
        spin_unlock(&memfd->lock);
        return (void *)SYSCALL_FAULT_(ENOMEM);
    }
    if (end > memfd->size) memfd->size = end;

    pcb_t process = get_current_task()->parent_group;
    uint64_t pte_flags = PTE_USER;
    if (prot != PROT_NONE) pte_flags |= PTE_PRESENT;
    if (prot & PROT_WRITE) pte_flags |= PTE_WRITEABLE;
    if (!(prot & PROT_EXEC)) pte_flags |= PTE_NO_EXECUTE;

    for (size_t pos = 0; pos < size; pos += PAGE_SIZE)
    {
        uint64_t virt = (uint64_t)addr + pos;
        uint64_t frame = memfd->phys + ((offset + pos) & PAGE_MASK);
        page_map_to(process->pagedir, virt, frame, pte_flags);
    }
    spin_unlock(&memfd->lock);
    return addr;
}

static void anonfs_resize(void *file, uint64_t size)
{
    if (file == NULL) return;
    anonfs_handle_t *handle = (anonfs_handle_t *)file;
    if (handle->kind != ANONFS_MEMFD) return;

    anonfs_memfd_t *memfd = &handle->u.memfd;
    spin_lock(&memfd->lock);
    if (size < memfd->size) memfd->size = (size_t)size;
    else if ((size_t)size > memfd->size)
    {
        if (anonfs_memfd_reserve(memfd, (size_t)size))
        {
            memset(memfd->data + memfd->size, 0, (size_t)size - memfd->size);
            memfd->size = (size_t)size;
        }
    }
    spin_unlock(&memfd->lock);
}

static errno_t anonfs_del(void *parent, vfs_node_t node)
{
    (void)parent;
    if (node == NULL || node->handle == NULL) return EOK;
    anonfs_handle_t *handle = (anonfs_handle_t *)node->handle;
    if (handle->kind == ANONFS_INOTIFY)
    {
        spin_lock(&anonfs_inotify_global_lock);
        for (size_t i = 0; i < sizeof(anonfs_inotify_handles) / sizeof(anonfs_inotify_handles[0]); i++)
        {
            if (anonfs_inotify_handles[i] == handle) anonfs_inotify_handles[i] = NULL;
        }
        spin_unlock(&anonfs_inotify_global_lock);
        for (size_t i = 0; i < handle->u.inotify.watch_count; i++)
            free(handle->u.inotify.watches[i].path);
    }
    if (handle->kind == ANONFS_MEMFD)
    {
        free(handle->u.memfd.name);
        if (handle->u.memfd.phys != 0)
        {
            size_t pages = (handle->u.memfd.capacity + PAGE_SIZE - 1) / PAGE_SIZE;
            if (pages != 0) free_frames(handle->u.memfd.phys, pages);
        }
    }
    free(handle);
    node->handle = NULL;
    return EOK;
}

static errno_t anonfs_rename(void *current, const char *nw)
{
    (void)current;
    (void)nw;
    return -ENOSYS;
}

static struct vfs_callback anonfs_callbacks = {
    .mount    = anonfs_mount,
    .unmount  = (vfs_unmount_t)empty,
    .open     = anonfs_open,
    .close    = (vfs_close_t)empty,
    .read     = (vfs_read_t)anonfs_read,
    .write    = (vfs_write_t)anonfs_write,
    .readlink = (vfs_readlink_t)anonfs_readlink,
    .mkdir    = (vfs_mk_t)empty,
    .mkfile   = (vfs_mk_t)empty,
    .link     = (vfs_mk_t)anonfs_dummy,
    .symlink  = (vfs_mk_t)anonfs_dummy,
    .stat     = anonfs_stat,
    .ioctl    = anonfs_ioctl,
    .dup      = (vfs_dup_t)empty,
    .poll     = anonfs_poll,
    .map      = anonfs_map,
    .resize   = anonfs_resize,
    .del      = anonfs_del,
    .rename   = anonfs_rename,
};

static vfs_node_t anonfs_alloc_node(const char *name, anonfs_kind_t kind, uint64_t fd_flags)
{
    if (anonfs_root == NULL) return NULL;

    spin_lock(&anonfs_alloc_lock);
    int inode = anonfs_next_inode++;
    spin_unlock(&anonfs_alloc_lock);

    vfs_node_t node = vfs_node_alloc(anonfs_root, name);
    if (node == NULL) return NULL;

    anonfs_handle_t *handle = (anonfs_handle_t *)calloc(1, sizeof(anonfs_handle_t));
    if (handle == NULL)
    {
        free(node->name);
        free(node);
        return NULL;
    }

    node->fsid  = anonfs_id;
    node->inode = (uint64_t)inode;
    node->mode  = 0600;
    node->flags = fd_flags;
    node->handle = handle;
    handle->kind = kind;

    switch (kind)
    {
    case ANONFS_EVENTFD:
        node->type = file_stream | file_anon;
        node->size = sizeof(uint64_t);
        handle->u.eventfd.lock = SPIN_INIT;
        break;
    case ANONFS_SIGNALFD:
        node->type = file_stream | file_anon;
        node->size = (uint64_t)-1;
        handle->u.signalfd.lock = SPIN_INIT;
        break;
    case ANONFS_TIMERFD:
        node->type = file_stream | file_anon;
        node->size = sizeof(uint64_t);
        handle->u.timerfd.lock = SPIN_INIT;
        break;
    case ANONFS_MEMFD:
        node->type = file_none | file_anon;
        node->size = 0;
        handle->u.memfd.lock = SPIN_INIT;
        break;
    case ANONFS_INOTIFY:
        node->type = file_stream | file_anon;
        node->size = (uint64_t)-1;
        handle->u.inotify.lock = SPIN_INIT;
        handle->u.inotify.next_wd = 1;
        break;
    default:
        free(handle);
        free(node->name);
        free(node);
        return NULL;
    }

    return node;
}

static int anonfs_install_fd(vfs_node_t node, uint64_t flags)
{
    if (node == NULL) return -ENOMEM;

    fd_file_handle *fd_handle = (fd_file_handle *)calloc(1, sizeof(fd_file_handle));
    if (fd_handle == NULL)
    {
        anonfs_del(NULL, node);
        free(node->name);
        free(node);
        return -ENOMEM;
    }

    fd_handle->node  = node;
    fd_handle->flags = flags;
    fd_handle->fd    = queue_enqueue_lowest(get_current_task()->parent_group->file_open, fd_handle);
    if ((int)fd_handle->fd < 0)
    {
        anonfs_del(NULL, node);
        free(node->name);
        free(node);
        free(fd_handle);
        return -EMFILE;
    }
    return (int)fd_handle->fd;
}

void anonfs_init(void)
{
    if (anonfs_id != 0) return;
    anonfs_id = vfs_regist("anonfs", &anonfs_callbacks, TMPFS_REGISTER_ID + 1000, 0x616e6f6e6673ULL);
    if (anonfs_id <= 0) return;
    vfs_node_t root = vfs_node_alloc(rootdir, "anon");
    if (root == NULL) return;
    root->type = file_dir;
    root->mode = 0700;
    if (vfs_mount((const char *)(uintptr_t)(TMPFS_REGISTER_ID + 1000), root) != VFS_STATUS_SUCCESS)
    {
        free(root->name);
        free(root);
    }
}

int anonfs_create_eventfd(uint64_t initial_value, uint32_t flags)
{
    if ((flags & ~(EFD_CLOEXEC | EFD_NONBLOCK | EFD_SEMAPHORE)) != 0) return -EINVAL;
    vfs_node_t node = anonfs_alloc_node("eventfd", ANONFS_EVENTFD, (flags & (EFD_CLOEXEC | EFD_NONBLOCK)));
    if (node == NULL) return -ENOMEM;

    anonfs_handle_t *handle = (anonfs_handle_t *)node->handle;
    handle->u.eventfd.counter = initial_value;
    handle->u.eventfd.flags = flags;
    return anonfs_install_fd(node, flags & (EFD_CLOEXEC | EFD_NONBLOCK));
}

int anonfs_create_inotify(uint32_t flags)
{
    if ((flags & ~(IN_CLOEXEC | IN_NONBLOCK)) != 0) return -EINVAL;
    vfs_node_t node = anonfs_alloc_node("inotify", ANONFS_INOTIFY, flags & (IN_CLOEXEC | IN_NONBLOCK));
    if (node == NULL) return -ENOMEM;

    anonfs_handle_t *handle = (anonfs_handle_t *)node->handle;
    handle->u.inotify.flags = flags;
    spin_lock(&anonfs_inotify_global_lock);
    for (size_t i = 0; i < sizeof(anonfs_inotify_handles) / sizeof(anonfs_inotify_handles[0]); i++)
    {
        if (anonfs_inotify_handles[i] == NULL)
        {
            anonfs_inotify_handles[i] = handle;
            break;
        }
    }
    spin_unlock(&anonfs_inotify_global_lock);
    int fd = anonfs_install_fd(node, flags & (IN_CLOEXEC | IN_NONBLOCK));
    write_serial_fmt("[udev-trace] inotify:create flags=0x%x fd=%d handle=%p\n", flags, fd, handle);
    return fd;
}

static anonfs_handle_t *anonfs_get_inotify_handle(int fd)
{
    if (fd < 0) return NULL;
    fd_file_handle *fd_handle = (fd_file_handle *)queue_get(get_current_task()->parent_group->file_open, (size_t)fd);
    if (fd_handle == NULL || fd_handle->node == NULL || fd_handle->node->handle == NULL) return NULL;
    anonfs_handle_t *handle = (anonfs_handle_t *)fd_handle->node->handle;
    return handle->kind == ANONFS_INOTIFY ? handle : NULL;
}

int anonfs_inotify_add_watch(int fd, const char *pathname, uint32_t mask)
{
    if (pathname == NULL) return -EFAULT;
    anonfs_handle_t *handle = anonfs_get_inotify_handle(fd);
    if (handle == NULL) return -EINVAL;

    char *path = NULL;
    int ret = anonfs_copy_string_from_user(&path, pathname, ANONFS_PATH_MAX);
    if (ret < 0) return ret;
    write_serial_fmt("[udev-trace] inotify:add-watch request fd=%d path=%s mask=0x%x\n", fd, path, mask);
    vfs_node_t node = vfs_open(path);
    if (node == NULL)
    {
        write_serial_fmt("[udev-trace] inotify:add-watch open-miss path=%s\n", path);
        free(path);
        return -ENOENT;
    }
    vfs_close(node);

    char *normalized = normalize_path(path);
    if (normalized != NULL)
    {
        free(path);
        path = normalized;
    }

    spin_lock(&handle->u.inotify.lock);
    for (size_t i = 0; i < handle->u.inotify.watch_count; i++)
    {
        if (strcmp(handle->u.inotify.watches[i].path, path) == 0)
        {
            if (mask & IN_MASK_CREATE)
            {
                spin_unlock(&handle->u.inotify.lock);
                free(path);
                return -EEXIST;
            }
            handle->u.inotify.watches[i].mask = (mask & IN_MASK_ADD) ?
                                                    (handle->u.inotify.watches[i].mask | (mask & ~IN_MASK_ADD)) :
                                                    mask;
            int existing_wd = handle->u.inotify.watches[i].wd;
            spin_unlock(&handle->u.inotify.lock);
            write_serial_fmt("[udev-trace] inotify:add-watch update path=%s wd=%d mask=0x%x\n",
                             path,
                             existing_wd,
                             handle->u.inotify.watches[i].mask);
            free(path);
            return existing_wd;
        }
    }

    if (handle->u.inotify.watch_count >= sizeof(handle->u.inotify.watches) / sizeof(handle->u.inotify.watches[0]))
    {
        spin_unlock(&handle->u.inotify.lock);
        free(path);
        return -ENOSPC;
    }

    int wd = handle->u.inotify.next_wd++;
    if (handle->u.inotify.next_wd <= 0) handle->u.inotify.next_wd = 1;
    size_t index = handle->u.inotify.watch_count++;
    handle->u.inotify.watches[index].wd = wd;
    handle->u.inotify.watches[index].mask = mask == 0 ? IN_ALL_EVENTS : mask;
    handle->u.inotify.watches[index].path = path;
    spin_unlock(&handle->u.inotify.lock);
    write_serial_fmt("[udev-trace] inotify:add-watch ok fd=%d wd=%d path=%s mask=0x%x watches=%llu\n",
                     fd,
                     wd,
                     path,
                     mask == 0 ? IN_ALL_EVENTS : mask,
                     (unsigned long long)handle->u.inotify.watch_count);
    return wd;
}

int anonfs_inotify_rm_watch(int fd, int wd)
{
    if (wd <= 0) return -EINVAL;
    anonfs_handle_t *handle = anonfs_get_inotify_handle(fd);
    if (handle == NULL) return -EINVAL;
    spin_lock(&handle->u.inotify.lock);
    for (size_t i = 0; i < handle->u.inotify.watch_count; i++)
    {
        if (handle->u.inotify.watches[i].wd == wd)
        {
            write_serial_fmt("[udev-trace] inotify:rm-watch fd=%d wd=%d path=%s\n",
                             fd,
                             wd,
                             handle->u.inotify.watches[i].path != NULL ? handle->u.inotify.watches[i].path : "(null)");
            free(handle->u.inotify.watches[i].path);
            handle->u.inotify.watches[i] = handle->u.inotify.watches[handle->u.inotify.watch_count - 1];
            memset(&handle->u.inotify.watches[handle->u.inotify.watch_count - 1], 0,
                   sizeof(handle->u.inotify.watches[0]));
            handle->u.inotify.watch_count--;
            spin_unlock(&handle->u.inotify.lock);
            return 0;
        }
    }
    spin_unlock(&handle->u.inotify.lock);
    return -EINVAL;
}

static bool anonfs_path_is_child_event(const char *watch_path, const char *path, const char **out_name)
{
    if (watch_path == NULL || path == NULL) return false;
    size_t watch_len = strlen(watch_path);
    if (strcmp(watch_path, path) == 0)
    {
        if (out_name != NULL) *out_name = "";
        return true;
    }
    if (watch_len == 0 || strncmp(path, watch_path, watch_len) != 0) return false;
    if (watch_path[watch_len - 1] != '/' && path[watch_len] != '/') return false;
    const char *name = path + watch_len + (watch_path[watch_len - 1] == '/' ? 0 : 1);
    if (strchr(name, '/') != NULL) return false;
    if (out_name != NULL) *out_name = name;
    return true;
}

void anonfs_notify_path_event(const char *path, uint32_t mask)
{
    if (path == NULL || *path == '\0') return;

    char *normalized = normalize_path(path);
    const char *event_path = normalized != NULL ? normalized : path;
    size_t delivered = 0;

    spin_lock(&anonfs_inotify_global_lock);
    for (size_t h = 0; h < sizeof(anonfs_inotify_handles) / sizeof(anonfs_inotify_handles[0]); h++)
    {
        anonfs_handle_t *handle = anonfs_inotify_handles[h];
        if (handle == NULL || handle->kind != ANONFS_INOTIFY) continue;

        spin_lock(&handle->u.inotify.lock);
        for (size_t i = 0; i < handle->u.inotify.watch_count; i++)
        {
            const char *name = "";
            anonfs_inotify_watch_t *watch = &handle->u.inotify.watches[i];
            if ((watch->mask & mask) == 0) continue;
            if (!anonfs_path_is_child_event(watch->path, event_path, &name)) continue;

            if (handle->u.inotify.event_count == sizeof(handle->u.inotify.events) / sizeof(handle->u.inotify.events[0]))
            {
                handle->u.inotify.event_head =
                    (handle->u.inotify.event_head + 1) % (sizeof(handle->u.inotify.events) / sizeof(handle->u.inotify.events[0]));
                handle->u.inotify.event_count--;
            }
            anonfs_inotify_event_record_t *event = &handle->u.inotify.events[handle->u.inotify.event_tail];
            memset(event, 0, sizeof(*event));
            event->wd = watch->wd;
            event->mask = mask;
            strncpy(event->name, name, sizeof(event->name) - 1);
            handle->u.inotify.event_tail =
                (handle->u.inotify.event_tail + 1) % (sizeof(handle->u.inotify.events) / sizeof(handle->u.inotify.events[0]));
            handle->u.inotify.event_count++;
            delivered++;
            write_serial_fmt("[udev-trace] inotify:queue path=%s watch=%s wd=%d mask=0x%x name=%s count=%llu\n",
                             event_path,
                             watch->path != NULL ? watch->path : "(null)",
                             watch->wd,
                             mask,
                             name != NULL ? name : "",
                             (unsigned long long)handle->u.inotify.event_count);
        }
        spin_unlock(&handle->u.inotify.lock);
    }
    spin_unlock(&anonfs_inotify_global_lock);
    if (delivered == 0 && (strstr(event_path, "/dev") != NULL || strstr(event_path, "/sys") != NULL ||
                           strstr(event_path, "/run/udev") != NULL))
    {
        write_serial_fmt("[udev-trace] inotify:notify-no-watch path=%s mask=0x%x\n", event_path, mask);
    }
    free(normalized);
}

static void anonfs_signalfd_copy_info(struct signalfd_siginfo *dst, int sig, const siginfo_t *info)
{
    memset(dst, 0, sizeof(*dst));
    dst->ssi_signo = (uint32_t)sig;
    if (info == NULL) return;
    dst->ssi_errno = info->si_errno;
    dst->ssi_code  = info->si_code;
    dst->ssi_pid   = (uint32_t)info->_sifields._kill.si_pid;
    dst->ssi_uid   = info->_sifields._kill.si_uid;
    dst->ssi_status = info->_sifields._sigchld.si_status;
    dst->ssi_int = info->_sifields._rt.si_sigval;
    dst->ssi_tid = (uint32_t)info->_sifields._timer.si_tid;
    dst->ssi_overrun = (uint32_t)info->_sifields._timer.si_overrun;
    dst->ssi_addr = info->_sifields._sigfault.si_addr;
    dst->ssi_addr_lsb = (uint16_t)info->_sifields._sigfault.si_addr_lsb;
    dst->ssi_band = (uint32_t)info->_sifields._sigpoll.si_band;
    dst->ssi_fd = info->_sifields._sigpoll.si_fd;
    dst->ssi_syscall = info->_sifields._sigsys.si_syscall;
    dst->ssi_call_addr = info->_sifields._sigsys.si_call_addr;
    dst->ssi_arch = info->_sifields._sigsys.si_arch;
}

int anonfs_create_signalfd(int ufd, const sigset_t *mask, size_t sizemask, uint32_t flags)
{
    if ((flags & ~(SFD_CLOEXEC | SFD_NONBLOCK)) != 0) return -EINVAL;
    if (mask == NULL || sizemask < sizeof(sigset_t)) return -EINVAL;

    page_directory_t *pagedir = get_current_task()->parent_group->pagedir;
    sigset_t kmask = 0;
    if (!copy_from_user_pagedir(pagedir, &kmask, mask, sizeof(kmask))) return -EFAULT;

    if (ufd >= 0)
    {
        fd_file_handle *fd_handle = (fd_file_handle *)queue_get(get_current_task()->parent_group->file_open, (size_t)ufd);
        if (fd_handle == NULL || fd_handle->node == NULL || fd_handle->node->handle == NULL) return -EBADF;
        anonfs_handle_t *handle = (anonfs_handle_t *)fd_handle->node->handle;
        if (handle->kind != ANONFS_SIGNALFD) return -EINVAL;
        spin_lock(&handle->u.signalfd.lock);
        handle->u.signalfd.mask = kmask;
        handle->u.signalfd.flags = flags;
        spin_unlock(&handle->u.signalfd.lock);
        fd_handle->flags &= ~(O_NONBLOCK | O_CLOEXEC);
        fd_handle->flags |= flags & (O_NONBLOCK | O_CLOEXEC);
        return ufd;
    }

    vfs_node_t node = anonfs_alloc_node("signalfd", ANONFS_SIGNALFD, flags & (SFD_CLOEXEC | SFD_NONBLOCK));
    if (node == NULL) return -ENOMEM;

    anonfs_handle_t *handle = (anonfs_handle_t *)node->handle;
    handle->u.signalfd.mask  = kmask;
    handle->u.signalfd.flags = flags;
    return anonfs_install_fd(node, flags & (SFD_CLOEXEC | SFD_NONBLOCK));
}

int anonfs_create_timerfd(int clockid, uint32_t flags)
{
    if ((flags & ~(TFD_CLOEXEC | TFD_NONBLOCK)) != 0) return -EINVAL;
    switch (clockid)
    {
    case CLOCK_REALTIME:
    case CLOCK_MONOTONIC:
    case CLOCK_MONOTONIC_RAW:
    case CLOCK_BOOTTIME:
        break;
    default:
        return -EINVAL;
    }

    vfs_node_t node = anonfs_alloc_node("timerfd", ANONFS_TIMERFD, flags & (TFD_CLOEXEC | TFD_NONBLOCK));
    if (node == NULL) return -ENOMEM;

    anonfs_handle_t *handle = (anonfs_handle_t *)node->handle;
    handle->u.timerfd.flags = flags;
    handle->u.timerfd.clockid = clockid;
    return anonfs_install_fd(node, flags & (TFD_CLOEXEC | TFD_NONBLOCK));
}

static uint64_t anonfs_timespec_to_ns(const struct timerfd_timespec *ts)
{
    if (ts == NULL || ts->tv_sec < 0 || ts->tv_nsec < 0 || ts->tv_nsec >= 1000000000L) return ANONFS_U64_MAX;
    return (uint64_t)ts->tv_sec * 1000000000ULL + (uint64_t)ts->tv_nsec;
}

static void anonfs_ns_to_timespec(uint64_t ns, struct timerfd_timespec *ts)
{
    if (ts == NULL) return;
    ts->tv_sec  = (long long)(ns / 1000000000ULL);
    ts->tv_nsec = (long)(ns % 1000000000ULL);
}

int anonfs_timerfd_settime(int fd, int flags, const struct itimerspec *new_value, struct itimerspec *old_value)
{
    if ((flags & ~(TFD_TIMER_ABSTIME | TFD_TIMER_CANCEL_ON_SET)) != 0) return -EINVAL;
    if (new_value == NULL) return -EINVAL;

    fd_file_handle *fd_handle = (fd_file_handle *)queue_get(get_current_task()->parent_group->file_open, (size_t)fd);
    if (fd_handle == NULL || fd_handle->node == NULL || fd_handle->node->handle == NULL) return -EBADF;
    anonfs_handle_t *handle = (anonfs_handle_t *)fd_handle->node->handle;
    if (handle->kind != ANONFS_TIMERFD) return -EINVAL;

    struct itimerspec knew = {};
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &knew, new_value, sizeof(knew))) return -EFAULT;

    uint64_t interval_ns = anonfs_timespec_to_ns(&knew.it_interval);
    uint64_t value_ns    = anonfs_timespec_to_ns(&knew.it_value);
    if (interval_ns == ANONFS_U64_MAX || value_ns == ANONFS_U64_MAX) return -EINVAL;

    anonfs_timerfd_t *timer = &handle->u.timerfd;
    spin_lock(&timer->lock);
    if (old_value != NULL)
    {
        struct itimerspec kold = {};
        if (timer->armed && timer->next_expire_ns > 0)
        {
            uint64_t now = anonfs_clock_now_ns(timer->clockid);
            uint64_t remain = timer->next_expire_ns > now ? timer->next_expire_ns - now : 0;
            anonfs_ns_to_timespec(remain, &kold.it_value);
        }
        anonfs_ns_to_timespec(timer->interval_ns, &kold.it_interval);
        spin_unlock(&timer->lock);
        if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, old_value, &kold, sizeof(kold))) return -EFAULT;
        spin_lock(&timer->lock);
    }

    timer->interval_ns = interval_ns;
    timer->pending = 0;
    if (value_ns == 0)
    {
        timer->armed = false;
        timer->next_expire_ns = 0;
    }
    else
    {
        timer->armed = true;
        uint64_t base = anonfs_clock_now_ns(timer->clockid);
        timer->next_expire_ns = (flags & TFD_TIMER_ABSTIME) ? value_ns : base + value_ns;
    }
    spin_unlock(&timer->lock);
    return 0;
}

int anonfs_create_memfd(const char *name, uint32_t flags)
{
    if ((flags & ~(MFD_CLOEXEC | MFD_ALLOW_SEALING)) != 0) return -EINVAL;

    char *kname = NULL;
    if (name != NULL)
    {
        size_t len = anonfs_bounded_strlen_user(name, 256);
        kname = (char *)malloc(len + 1);
        if (kname == NULL) return -ENOMEM;
        if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, kname, name, len))
        {
            free(kname);
            return -EFAULT;
        }
        kname[len] = '\0';
    }
    else
    {
        kname = strdup("memfd");
        if (kname == NULL) return -ENOMEM;
    }

    vfs_node_t node = anonfs_alloc_node("memfd", ANONFS_MEMFD, (flags & MFD_CLOEXEC) ? O_CLOEXEC : 0);
    if (node == NULL)
    {
        free(kname);
        return -ENOMEM;
    }

    anonfs_handle_t *handle = (anonfs_handle_t *)node->handle;
    handle->u.memfd.name  = kname;
    handle->u.memfd.flags = flags;
    return anonfs_install_fd(node, (flags & MFD_CLOEXEC) ? O_CLOEXEC : 0);
}

size_t anonfs_notify_signal(pcb_t process, int sig, const siginfo_t *info)
{
    if (process == NULL || process->file_open == NULL || sig < MINSIG || sig > MAXSIG) return 0;

    size_t delivered = 0;
    spin_lock(&process->file_open->lock);
    for (lock_node *node = process->file_open->head; node != NULL; node = node->next)
    {
        fd_file_handle *fd_handle = (fd_file_handle *)node->data;
        if (fd_handle == NULL || fd_handle->node == NULL || fd_handle->node->handle == NULL) continue;

        anonfs_handle_t *handle = (anonfs_handle_t *)fd_handle->node->handle;
        if (handle->kind != ANONFS_SIGNALFD) continue;

        anonfs_signalfd_t *sfd = &handle->u.signalfd;
        spin_lock(&sfd->lock);
        if ((sfd->mask & SIGMASK(sig)) != 0 && sfd->count < (sizeof(sfd->queue) / sizeof(sfd->queue[0])))
        {
            struct signalfd_siginfo *entry = &sfd->queue[sfd->tail];
            anonfs_signalfd_copy_info(entry, sig, info);
            sfd->tail = (sfd->tail + 1) % (sizeof(sfd->queue) / sizeof(sfd->queue[0]));
            sfd->count++;
            delivered++;
        }
        spin_unlock(&sfd->lock);
    }
    spin_unlock(&process->file_open->lock);
    if (sig == SIGCHLD)
    {
        write_serial_fmt("[udev-trace] signalfd:notify-sigchld process=%s pid=%llu child=%d status=%d delivered=%llu\n",
                         process->name,
                         (unsigned long long)process->pid,
                         info != NULL ? info->_sifields._sigchld.si_pid : 0,
                         info != NULL ? info->_sifields._sigchld.si_status : 0,
                         (unsigned long long)delivered);
    }
    return delivered;
}
