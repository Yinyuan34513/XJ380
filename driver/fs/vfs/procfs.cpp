#define ALL_IMPLEMENTATION
#include <procfs.h>
#include <efi/boot.h>
#include <errno.h>
#include <task/pcb.h>
#include <task/scheduler.h>
#include <fs/vfs/vfs.h>
#include <syscall/syscall.h>
#include <proto.hpp>
#include <user/user.h>

static int procfs_id     = 0;
static int proc_self_id  = 0;
vfs_node_t procfs_root   = NULL;
spin_t     procfs_oplock = SPIN_INIT;

const char filesystems_content[] = //"nodev\tsysfs\n"
    "nodev\ttmpfs\n"
    "nodev\tproc\n"
    "nodev\tmodfs\n"
    "     \text4\n"
    "     \text3\n"
    "     \text2\n";

static int dummy() {
    return 0;
}

static int udummy() {
    return VFS_STATUS_FAILED;
}

static const char *procfs_kernel_cmdline()
{
    return "";
}

static uint32_t proc_task_uid(pcb_t task)
{
    if (task == NULL) return 0;
    if (task->thread_queue != NULL)
    {
        queue_foreach(task->thread_queue, node)
        {
            tcb_t thread = (tcb_t)node->data;
            if (thread != NULL && thread->user_info != NULL) return user_uid(thread->user_info);
        }
    }
    return user_uid(task_effective_user());
}

static uint32_t proc_task_gid(pcb_t task)
{
    if (task == NULL) return 0;
    if (task->thread_queue != NULL)
    {
        queue_foreach(task->thread_queue, node)
        {
            tcb_t thread = (tcb_t)node->data;
            if (thread != NULL && thread->user_info != NULL) return user_gid(thread->user_info);
        }
    }
    return user_gid(task_effective_user());
}

static void procfs_set_owner(vfs_node_t node, pcb_t task)
{
    if (node == NULL) return;
    node->owner = proc_task_uid(task);
    node->group = proc_task_gid(task);
}

static size_t procfs_copy_content(void *addr, size_t offset, size_t size, const char *content, size_t content_len)
{
    if (addr == NULL || content == NULL) return VFS_STATUS_FAILED;
    if (offset >= content_len) return 0;
    size_t to_copy = MIN(size, content_len - offset);
    memcpy(addr, content + offset, to_copy);
    return to_copy;
}

static tcb_t proc_task_first_thread(pcb_t task)
{
    if (task == NULL || task->thread_queue == NULL) return NULL;
    queue_foreach(task->thread_queue, node)
    {
        tcb_t thread = (tcb_t)node->data;
        if (thread != NULL) return thread;
    }
    return NULL;
}

static char proc_task_state(pcb_t task)
{
    if (task == NULL) return 'T';
    switch (task->status)
    {
    case RUNNING: return 'R';
    case WAIT:
    case FUTEX:
    case START:
    case CREATE: return 'S';
    case ZOMBIE: return 'Z';
    case DEATH:
    case OUT:
    default: return 'T';
    }
}

static uint64_t proc_thread_signal(pcb_t task)
{
    uint64_t signals = 0;
    if (task == NULL || task->thread_queue == NULL) return 0;
    queue_foreach(task->thread_queue, node)
    {
        tcb_t thread = (tcb_t)node->data;
        if (thread != NULL) signals |= thread->signal;
    }
    return signals;
}

static uint64_t proc_thread_blocked(pcb_t task)
{
    uint64_t blocked = 0;
    if (task == NULL || task->thread_queue == NULL) return 0;
    queue_foreach(task->thread_queue, node)
    {
        tcb_t thread = (tcb_t)node->data;
        if (thread != NULL) blocked |= thread->blocked;
    }
    return blocked;
}

static void proc_task_vma_bounds(pcb_t task, uint64_t *start_code, uint64_t *end_code,
                                 uint64_t *start_data, uint64_t *end_data)
{
    if (start_code) *start_code = 0;
    if (end_code) *end_code = 0;
    if (start_data) *start_data = 0;
    if (end_data) *end_data = 0;
    if (task == NULL) return;

    const uint64_t invalid_addr = ~0ULL;
    uint64_t code_start = invalid_addr;
    uint64_t data_start = invalid_addr;
    uint64_t code_end = 0;
    uint64_t data_end = 0;

    for (vma_t *vma = task->vma_manager.vma_list; vma != NULL; vma = vma->vm_next)
    {
        if (vma->vm_flags & VMA_EXEC)
        {
            code_start = MIN(code_start, (uint64_t)vma->vm_start);
            code_end = MAX(code_end, (uint64_t)vma->vm_end);
        }
        if ((vma->vm_flags & VMA_WRITE) && !(vma->vm_flags & VMA_EXEC))
        {
            data_start = MIN(data_start, (uint64_t)vma->vm_start);
            data_end = MAX(data_end, (uint64_t)vma->vm_end);
        }
    }

    if (code_start == invalid_addr) code_start = task->load_start;
    if (code_end == 0 && task->load_start != 0) code_end = task->load_start + task->elf_size;
    if (data_start == invalid_addr) data_start = task->brk_start;
    if (data_end == 0) data_end = task->brk_current;

    if (start_code) *start_code = code_start;
    if (end_code) *end_code = code_end;
    if (start_data) *start_data = data_start;
    if (end_data) *end_data = data_end;
}

const char *get_vma_permissions(vma_t *vma) {
    static char perms[5];

    perms[0] = (vma->vm_flags & VMA_READ) ? 'r' : '-';
    perms[1] = (vma->vm_flags & VMA_WRITE) ? 'w' : '-';
    perms[2] = (vma->vm_flags & VMA_EXEC) ? 'x' : '-';
    perms[3] = (vma->vm_flags & VMA_SHARED) ? 's' : 'p';
    perms[4] = '\0';

    return perms;
}

char *proc_gen_maps_file(pcb_t task, size_t *content_len) {
    vma_t *vma = task->vma_manager.vma_list;

    size_t offset  = 0;
    size_t ctn_len = PAGE_SIZE;
    char  *buf     = (char*)malloc(ctn_len);

    while (vma) {
        vfs_node_t node = NULL;
        if (vma->vm_fd != -1) {
            fd_file_handle *fd_handle =
                (fd_file_handle*)queue_get(task->file_open, vma->vm_fd);
            if (fd_handle != NULL) node = fd_handle->node;
        }

        int len = sprintf(buf + offset, "%012lx-%012lx %s %08lx %02x:%02x %lu", vma->vm_start,
                          vma->vm_end, get_vma_permissions(vma), (unsigned long)vma->vm_offset, 0,
                          0, node ? node->inode : 0);

        if (offset + len > ctn_len) {
            ctn_len = (offset + len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
            buf     = (char*)realloc(buf, ctn_len);
        }
        offset += len;

        const char *pathname = vma->vm_name;
        if (pathname && strlen(pathname) > 0) {
            len = sprintf(buf + offset, "%*s%s", 15, "", pathname);
            if (offset + len > ctn_len) {
                ctn_len = (offset + len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
                buf     = (char*)realloc(buf, ctn_len);
            }
            offset += len;
        }

        len = sprintf(buf + offset, "\n");
        if (offset + len > ctn_len) {
            ctn_len = (offset + len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
            buf     = (char*)realloc(buf, ctn_len);
        }
        offset += len;

        vma = vma->vm_next;
    }

    *content_len = offset;

    return buf;
}

char *proc_gen_status_file(pcb_t task, size_t *content_len) {
    uint32_t uid = proc_task_uid(task);
    uint32_t gid = proc_task_gid(task);
    const char *state = task->status == RUNNING  ? "R (running)"
                      : task->status == ZOMBIE ? "Z (zombie)"
                      : task->status == FUTEX  ? "S (sleeping)"
                                               : "T (stopped)";
    char *buffer = (char*)malloc(PAGE_SIZE);
    int len = sprintf(buffer,
                      "Name:\t%s\n"
                      "Pid:\t%llu\n"
                      "PPid:\t%llu\n"
                      "State:\t%s\n"
                      "Uid:\t%u\t%u\t%u\t%u\n"
                      "Gid:\t%u\t%u\t%u\t%u\n"
                      "Threads:\t%llu\n"
                      "VmSize:\t%llu kB\n",
                      task->name,
                      (unsigned long long)task->pid,
                      (unsigned long long)(task->parent_task ? task->parent_task->pid : 0),
                      state,
                      uid, uid, uid, uid,
                      gid, gid, gid, gid,
                      (unsigned long long)(task->thread_queue ? task->thread_queue->size : 0),
                      (unsigned long long)(task->vma_manager.vm_total / 1024));
    *content_len = len;
    return buffer;
}

char *proc_gen_stat_file(pcb_t task, size_t *content_len) {
    tcb_t main_thread = proc_task_first_thread(task);
    uint64_t start_code = 0;
    uint64_t end_code = 0;
    uint64_t start_data = 0;
    uint64_t end_data = 0;
    proc_task_vma_bounds(task, &start_code, &end_code, &start_data, &end_data);

    uint64_t signal_pending = proc_thread_signal(task);
    uint64_t blocked = proc_thread_blocked(task);
    uint64_t sigignore = 0;
    uint64_t sigcatch = 0;
    if (main_thread != NULL)
    {
        for (size_t i = 0; i < MAX_SIGNALS; i++)
        {
            if (main_thread->actions[i].sa_handler == SIG_IGN) sigignore |= (1ULL << i);
            else if (main_thread->actions[i].sa_handler != SIG_DFL &&
                     main_thread->actions[i].sa_handler != NULL)
            {
                sigcatch |= (1ULL << i);
            }
        }
    }

    uint64_t vsize = task->vma_manager.vm_total;
    if (vsize == 0 && task->brk_current > task->brk_start) vsize = task->brk_current - task->brk_start;
    long rss = (long)((task->vma_manager.vm_used + PAGE_SIZE - 1) / PAGE_SIZE);
    if (rss == 0 && vsize != 0) rss = (long)((vsize + PAGE_SIZE - 1) / PAGE_SIZE);

    uint64_t start_stack = main_thread ? main_thread->user_stack : 0;
    uint64_t kstkesp = main_thread ? main_thread->kernel_stack : 0;
    uint64_t ksteip = main_thread ? main_thread->context0.rip : 0;
    uint32_t processor = main_thread ? (uint32_t)main_thread->cpu_id : 0;
    uint32_t flags = task->linux_abi ? 0x00400000u : 0;
    int exit_signal = task->status == ZOMBIE ? SIGCHLD : 0;
    int priority = task->task_level == TASK_KERNEL_LEVEL ? 5 : 6;
    size_t num_threads = task->thread_queue ? task->thread_queue->size : 0;

    char *buffer = (char*)malloc(PAGE_SIZE * 4);
    int   len    = sprintf(buffer,
                           "%d (%s) %c %d %d %d %d %d %u %lu %lu %lu %lu %lu %lu %ld %ld %ld %ld "
                                "%ld %ld %llu %lu %ld %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu "
                                "%lu %d %d %u %u %llu %lu %ld %lu %lu %lu %lu %lu %lu %lu %d\n",
                           (int)task->pid,  // pid
                           task->name, // name
                           proc_task_state(task),                        // state
                           task->parent_task ? (int)task->parent_task->pid : 0, // ppid
                           (int)task->pid,                              // pgrp
                           task->parent_task ? (int)task->parent_task->pid : (int)task->pid, // session
                           0,                                            // tty_nr
                           (int)task->pid,                              // tpgid
                           flags,                                        // flags
                           0,                                            // minflt
                           0,                                            // cminflt
                           0,                                            // majflt
                           0,                                            // cmajflt
                           0,                                            // utime
                           0,                                            // stime
                           0,                                            // cutime
                           0,                                            // cstime
                           priority,                                     // priority
                           0,                                            // nice
                           num_threads,                                  // num_threads
                           0,                                            // itrealvalue
                           0,                                            // starttime
                           vsize,                                        // vsize
                           rss,                                          // rss
                           0,                                            // rsslim
                           start_code,                                   // startcode
                           end_code,                                     // endcode
                           start_stack,                                  // startstack
                           kstkesp,                                      // kstkesp
                           ksteip,                                       // ksteip
                           signal_pending,                               // signal
                           blocked,                                      // blocked
                           sigignore,                                    // sigignore
                           sigcatch,                                     // sigcatch
                           0,                                            // wchan
                           0,                                            // nswap
                           0,                                            // cnswap
                           exit_signal,                                  // exit_signal
                           processor,                                    // processor
                           0,                                            // rt_priority
                           0,                                            // policy
                           0,                                            // delayacct_blkio_ticks
                           0,                                            // guest_time
                           0,                                            // cguest_time
                           start_data,                                   // start_data
                           end_data,                                     // end_data
                           task->brk_start,                              // start_brk
                           0,                                            // arg_start
                           0,                                            // arg_end
                           0,                                            // env_start
                           0,                                            // env_end
                           task->exit_code                               // exit_code
         );

    *content_len = len;

    return buffer;
}

char *proc_gen_mounts(size_t *context_len) {
    char *mount_info =
        "dev /dev devfs rw,nosuid,relatime,mode=755,inode64 0 0\n"
        "proc /proc proc rw,nosuid,nodev,noexec,relatime 0 0\n"
        "tmpfs /tmp tmpfs rw,nosuid,size=8040232k,nr_inodes=1048576,nodev,inode64,usrquota 0 0";
    *context_len = strlen(mount_info);
    return strdup(mount_info);
}

extern lock_queue   *pcb_group_queue;

errno_t procfs_mount(const char *src, vfs_node_t node) {
    if (src != (void *)PROC_REGISTER_ID) return VFS_STATUS_FAILED;
    procfs_root = node;

    procfs_root->fsid = procfs_id;
    procfs_root->owner = 0;
    procfs_root->group = 0;

    vfs_node_t procfs_self = vfs_node_alloc(procfs_root, "self");
    procfs_self->type      = file_symlink;
    procfs_self->mode      = 0644;
    procfs_self->linkto    = NULL;
    procfs_self->fsid      = proc_self_id;
    procfs_self->owner     = 0;
    procfs_self->group     = 0;

    vfs_node_t cmdline    = vfs_node_alloc(procfs_root, "cmdline");
    cmdline->type         = file_none;
    cmdline->mode         = 0444;
    cmdline->owner        = 0;
    cmdline->group        = 0;
    proc_handle_t *handle = (proc_handle_t*)malloc(sizeof(proc_handle_t));
    cmdline->handle       = handle;
    handle->task          = NULL;
    sprintf(handle->name, "cmdline");

    vfs_node_t mounts       = vfs_node_alloc(procfs_root, "mounts");
    mounts->type            = file_none;
    mounts->mode            = 0444;
    mounts->owner           = 0;
    mounts->group           = 0;
    proc_handle_t *mounts_h = (proc_handle_t*)malloc(sizeof(proc_handle_t));
    mounts->handle          = mounts_h;
    mounts_h->task          = NULL;
    sprintf(mounts_h->name, "mounts");

    vfs_node_t filesystems            = vfs_node_alloc(procfs_root, "filesystems");
    filesystems->type                 = file_none;
    filesystems->mode                 = 0444;
    filesystems->owner                = 0;
    filesystems->group                = 0;
    proc_handle_t *filesystems_handle = (proc_handle_t*)malloc(sizeof(proc_handle_t));
    filesystems->handle               = filesystems_handle;
    filesystems_handle->task          = NULL;
    sprintf(filesystems_handle->name, "filesystems");

    procfs_update_task_list();

    return VFS_STATUS_SUCCESS;
}

void procfs_open(void *parent, const char *name, vfs_node_t node) {
    UNUSED(parent, name);
    if (node == procfs_root) procfs_update_task_list();
}

void procfs_close(void *current) {
    UNUSED(current);
}

size_t procfs_readlink(vfs_node_t node, void *addr, size_t offset, size_t size) {
    UNUSED(node, addr, offset, size);
    return 0;
}

size_t procfs_write(void *file, const void *addr, size_t offset, size_t size) {
    UNUSED(file, addr, offset, size);
    return size;
}

size_t procfs_read(void *file, void *addr, size_t offset, size_t size) {
    proc_handle_t *handle = (proc_handle_t *)file;
    if (!handle) { return VFS_STATUS_FAILED; }
    pcb_t task;
    if (handle->task == NULL) {
        task = get_current_task()->parent_group;
    } else {
        task = handle->task;
    }

    if (!strcmp(handle->name, "filesystems")) {
        size_t fs_size = strlen(filesystems_content);
        return procfs_copy_content(addr, offset, size, filesystems_content, fs_size);
    } else if (!strcmp(handle->name, "cmdline")) {
        size_t len = strlen(procfs_kernel_cmdline());
        return procfs_copy_content(addr, offset, size, procfs_kernel_cmdline(), len);
    } else if (!strcmp(handle->name, "mounts")) {
        size_t len     = 0;
        char  *contect = proc_gen_mounts(&len);
        size_t copied = procfs_copy_content(addr, offset, size, contect, len);
        free(contect);
        return copied;
    } else if (!strcmp(handle->name, "proc_cmdline")) {
        const char *cmdline = task->cmdline ? task->cmdline : "no_cmdline";
        ssize_t len     = strlen(cmdline);
        return procfs_copy_content(addr, offset, size, cmdline, len);
    } else if (!strcmp(handle->name, "proc_maps")) {
        size_t content_len = 0;
        char  *content     = proc_gen_maps_file(handle->task, &content_len);
        size_t to_copy = procfs_copy_content(addr, offset, size, content, content_len);
        free(content);
        return to_copy;
    } else if (!strcmp(handle->name, "dri_name")) {
        char name[] = "cpkernel_drm";
        int  len    = strlen(name);
        return procfs_copy_content(addr, offset, size, name, len);
    } else if (!strcmp(handle->name, "proc_stat")) {
        size_t content_len = 0;
        char  *content     = proc_gen_stat_file(task, &content_len);
        size_t to_copy = procfs_copy_content(addr, offset, size, content, content_len);
        free(content);
        return to_copy;
    } else if (!strcmp(handle->name, "proc_status")) {
        size_t content_len = 0;
        char  *content     = proc_gen_status_file(task, &content_len);
        size_t to_copy = procfs_copy_content(addr, offset, size, content, content_len);
        free(content);
        return to_copy;
    }
    return VFS_STATUS_FAILED;
}

vfs_node_t procfs_dup(vfs_node_t src) {
    return src;
}

errno_t procfs_stat(void *file, vfs_node_t node) {
    if (file == NULL) return EOK;
    proc_handle_t *handle = (proc_handle_t *)file;
    if (!strcmp(handle->name, "filesystems"))
        node->size = strlen(filesystems_content);
    else if (!strcmp(handle->name, "cmdline"))
        node->size = strlen(procfs_kernel_cmdline());
    else if (!strcmp(handle->name, "proc_cmdline"))
        node->size = strlen(handle->task->cmdline ? handle->task->cmdline : "null");
    else if (!strcmp(handle->name, "proc_maps")) {
        size_t content_len = 0;
        char  *content     = proc_gen_maps_file(handle->task, &content_len);
        free(content);
        node->size = content_len;
    } else if (!strcmp(handle->name, "dri_name"))
        node->size = strlen("cpkernel_drm");
    else if (!strcmp(handle->name, "proc_stat")) {
        size_t content_len = 0;
        char  *content     = proc_gen_stat_file(handle->task, &content_len);
        node->size         = content_len;
        free(content);
    } else if (!strcmp(handle->name, "proc_status")) {
        size_t content_len = 0;
        char  *content     = proc_gen_status_file(handle->task, &content_len);
        node->size         = content_len;
        free(content);
    } else if (!strcmp(handle->name, "mounts")) {
        size_t content_len = 0;
        char  *content     = proc_gen_mounts(&content_len);
        free(content);
        node->size = content_len;
    }
    return EOK;
}

void procfs_self_open(void *parent, const char *name, vfs_node_t node) {
    UNUSED(parent, name);
    procfs_self_handle_t *handle = (procfs_self_handle_t *)malloc(sizeof(procfs_self_handle_t));
    handle->self                 = node;
    node->linkto                 = get_current_task()->parent_group->procfs_node;
    node->handle                 = handle;
}

void procfs_self_close(void *current) {
    procfs_self_handle_t *handle  = (procfs_self_handle_t *)current;
    if (handle != NULL && handle->self != NULL)
    {
        handle->self->handle = NULL;
        handle->self->linkto = NULL;
    }
    free(handle);
}

size_t procfs_self_read(void *fd, void *addr, size_t offset, size_t size) {
    procfs_self_handle_t *handle = (procfs_self_handle_t *)fd;
    return procfs_read(handle->self->linkto->handle, addr, offset, size);
}

size_t procfs_self_write(void *fd, const void *addr, size_t offset, size_t size) {
    procfs_self_handle_t *handle = (procfs_self_handle_t *)fd;
    return procfs_write(handle->self->linkto->handle, addr, offset, size);
}

size_t procfs_self_readlink(vfs_node_t file, void *addr, size_t offset, size_t size) {
    procfs_self_handle_t *handle = (procfs_self_handle_t *)file->handle;
    if (handle == NULL || handle->self == NULL || handle->self->linkto == NULL) return VFS_STATUS_FAILED;
    char target[MAX_PID_NAME_LEN + 8];
    sprintf(target, "/proc/%s", handle->self->linkto->name);
    return procfs_copy_content(addr, offset, size, target, strlen(target));
}

errno_t procfs_self_stat(void *file, vfs_node_t node) {
    procfs_self_handle_t *handle  = (procfs_self_handle_t *)file;
    node->type                   |= file_symlink;
    node->size                    = handle && handle->self && handle->self->linkto ?
                                    strlen(handle->self->linkto->name) + strlen("/proc/") : 0;
    return EOK;
}

void procfs_self_free_handle(procfs_self_handle_t *handle) {
    free(handle);
}

static struct vfs_callback procfs_self_callbacks = {
    .mount    = (vfs_mount_t)udummy,
    .unmount  = (vfs_unmount_t)dummy,
    .open     = procfs_self_open,
    .close    = procfs_self_close,
    .read     = procfs_self_read,
    .write    = procfs_self_write,
    .readlink = procfs_self_readlink,
    .mkdir    = (vfs_mk_t)dummy,
    .mkfile   = (vfs_mk_t)dummy,
    .link     = (vfs_mk_t)dummy,
    .symlink  = (vfs_mk_t)dummy,
    .stat     = procfs_self_stat,
    .ioctl    = (vfs_ioctl_t)dummy,
    .dup      = (vfs_dup_t)dummy,
    .poll     = (vfs_poll_t)dummy,
    .map      = (vfs_mapfile_t)dummy,
    .del      = (vfs_del_t)dummy,
    .rename   = (vfs_rename_t)dummy,
};

static struct vfs_callback procfs_callbacks = {
    .mount    = procfs_mount,
    .unmount  = (vfs_unmount_t)dummy,
    .open     = procfs_open,
    .close    = procfs_close,
    .read     = procfs_read,
    .write    = procfs_write,
    .readlink = procfs_readlink,
    .mkdir    = (vfs_mk_t)dummy,
    .mkfile   = (vfs_mk_t)dummy,
    .link     = (vfs_mk_t)dummy,
    .symlink  = (vfs_mk_t)dummy,
    .stat     = procfs_stat,
    .ioctl    = (vfs_ioctl_t)dummy,
    .dup      = procfs_dup,
    .poll     = (vfs_poll_t)dummy,
    .map      = (vfs_mapfile_t)dummy,
    .del      = (vfs_del_t)dummy,
    .rename   = (vfs_rename_t)dummy,
};

void procfs_setup() {
    procfs_id    = vfs_regist("proc", &procfs_callbacks, PROC_REGISTER_ID, 0x9fa0);
    proc_self_id = vfs_regist("proc_self", &procfs_self_callbacks, PROC_REGISTER_ID, 0x0);
    vfs_node_t proot;
    vfs_mkdir("/proc");
    proot=vfs_open("/proc");
    procfs_mount((const char*)PROC_REGISTER_ID,proot);
    if (procfs_id == VFS_STATUS_FAILED || proc_self_id == VFS_STATUS_FAILED) {
        write_serial_fmt("procfs register error\n");
    }
}

void procfs_update_task_list() {
    if (procfs_root == NULL || pcb_group_queue == NULL) return;
    queue_foreach(pcb_group_queue, node)
    {
        procfs_on_new_task((pcb_t)node->data);
    }
}

void procfs_on_new_task(pcb_t task) {
    if (procfs_root == NULL || task == NULL) return;

    char name[MAX_PID_NAME_LEN];
    sprintf(name, "%llu", (unsigned long long)task->pid);

    vfs_node_t pro = vfs_do_search(procfs_root, name);
    if (pro != NULL) {
        task->procfs_node = pro;
        return;
    }

    vfs_node_t node   = vfs_child_append(procfs_root, name, NULL);
    node->type        = file_dir;
    node->mode        = 0555;
    node->fsid        = procfs_id;
    procfs_set_owner(node, task);
    task->procfs_node = node;

    vfs_node_t cmdline    = vfs_child_append(node, "cmdline", NULL);
    cmdline->type         = file_none;
    cmdline->mode         = 0444;
    cmdline->fsid         = procfs_id;
    procfs_set_owner(cmdline, task);
    proc_handle_t *handle = (proc_handle_t *)malloc(sizeof(proc_handle_t));
    cmdline->handle       = handle;
    handle->task          = task;
    sprintf(handle->name, "proc_cmdline");

    vfs_node_t mmaps        = vfs_child_append(node, "maps", NULL);
    mmaps->type             = file_none;
    mmaps->mode             = 0400;
    mmaps->fsid             = procfs_id;
    procfs_set_owner(mmaps, task);
    proc_handle_t *handle_m = (proc_handle_t *)malloc(sizeof(proc_handle_t));
    mmaps->handle           = handle_m;
    handle_m->task          = task;
    sprintf(handle_m->name, "proc_maps");

    vfs_node_t self_stat            = vfs_child_append(node, "stat", NULL);
    self_stat->type                 = file_none;
    self_stat->mode                 = 0444;
    self_stat->fsid                 = procfs_id;
    procfs_set_owner(self_stat, task);
    proc_handle_t *self_stat_handle = (proc_handle_t *)malloc(sizeof(proc_handle_t));
    self_stat->handle               = self_stat_handle;
    self_stat_handle->task          = task;
    sprintf(self_stat_handle->name, "proc_stat");

    vfs_node_t status            = vfs_child_append(node, "status", NULL);
    status->type                 = file_none;
    status->mode                 = 0444;
    status->fsid                 = procfs_id;
    procfs_set_owner(status, task);
    proc_handle_t *status_handle = (proc_handle_t *)malloc(sizeof(proc_handle_t));
    status->handle               = status_handle;
    status_handle->task          = task;
    sprintf(status_handle->name, "proc_status");
}

void procfs_on_exit_task(pcb_t task) {
    if (procfs_root == NULL) return;
    spin_lock(&procfs_oplock);

    char name[MAX_PID_NAME_LEN];
    sprintf(name, "%llu", (unsigned long long)task->pid);

    vfs_node_t node = vfs_do_search(procfs_root, name);
    if (node && node->parent) {
        list_delete(node->parent->child, node);
        vfs_free(node);
        task->procfs_node = NULL;
    }

    spin_unlock(&procfs_oplock);
}
