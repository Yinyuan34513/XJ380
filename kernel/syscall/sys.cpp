#include <syscall/syscall.h>
#include <proto.hpp>
#include <fs/vfs/vfs.h>
#include <cpu/fsgsbase.h>
#include <task/poll.h>
#include <mm/lazyalloc.h>
#include "../build_settings.h"
#include <pipe.h>
#include <rtc.h>
#include <math.hpp>
sys_(exit, int exit_code)
{
    tcb_t exit_thread = get_current_task();
    write_serial_fmt("sys_exit: Thread %s exit with code %d.\n", exit_thread->name, exit_code);
    kill_thread(exit_thread);
    open_interrupt;
    cpu_hlt;
    return 0;
}

sys_(open, char *path0, uint64_t flags, uint64_t mode)
{
    if (unlikely(path0 == NULL)) return SYSCALL_FAULT_(EINVAL);

    char *normalized_path = strdup(path0);


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
            else
                goto next;
        }
        else
        err:
            free(normalized_path);
        return SYSCALL_FAULT_(ENOENT);
    }
next:
    fd_file_handle *fd_handle = (fd_file_handle *)calloc(1, sizeof(fd_file_handle));
    // not_null_assets(fd_handle, "sys_open: null alloc fd");
    fd_handle->offset = flags & O_APPEND ? node->size : 0;
    fd_handle->node   = node;
    int index         = (int)lock_queue_enqueue(get_current_task()->parent_group->file_open, fd_handle);
    fd_handle->fd     = index;
    spin_unlock(&get_current_task()->parent_group->file_open->lock);
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

sys_(close, int fd)
{
    if (unlikely(fd < 0)) return SYSCALL_FAULT_(EINVAL);
    fd_file_handle *handle = (fd_file_handle *)queue_remove_at(get_current_task()->parent_group->file_open, fd);
    vfs_close(handle->node);
    free(handle);
    return 0;
}

sys_(write, int fd, uint8_t *buffer, size_t size)
{
    if (unlikely(fd < 0 || buffer == NULL)) return SYSCALL_FAULT_(EINVAL);
    if (unlikely(size == 0)) return 0;
    if (fd == 1 || fd == 2)
    {
        write_serial_fmt("%s", (char *)buffer);
        return strlen((char *)buffer);
    }
    fd_file_handle *handle = (fd_file_handle *)queue_get(get_current_task()->parent_group->file_open, fd);
    if (!handle) return SYSCALL_FAULT_(EBADF);
    size_t ret = vfs_write(handle->node, buffer, handle->offset, size);
    if (ret == (size_t)VFS_STATUS_FAILED) return SYSCALL_FAULT_(EIO);
    if (handle->node->size != (uint64_t)-1) handle->offset += ret;
    vfs_update(handle->node);
    return ret;
}

sys_(read, int fd, uint8_t *buffer, size_t size)
{
    if (unlikely(fd < 0 || buffer == NULL)) return SYSCALL_FAULT_(EINVAL);
    if (unlikely(size == 0)) return 0;
    fd_file_handle *handle = (fd_file_handle *)queue_get(get_current_task()->parent_group->file_open, fd);
    if (!handle) return SYSCALL_FAULT_(EBADF);
    if (handle->node->type & file_pipe && handle->node->size == 0 && handle->flags & O_NONBLOCK)
    {
        return SYSCALL_FAULT_(EWOULDBLOCK);
    }
    size_t ret;
    if (handle->node->size != (uint64_t)-1)
    {
        if (handle->offset >= handle->node->size)
        {
            if (handle->node->type & file_pipe) { goto pipe; }
            return 0;
        }
    }
read:;
    ret = vfs_read(handle->node, buffer, handle->offset, size);
    if (ret == (size_t)VFS_STATUS_FAILED) return SYSCALL_FAULT_(EIO);
    if (handle->node->size != (uint64_t)-1) { handle->offset += ret; }
    return ret;
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

sys_(sigret)
{
    return 0;
}

sys_(getpid)
{
    if (unlikely(arg0 == UINT64_MAX || arg1 == UINT64_MAX || arg2 == UINT64_MAX || arg3 == UINT64_MAX ||
                 arg4 == UINT64_MAX))
        return 1;
    return get_current_task()->parent_group->pid;
}


sys_(stat, char *fn, struct stat *buf)
{
    if (unlikely(fn == NULL || buf == NULL)) return SYSCALL_FAULT_(EINVAL);
    char      *path = vfs_cwd_path_build(fn);
    vfs_node_t node = vfs_open(path);
    if (node == NULL)
    {
        free(path);
        return SYSCALL_FAULT_(ENOENT);
    }
    buf->st_gid   = (int)node->group;
    buf->st_uid   = (int)node->owner;
    buf->st_ino   = node->inode;
    buf->st_size  = (long long int)node->size;
    buf->st_mode  = node->mode | (node->type == file_symlink  ? S_IFLNK
                                  : node->type == file_dir    ? S_IFDIR
                                  : node->type == file_block  ? S_IFBLK
                                  : node->type == file_socket ? S_IFSOCK
                                  : node->type == file_none   ? S_IFREG
                                  : node->type == file_stream ? S_IFCHR
                                                              : S_IFREG);
    buf->st_nlink = 1;
    buf->st_dev   = (long)node->dev;
    buf->st_rdev  = (long)node->rdev;
    free(path);
    return 0;
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
    char sysname[] = "XJ380OS";
    char machine[] = "x86_64";
    char version[] = "2.1";
    memcpy(utsname->sysname, sysname, sizeof(sysname));
    memcpy(utsname->nodename, "localhost", 50); 
    memcpy(utsname->release, KN_VERSION, sizeof(KN_VERSION));
    memcpy(utsname->version, version, sizeof(version));
    memcpy(utsname->machine, machine, sizeof(machine));
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
    memcpy(&k_req, time_handle, sizeof(k_req));
    if (unlikely(k_req.tv_nsec >= 1000000000L)) return SYSCALL_FAULT_(EINVAL);
    uint64_t nsec = k_req.tv_sec * 1000000000 + k_req.tv_nsec;
    scheduler_nano_sleep(nsec);
    return 0;
}

sys_(ioctl, int fd, int options, void *arg2)
{
    if (unlikely(fd < 0 || arg2 == NULL)) return SYSCALL_FAULT_(EINVAL);
    fd_file_handle *handle =  (fd_file_handle*)queue_get(get_current_task()->parent_group->file_open, fd);
    int             ret    = vfs_ioctl(handle->node, options, arg2);
    return ret;
}

sys_(writev, int fd, struct iovec *iov, int iovcnt)
{
    if (unlikely(fd < 0 || iov == NULL)) return SYSCALL_FAULT_(EINVAL);
    if (iovcnt == 0) return 0;
    fd_file_handle *handle =  (fd_file_handle*)queue_get(get_current_task()->parent_group->file_open, fd);
    size_t          total  = 0;
    for (int i = 0; i < iovcnt; i++)
    {
        size_t status = vfs_write(handle->node, iov[i].iov_base, handle->offset, iov[i].iov_len);
        if (handle->node->size != (uint64_t)-1)
        {
            if (status == (size_t)VFS_STATUS_FAILED) return total;
            handle->offset += status;
        }
        total += iov[i].iov_len;
    }
    return total;
}

sys_(readv, int fd, struct iovec *iov, int iovcnt0)
{
    if (unlikely(fd < 0 || iov == NULL)) return SYSCALL_FAULT_(EINVAL);
    if (iovcnt0 == 0) return 0;
    size_t          iovcnt = iovcnt0;
    fd_file_handle *handle =  (fd_file_handle*)queue_get(get_current_task()->parent_group->file_open, fd);
    if (iovcnt == 0) return 0;
    if (handle == NULL) return SYSCALL_FAULT_(EBADF);
    size_t buf_len = 0;
    for (size_t i = 0; i < iovcnt; i++)
    {
        buf_len += iov[i].iov_len;
    }
    uint8_t *buf = (uint8_t *)malloc(buf_len);
    if (handle->node->size != (uint64_t)-1)
    {
        if (handle->offset > handle->node->size) return 0;
    }
    size_t status = vfs_read(handle->node, buf, handle->offset, buf_len);
    if (status == (size_t)VFS_STATUS_FAILED)
    {
        free(buf);
        return SYSCALL_FAULT_(EIO);
    }
    if (handle->node->size != (uint64_t)-1) handle->offset += status;
    size_t copied = 0;
    for (size_t i = 0; i < iovcnt; i++)
    {
        size_t len = iov[i].iov_len;
        if (len == 0) continue;

        size_t to_copy = len;
        if (copied + to_copy > status) { to_copy = status - copied; }

        memcpy(iov[i].iov_base, buf + copied, to_copy);
        copied += to_copy;
    }
    free(buf);
    return status;
}
spin_t mm_op_lock=SPIN_INIT;

uint64_t sys_brk(uint64_t brk) {
    brk = (brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    if (!brk) {
        return get_current_task()->parent_group->brk_start;
    }

    if (brk < get_current_task()->parent_group->brk_start) {
        return get_current_task()->parent_group->brk_current;
    }

    if (brk == get_current_task()->parent_group->brk_current) {
        return get_current_task()->parent_group->brk_current;
    }

    spin_lock(&mm_op_lock);

    if (brk > get_current_task()->parent_group->brk_current) {
        page_map_range_to_random(get_current_directory(),
                       get_current_task()->parent_group->brk_current,
                       brk - get_current_task()->parent_group->brk_current,
                       PTE_FLAG_U | PTE_PRESENT | PTE_WRITEABLE);

        vma_t *vma = vma_alloc();
        if (!vma) {
            spin_unlock(&mm_op_lock);
            return (uint64_t)-ENOMEM;
        }

        vma->vm_start = get_current_task()->parent_group->brk_current;
        vma->vm_end = brk;
        vma->vm_flags = VMA_READ | VMA_WRITE | VMA_ANON;
        vma->vm_type = VMA_TYPE_ANON;
        vma->vm_name = strdup("[heap]");
        if (!vma->vm_name) {
            vma_free(vma);
            spin_unlock(&mm_op_lock);
            return (uint64_t)-ENOMEM;
        }

        vma_t *region =
            vma_find_intersection(&get_current_task()->parent_group->vma_manager,
                                  get_current_task()->parent_group->brk_start,
                                  get_current_task()->parent_group->brk_end);

        if (region) {
            vma_remove(&get_current_task()->parent_group->vma_manager, region);
            vma_free(region);
        }

        if (vma_insert(&get_current_task()->parent_group->vma_manager, vma)) {
            vma_free(vma);
            spin_unlock(&mm_op_lock);
            return (uint64_t)-ENOMEM;
        }

        get_current_task()->parent_group->brk_current = brk;
    } else {
        unmap_page_range(get_current_directory(), brk,
                         get_current_task()->parent_group->brk_current - brk);

        vma_t *region =
            vma_find_intersection(&get_current_task()->parent_group->vma_manager,
                                  get_current_task()->parent_group->brk_start,
                                  get_current_task()->parent_group->brk_end);

        if (region) {
            vma_remove(&get_current_task()->parent_group->vma_manager, region);
            vma_free(region);
        }

        if (brk > get_current_task()->parent_group->brk_start) {
            vma_t *vma = vma_alloc();
            if (!vma) {
                get_current_task()->parent_group->brk_current = brk;
                return get_current_task()->parent_group->brk_current;
            }

            vma->vm_start = get_current_task()->parent_group->brk_start;
            vma->vm_end = brk;
            vma->vm_flags = VMA_READ | VMA_WRITE | VMA_ANON;
            vma->vm_type = VMA_TYPE_ANON;
            vma->vm_name = strdup("[heap]");

            if (vma_insert(&get_current_task()->parent_group->vma_manager,
                           vma) != 0) {
                vma_free(vma);
                // 继续执行，即使VMA插入失败
            }
        }

        get_current_task()->parent_group->brk_current = brk;
    }

    spin_unlock(&mm_op_lock);

    return get_current_task()->parent_group->brk_current;
}

sys_(mmap, uint64_t addr, size_t length, uint64_t prot, uint64_t flags, int fd,
         uint64_t offset) {

    addr = addr & (~(PAGE_SIZE - 1));

    uint64_t aligned_len = (length + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1));

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
            while (vma_find_intersection(mgr, start_addr, start_addr + aligned_len)) {
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
    if (!vma) return SYSCALL_FAULT_(ENOMEM);

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

sys_(munmap, uint64_t addr, size_t size) {
    if (size == 0) return 0;

    addr = addr & (~(PAGE_SIZE - 1));
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

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
    old_size = (old_size + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1));
    new_size = (new_size + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1));

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
        while (vma_find_intersection(mgr, start_addr, start_addr + new_size)) {
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
    tcb_t  process  = get_current_task();
    char  *cwd      = vfs_get_fullpath(process->cwd);
    size_t cwd_leng = strlen(cwd);
    if (length > cwd_leng) length = cwd_leng;
    memcpy(buffer, cwd, length);
    return length;
}

sys_(chdir, char *s)
{
    if (unlikely(s == NULL)) return SYSCALL_FAULT_(EINVAL);
    pcb_t process = get_current_task()->parent_group;

    char *path;
    char *bpath = NULL;
    if (s[0] == '/') { path = strdup(s); }
    else
    {
        bpath = vfs_get_fullpath(get_current_task()->cwd);
        path  = pathacat(bpath, s);
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

    if (node->type == file_dir) { get_current_task()->cwd = node; }
    else
    {
        return SYSCALL_FAULT_(ENOTDIR);
    }

    free(normalized_path);
    return 0;
}

sys_(exit_group, int exit_code)
{
    pcb_t exit_process = get_current_task()->parent_group;
    write_serial_fmt("task: Process %s exit with code %d.\n", exit_process->name, exit_code);
    close_interrupt;
    kill_proc(exit_process, exit_code,
              true); // 子进程调用，is_zombie = true，不能在child_pcb中删除当前进程
    open_interrupt;
    //scheduler_yield();
    cpu_hlt;
}

sys_(poll, struct pollfd *fds_user, size_t nfds, size_t timeout)
{
    int      ready      = 0;
    uint64_t start_time = nanoTime();
    bool     sigexit    = false;

    extern vfs_callback_t fs_callbacks[256];

    do
    {
        // 检查每个文件描述符
        size_t i = 0;
        queue_foreach(get_current_task()->parent_group->file_open, node0)
        {
            if (i >= nfds) break;
            fd_file_handle *handle = (fd_file_handle *)node0->data;
            vfs_node_t      node   = handle->node;
            if (fs_callbacks[node->fsid]->poll == (void *)empty)
            {
                if (fds_user[i].events & POLLIN || fds_user[i].events & POLLOUT)
                {
                    fds_user[i].revents = fds_user[i].events & POLLIN ? POLLIN : POLLOUT;
                    ready++;
                }
                i++;
                continue;
            }
            int revents = (int)epoll_to_poll_comp(vfs_poll(node, poll_to_epoll_comp(fds_user[i].events)));
            if (revents > 0)
            {
                fds_user[i].revents = (short)revents;
                ready++;
            }
            i++;
        }

        // sigexit = signals_pending_quick(get_current_task());

        if (ready > 0 || sigexit) break;

        open_interrupt;
        __asm__("pause");
    } while (timeout != 0 && ((int)timeout == -1 || (nanoTime() - start_time) < timeout));

    close_interrupt;

    if (!ready && sigexit) return (size_t)-EINTR;
    return ready;
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
        handle->node->flags  |= arg & valid_flags;}
    default: break;
    }
    return 0;
}

sys_(fork)
{
    return process_fork(regs, false, 0);
}

sys_(get_tid)
{
    return get_current_task()->tid;
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
    vfs_node_t node = handle->node;
    buf->st_gid     = (int)node->group;
    buf->st_uid     = (int)node->owner;
    buf->st_size    = node->size == (uint64_t)-1 ? 0 : (long long int)node->size;
    buf->st_mode    = node->type | (node->type == file_symlink  ? S_IFLNK
                                    : node->type == file_dir    ? S_IFDIR
                                    : node->type == file_block  ? S_IFBLK
                                    : node->type == file_socket ? S_IFSOCK
                                    : node->type == file_none   ? S_IFREG
                                    : node->type == file_stream ? S_IFCHR
                                                                : 0);
    buf->st_nlink   = 1;
    buf->st_dev     = node->dev;
    buf->st_rdev    = node->rdev;
    buf->st_ctim = buf->st_atim = buf->st_ctim = buf->st_mtim =
        (struct timespec){.tv_sec = node->createtime / 1000000000ULL, .tv_nsec = node->createtime % 1000000000ULL};
    buf->st_blksize = PAGE_SIZE;
    buf->st_blocks  = (node->size + PAGE_SIZE - 1) / PAGE_SIZE;
    buf->st_ino     = node->inode;
    return 0;
}

sys_(clock_gettime, uint64_t arg0, struct timespec *ts)
{
    switch (arg0)
    {
    case 1:
    case 6:
    case 4: {
        if (ts != NULL)
        {
            uint64_t nano = nanoTime();
            ts->tv_sec    = nano / 1000000000ULL;
            ts->tv_nsec   = nano % 1000000000ULL;
        }
        return 0;
    }
    case 0: {
        tm tm;
        time_read(&tm);
        uint64_t timestamp = mktime(&tm);
        if (ts != NULL)
        {
            ts->tv_sec  = timestamp;
            ts->tv_nsec = 0;
        }
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
    if (arg2 == 0) return SYSCALL_FAULT_(EINVAL);
    ((struct timespec *)arg2)->tv_nsec = 1000000;
    return 0;
}


sys_(mkdir, char *name, uint64_t mode)
{
    if (name == NULL) return SYSCALL_FAULT_(EINVAL);
    char  *npath = vfs_cwd_path_build(name);
    size_t ret   = vfs_mkdir(npath) == VFS_STATUS_FAILED ? -1 : 0;
    free(npath);
    return ret;
}

sys_(lseek, int fd, size_t offset, size_t whence)
{
    fd_file_handle *handle =  (fd_file_handle*)queue_get(get_current_task()->parent_group->file_open, fd);
    if (unlikely(handle == NULL)) return SYSCALL_FAULT_(EBADF);

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

sys_(select, int nfds, uint8_t *read, uint8_t *write, uint8_t *except, struct timeval *timeout)
{
    if (read && check_user_overflow((uint64_t)read, sizeof(struct pollfd))) { return SYSCALL_FAULT_(EFAULT); }
    if (write && check_user_overflow((uint64_t)write, sizeof(struct pollfd))) { return SYSCALL_FAULT_(EFAULT); }
    if (except && check_user_overflow((uint64_t)except, sizeof(struct pollfd))) { return SYSCALL_FAULT_(EFAULT); }
    size_t         complength = sizeof(struct pollfd);
    struct pollfd *comp       = (struct pollfd *)malloc(complength);
    memset(comp, 0, complength);
    size_t compIndex = 0;
    if (read)
    {
        for (int i = 0; i < nfds; i++)
        {
            if (select_bitmap(read, i)) select_add(&comp, &compIndex, &complength, i, POLLIN);
        }
    }
    if (write)
    {
        for (int i = 0; i < nfds; i++)
        {
            if (select_bitmap(write, i)) select_add(&comp, &compIndex, &complength, i, POLLOUT);
        }
    }
    if (except)
    {
        for (int i = 0; i < nfds; i++)
        {
            if (select_bitmap(except, i)) select_add(&comp, &compIndex, &complength, i, POLLPRI | POLLERR);
        }
    }

    //int toZero = (nfds + 8) / 8;
    int toZero = (nfds + 7) / 8;
    if (read) memset(read, 0, toZero);
    if (write) memset(write, 0, toZero);
    if (except) memset(except, 0, toZero);

    size_t time = 0;
    if (timeout == NULL) { time = -1; }
    else if (timeout->tv_sec == -1 || timeout->tv_usec == -1) { time = -1; }
    else
        time = (timeout->tv_sec * 1000 + (timeout->tv_usec + 1000) / 1000);

    size_t res = sys_poll(comp, compIndex, time, 0, 0, 0, 0);

    if ((int64_t)res < 0)
    {
        free(comp);
        return res;
    }

    size_t verify = 0;
    for (size_t i = 0; i < compIndex; i++)
    {
        if (!comp[i].revents) continue;
        if (comp[i].events & POLLIN && comp[i].revents & POLLIN)
        {
            select_bitmap_set(read, comp[i].fd);
            verify++;
        }
        if (comp[i].events & POLLOUT && comp[i].revents & POLLOUT)
        {
            select_bitmap_set(write, comp[i].fd);
            verify++;
        }
        if ((comp[i].events & POLLPRI && comp[i].revents & POLLPRI))
        {
            select_bitmap_set(except, comp[i].fd);
            verify++;
        }
    }

    free(comp);
    return verify;
}

sys_(pselect6, uint64_t nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timespec *timeout,
     WeirdPselect6 *weirdPselect6)
{
    if (readfds && check_user_overflow((uint64_t)readfds, sizeof(fd_set) * nfds)) { return SYSCALL_FAULT_(EFAULT); }
    if (writefds && check_user_overflow((uint64_t)writefds, sizeof(fd_set) * nfds)) { return SYSCALL_FAULT_(EFAULT); }
    if (exceptfds && check_user_overflow((uint64_t)exceptfds, sizeof(fd_set) * nfds)) { return SYSCALL_FAULT_(EFAULT); }
    size_t    sigsetsize = weirdPselect6->ss_len;
    sigset_t *sigmask    = weirdPselect6->ss;
    if (sigsetsize < sizeof(sigset_t)) { return SYSCALL_FAULT_(EINVAL); }
    sigset_t origmask = 0;
    if (sigmask) { syscall_ssetmask(SIG_SETMASK, sigmask, &origmask); }
    struct timeval timeoutConv;
    if (timeout)
    {
        timeoutConv =
            (struct timeval){.tv_sec = (long)timeout->tv_sec, .tv_usec = (long)(timeout->tv_nsec + 1000l) / 1000};
    }
    else
    {
        timeoutConv = (struct timeval){.tv_sec = (long)-1, .tv_usec = (long)-1};
    }

    size_t ret = sys_select((uint64_t)nfds, (uint8_t *)readfds, (uint8_t *)writefds, (uint8_t *)exceptfds,
                                &timeoutConv, 0, 0);
    if (sigmask) { syscall_ssetmask(SIG_SETMASK, &origmask, NULL); }
    return ret;
}


sys_(getdents, int fd, struct dirent *dents, size_t size)
{
    // if (unlikely(check_user_overflow((uint64_t)dents, size))) { return SYSCALL_FAULT_(EFAULT); }
    fd_file_handle *handle =  (fd_file_handle*)queue_get(get_current_task()->parent_group->file_open, fd);
    if (unlikely(handle == NULL)) { return SYSCALL_FAULT_(EBADF); }
    if (handle->node->type != file_dir) { return SYSCALL_FAULT_(ENOTDIR); }
    size_t   child_count   = (uint64_t)list_length(handle->node->child);
    size_t   max_dents_num = size / sizeof(struct dirent);
    size_t   read_count    = 0;
    uint64_t offset        = 0;
    vfs_node_t child_node;
    list_foreach(handle->node->child, i)
    {
        if (offset < handle->offset) { goto next; }
        if (handle->offset >= (child_count * sizeof(struct dirent))) { break; }
        if (read_count >= max_dents_num) { break; }
        child_node      = (vfs_node_t)i->data;
        dents[read_count].d_ino    = (long)child_node->inode;
        dents[read_count].d_off    = (long)handle->offset;
        dents[read_count].d_reclen = sizeof(struct dirent);
        if (child_node->type & file_symlink) { dents[read_count].d_type = DT_LNK; }
        else if (child_node->type & file_none) { dents[read_count].d_type = DT_REG; }
        else if (child_node->type & file_block) { dents[read_count].d_type = DT_BLK; }
        else if (child_node->type & file_stream) { dents[read_count].d_type = DT_CHR; }
        else if (child_node->type & file_socket) { dents[read_count].d_type = DT_SOCK; }
        else if (child_node->type & file_dir) { dents[read_count].d_type = DT_DIR; }
        else
        {
            dents[read_count].d_type = DT_UNKNOWN;
        }
        strncpy(dents[read_count].d_name, child_node->name, 256);
        handle->offset += sizeof(struct dirent);
        read_count++;
    next:
        offset += sizeof(struct dirent);
    }
    return read_count * sizeof(struct dirent);
}

sys_(newfstatat, int dirfd, char *pathname, struct stat *buf, uint64_t flags)
{
    char    *resolved = at_resolve_pathname(dirfd, pathname);
    uint64_t ret      = sys_stat(resolved, buf, 0, 0, 0, 0, 0);
    free(resolved);
    return ret;
}

sys_(statx, int dirfd, char *pathname, uint64_t flags, uint64_t mask, struct statx *buff)
{
    // if (unlikely(!pathname || check_user_overflow((uint64_t)pathname, strlen(pathname))))
    // {
    //     return SYSCALL_FAULT_(EFAULT);
    // }
    // if (unlikely(!buff || check_user_overflow((uint64_t)buff, sizeof(struct statx)))) { return SYSCALL_FAULT_(EFAULT); }

    struct stat simple;
    memset(&simple, 0, sizeof(struct stat));
    uint64_t ret = sys_newfstatat(dirfd, pathname, &simple, flags, 0, 0, 0);
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

    buff->stx_atime.tv_sec  = (long)simple.st_atim.tv_sec;
    buff->stx_atime.tv_nsec = simple.st_atim.tv_nsec;

    buff->stx_btime.tv_sec  = (long)simple.st_ctim.tv_sec;
    buff->stx_btime.tv_nsec = simple.st_ctim.tv_nsec;

    buff->stx_ctime.tv_sec  = (long)simple.st_ctim.tv_sec;
    buff->stx_ctime.tv_nsec = simple.st_ctim.tv_nsec;

    buff->stx_mtime.tv_sec  = (long)simple.st_mtim.tv_sec;
    buff->stx_mtime.tv_nsec = simple.st_mtim.tv_nsec;
    return 0;
}

sys_(pipe2, int *pipefd, uint64_t flags)
{
    /* fs/pipefs.c */
    extern vfs_node_t pipefs_root;
    extern int        pipefd_id;
    extern int        pipefs_id;

    if (pipefs_root == NULL) return SYSCALL_FAULT_(ENOSYS);

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

    pipefd[0] = (int)handle_in->fd;
    pipefd[1] = (int)handle_out->fd;

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
    char      *npath = vfs_cwd_path_build(name);
    vfs_node_t node  = vfs_open(npath);
    if (node == NULL) return SYSCALL_FAULT_(ENOENT);
    if (node->type != file_none && node->type != file_symlink)
    {
        vfs_close(node);
        return SYSCALL_FAULT_(ENOTDIR);
    }
    size_t ret;
    if (node->refcount > 1)
    {
        node->refcount--;
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
    char      *n_name = vfs_cwd_path_build(name);
    vfs_node_t node   = vfs_open(n_name);
    if (node == NULL) return SYSCALL_FAULT_(ENOENT);
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
    // if (check_user_overflow((uint64_t)name, strlen(name))) { return (uint64_t)-EFAULT; }
    char *path = at_resolve_pathname(dirfd, (char *)name);
    if (!path) return -ENOENT;

    uint64_t ret = sys_unlink((uint64_t)path, 0, 0, 0, 0, 0, regs);

    free(path);

    return ret;
}

sys_(access, char *filename)
{
    struct stat buf;
    return sys_stat(filename, &buf, 0, 0, 0, 0, regs);
}

sys_(faccessat)
{
    int      dirfd    = arg0;
    char    *pathname = (char *)arg1;
    uint64_t mode     = arg2;
    if (pathname[0] == '\0')
    { // by fd
        return 0;
    }
    // if (check_user_overflow((uint64_t)pathname, strlen(pathname))) { return SYSCALL_FAULT_(EFAULT); }

    char *resolved = at_resolve_pathname(dirfd, (char *)pathname);
    if (resolved == NULL) return SYSCALL_FAULT_(ENOENT);

    size_t ret = sys_access(resolved, mode, 0, 0, 0, 0, regs);

    free(resolved);

    return ret;
}

sys_(faccessat2)
{
    uint64_t dirfd    = arg0;
    char    *pathname = (char *)arg1;
    uint64_t mode     = arg2;
    uint64_t flag     = arg3;
    if (pathname[0] == '\0')
    { // by fd
        return 0;
    }
    // if (check_user_overflow((uint64_t)pathname, strlen(pathname))) { return SYSCALL_FAULT_(EFAULT); }

    char *resolved = at_resolve_pathname(dirfd, (char *)pathname);
    if (resolved == NULL) return SYSCALL_FAULT_(ENOENT);

    size_t ret = sys_access(resolved, mode, 0, 0, 0, 0, regs);

    free(resolved);

    return ret;
}

sys_(openat)
{
    int      dirfd = arg0;
    char    *name  = (char *)arg1;
    uint64_t flags = arg2;
    uint64_t mode  = arg3;

    // if (unlikely(!name || check_user_overflow((uint64_t)name, strlen(name)))) { return SYSCALL_FAULT_(EFAULT); }
    char *path = at_resolve_pathname(dirfd, (char *)name);
    if (!path) return SYSCALL_FAULT_(ENOMEM);

    uint64_t ret = sys_open(path, flags, mode, 0, 0, 0, regs);

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

    char      *ndir_name = vfs_cwd_path_build(dir_name);
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
        char      *old_root_p = vfs_cwd_path_build(dev_name);
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

    char            *ndev_name;
    vfs_filesystem_t filesystem = get_filesystem(type);
    if (filesystem == NULL) return SYSCALL_FAULT_(EINVAL);
    if (filesystem->id != 0)
    {
        ndev_name = (char *)filesystem->id;
        goto mount;
    }

    ndev_name = vfs_cwd_path_build(dev_name);
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
    if (count > 0)
    {
        gid_list[0] = 0;
        return 1;
    }
    return 0;
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
    // if (check_user_overflow((uint64_t)oldpath, strlen(oldpath))) { return SYSCALL_FAULT_(EFAULT); }
    // if (check_user_overflow((uint64_t)newpath, strlen(newpath))) { return SYSCALL_FAULT_(EFAULT); }
    char      *noldpath = vfs_cwd_path_build(oldpath);
    char      *nnewpath = vfs_cwd_path_build(newpath);
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
    // if (check_user_overflow((uint64_t)name, strlen(name))) { return SYSCALL_FAULT_(EFAULT); }
    errno_t ret = vfs_symlink(name, nw);
    return ret;
}

sys_(link, char *name, char *nw)
{
    // if (check_user_overflow((uint64_t)name, strlen(name))) { return SYSCALL_FAULT_(EFAULT); }
    errno_t ret = vfs_link(name, nw);

    return ret;
}

sys_(umount2)
{
    char   *path   = normalize_path((char *)arg0);
    int     flags  = arg1;
    errno_t status = vfs_unmount(path);
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
    // if (check_user_overflow((uint64_t)buf, size)) { return SYSCALL_FAULT_(EFAULT); }

    vfs_node_t node = vfs_open(path);
    if (node == NULL) { return SYSCALL_FAULT_(ENOENT); }
    if (!node->linkto) return SYSCALL_FAULT_(EINVAL);

    return vfs_readlink(node, buf, (size_t)size);
}


sys_(getgid)
{
    return get_current_task()->parent_group->pid;
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
    // if (check_user_overflow(addr, size)) { return SYSCALL_FAULT_(EFAULT); }

    if (size == 0) { return 0; }

    uint64_t start_page = addr & (~(PAGE_SIZE - 1));
    uint64_t end_page   = (addr + size - 1) & (~(PAGE_SIZE - 1));
    uint64_t num_pages  = ((end_page - start_page) / PAGE_SIZE) + 1;

    // if (check_user_overflow(vec, num_pages)) { return SYSCALL_FAULT_(EFAULT); }

    spin_lock(&mm_op_lock);
    uint64_t current_addr = start_page;

    for (uint64_t i = 0; i < num_pages; i++)
    {
        uint64_t phys_addr = page_virt_to_phys(current_addr);

        uint8_t resident = (phys_addr != 0) ? 1 : 0;

        memcpy((void *)(vec + i), &resident, sizeof(uint8_t));

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
    vfs_node_t node = vfs_open(path);
    if (node == NULL) return SYSCALL_FAULT_(ENOENT);
    vfs_filesystem_t filesystem = get_filesystem_node(node);
    if (filesystem == NULL) return SYSCALL_FAULT_(EINVAL);
    buf->f_type = filesystem->magic;
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
