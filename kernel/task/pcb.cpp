#include "task/pcb.h"
#include "cpu/regio.h"
#include "krlibc.h"
#include "mm/heap.h"
#include "mm/page.h"
#include "proto.hpp"
#include <cpu/longm.h>
#include <elf.h>
#include <errno.h>
#include <fs/vfs/proc.h>
#include <fs/vfs/vfs.h>
#include <mm/lazyalloc.h>
#include <syscall/syscall.h>
#include <task/ipc.h>

#define SA_RPL3 3

#define SA_RPL_MASK      0xFFFC
#define SA_TI_MASK       0xFFFB
#define GET_SEL(cs, rpl) ((cs & SA_RPL_MASK & SA_TI_MASK) | (rpl))

#define PUSH_STACK(stack, value) *(--stack) = value
static size_t now_pid         = 0;
size_t        now_tid         = 0;
lock_queue   *pcb_group_queue = NULL;
pcb_t         kernel_group    = NULL;

extern UserInfo *current_user;

extern bool no_interrupt;
// pcb.cpp
#include "task/scheduler.h"

extern spin_t scheduler_lock;

uint16_t dock_ct_sheet;

pcb_t found_pcb(int pid)
{
    pcb_t ret = NULL;
    spin_lock(pcb_group_queue->lock);
    queue_foreach(pcb_group_queue, node)
    {
        pcb_t pcb = (pcb_t)node->data;
        if (pcb->pid == pid)
        {
            ret = pcb;
            break;
        }
    }
    spin_unlock(pcb_group_queue->lock);
    return ret;
}

tcb_t found_thread(pcb_t pcb, int tid)
{
    if (pcb == NULL) return NULL;
    queue_foreach(pcb->thread_queue, node)
    {
        tcb_t thread = (tcb_t)node->data;
        if (thread->tid == tid) { return thread; }
    }
    return NULL;
}

void kill_proc(pcb_t pcb, int exit_code, bool is_zombie)
{
    if (pcb == NULL) return;
    if (pcb->pid == kernel_group->pid)
    {
        write_serial_fmt("Cannot kill System process.");
        return;
    }

    // if (get_current_user()->fgproc == pcb->pid) { get_current_user()->fgproc = 0; }

    if (is_zombie)
    {
        if (pcb->thread_queue->size > 0)
        {
            spin_lock(pcb->thread_queue->lock);
            queue_foreach(pcb->thread_queue, node)
            {
                tcb_t tcb = (tcb_t)node->data;
                kill_thread(tcb);
            }
            spin_unlock(pcb->thread_queue->lock);
        }

        pcb->status       = ZOMBIE;
        ipc_message_t msg = (ipc_message_t)malloc(sizeof(struct ipc_message));
        msg->pid          = pcb->pid;
        msg->type         = IPC_MSG_TYPE_EPID;
        msg->data[0]      = exit_code & 0xFF;
        msg->data[1]      = (exit_code >> 8) & 0xFF;
        msg->data[2]      = (exit_code >> 16) & 0xFF;
        msg->data[3]      = (exit_code >> 24) & 0xFF;
        ipc_send(pcb->parent_task, msg);
    }
    else
    {
        queue_remove_at(pcb->parent_task->child_pcb, pcb->child_index);
        pcb->status = DEATH;
        kill_proc0(pcb);
    }
}

void free_envp(char **envp)
{
    if (!envp) return;
    for (size_t i = 0; envp[i] != NULL; i++)
    {
        free(envp[i]);
    }
    free(envp);
}

void kill_proc0(pcb_t pcb)
{
    do
    {
        tcb_t thread = NULL;
        queue_foreach(pcb->thread_queue, thread0)
        {
            thread = (tcb_t)thread0->data;
        }
        if (thread == NULL) break;
        queue_remove_at(pcb->thread_queue, thread->group_index);
        kill_thread0(thread);
        free(thread);
    } while (true);

    queue_destroy(pcb->thread_queue);
    queue_remove_at(pcb_group_queue, pcb->queue_index);

    //procfs_update_task_list();
    procfs_on_exit_task(pcb);

    while (1)
    {
        fd_file_handle *handle = (fd_file_handle *)queue_dequeue(pcb->file_open);
        if (handle == NULL) break;
        vfs_close(handle->node);
        free(handle);
    }

    lazy_free(pcb);

    queue_destroy(pcb->file_open);
    queue_destroy(pcb->ipc_queue);
    queue_destroy(pcb->virt_queue);
    free(pcb->cmdline);
    free(pcb->elf_file);
    if (pcb->envp) free_envp(pcb->envp);
    write_serial_fmt("task: Freeing process %s (PID: %d) vfork: %s\n", pcb->name, pcb->pid,
                     pcb->vfork ? "true" : "false");
    if (!pcb->vfork) free_page_directory(pcb->pagedir);
    free(pcb);
}

void kill_thread(tcb_t task)
{
    if (task == NULL) return;
    if (task->task_level == TASK_IDLE_LEVEL)
    {
        write_serial_fmt("Cannot stop kernel thread.");
        return;
    }
    task->status = DEATH;
}

extern void remove_task(tcb_t task);

void kill_thread0(tcb_t task)
{
    task->status = OUT;
    free_frames((uint64_t)virt_to_phys((task->syscall_stack - KERNEL_STACK_SIZE)), KERNEL_STACK_SIZE / PAGE_SIZE);
    page_directory_t *src_dir = get_current_directory();
    switch_process_page_directory(task->parent_group->pagedir);
    switch_process_page_directory(src_dir);
    remove_task(task);
}

[[noreturn]] void process_exit()
{
    uint64_t rax = 0;
    __asm__("movq %%rax,%0" ::"r"(rax) :);
    write_serial_string("Kernel thread exit, Code: ");
    write_serial_dec(rax);
    write_serial_string("\n");
    kill_thread(get_current_task());
    open_interrupt;
    //    kill_thread(get_current_task()->tid);
    while (true)
        __asm__ volatile("hlt");
}

static uint64_t push_slice(uint64_t ustack, uint8_t *slice, uint64_t len)
{
    uint64_t tmp_stack  = ustack;
    tmp_stack          -= len;
    tmp_stack          -= (tmp_stack % 0x08);
    memcpy((void *)tmp_stack, slice, len);
    return tmp_stack;
}

static uint64_t build_user_stack(tcb_t task, uint64_t sp, uint64_t entry_point, uint64_t link_start, uint8_t *link_data,
                                 size_t link_size, uint64_t *ep)
{
    uint64_t env_i  = 0;
    int      argv_i = 0;

    char     *argv[50];
    char     *build_cmdline = strdup(task->parent_group->cmdline);
    const int argc          = cmd_parse(build_cmdline, argv, ' ');

    char **envp = task->parent_group->envp ? task->parent_group->envp : current_user->envp;

    uint64_t tmp_stack = sp;
    tmp_stack          = push_slice(tmp_stack, (uint8_t *)task->name, strlen(task->name) + 1);

    uint64_t execfn_ptr = tmp_stack;

    uint64_t *envps = (uint64_t *)malloc(1024);
    memset(envps, 0, 1024);
    uint64_t *argvps = (uint64_t *)malloc(1024);
    memset(argvps, 0, 1024);

    if (envp != NULL)
    {
        for (env_i = 0; env_i < (task->parent_group->envp ? task->parent_group->envc : current_user->envc); env_i++)
        {
            if (envp[env_i] != NULL)
            {
                // write_serial_fmt("envp[%d]: %s\n",env_i,envp[env_i]);
                tmp_stack    = push_slice(tmp_stack, (uint8_t *)envp[env_i], strlen(envp[env_i]) + 1);
                envps[env_i] = tmp_stack;
            }
        }
    }

    for (argv_i = 0; argv_i < argc; argv_i++)
    {
        tmp_stack      = push_slice(tmp_stack, (uint8_t *)argv[argv_i], strlen(argv[argv_i]) + 1);
        argvps[argv_i] = tmp_stack;
    }

    uint64_t total_length = 2 * sizeof(uint64_t) + 7 * 2 * sizeof(uint64_t) + (env_i + 0) * sizeof(uint64_t) +
                            sizeof(uint64_t) + (argv_i + 0) * sizeof(uint64_t) + sizeof(uint64_t) + sizeof(uint64_t);
    tmp_stack -= (tmp_stack - total_length) % 0x10;

    // push auxv
    uint8_t *tmp = (uint8_t *)malloc(2 * sizeof(uint64_t));
    memset(tmp, 0, 2 * sizeof(uint64_t));
    tmp_stack = push_slice(tmp_stack, tmp, 2 * sizeof(uint64_t));

    // page_map_range_to_random(get_current_directory(), EHDR_START_ADDR, get_current_task()->parent_group->elf_size,
    //                          PTE_PRESENT | PTE_WRITEABLE | PTE_USER);
    // memcpy((void *)EHDR_START_ADDR, get_current_task()->parent_group->elf_file,
    //        get_current_task()->parent_group->elf_size);

    // Elf64_Ehdr *ehdr = (Elf64_Ehdr *)EHDR_START_ADDR;
    // // CP_Kernel 将用户程序本体从 0 地址加载故不加phdrs的偏移
    // Elf64_Phdr *phdrs = (Elf64_Phdr *)(ehdr->e_phoff + get_current_task()->parent_group->load_start);

    ((uint64_t *)tmp)[0] = AT_NULL;
    ((uint64_t *)tmp)[1] = 0;

    ((uint64_t *)tmp)[0] = AT_ENTRY;
    ((uint64_t *)tmp)[1] = entry_point;
    tmp_stack            = push_slice(tmp_stack, tmp, 2 * sizeof(uint64_t));

    ((uint64_t *)tmp)[0] = AT_EXECFN;
    ((uint64_t *)tmp)[1] = execfn_ptr;
    tmp_stack            = push_slice(tmp_stack, tmp, 2 * sizeof(uint64_t));

    ((uint64_t *)tmp)[0] = AT_PAGESZ;
    ((uint64_t *)tmp)[1] = PAGE_SIZE;
    tmp_stack            = push_slice(tmp_stack, tmp, 2 * sizeof(uint64_t));

    memset(tmp, 0, 2 * sizeof(uint64_t));

    tmp_stack = push_slice(tmp_stack, tmp, sizeof(uint64_t));

    tmp_stack = push_slice(tmp_stack, (uint8_t *)envps, env_i * sizeof(uint64_t));
    *ep       = tmp_stack;

    tmp_stack = push_slice(tmp_stack, tmp, sizeof(uint64_t));
    tmp_stack = push_slice(tmp_stack, (uint8_t *)argvps, argv_i * sizeof(uint64_t));

    tmp_stack = push_slice(tmp_stack, (uint8_t *)&argv_i, sizeof(uint64_t));

    free(tmp);
    free(envps);
    free(argvps);
    free(link_data);
    cmd_free(argv, argc);

    return tmp_stack;
}

void switch_to_user_mode()
{
    close_interrupt;

    uint64_t rsp = get_current_task()->user_stack_top;

    // 构建用户栈并获取 argc 和 argv
    uint64_t ep;
    rsp = build_user_stack(get_current_task(), rsp, get_current_task()->main, 0, NULL, 0, &ep);

    // 从栈中读取 argc 和 argv
    uint64_t argc = *(uint64_t *)rsp;
    char   **argv = (char **)(rsp + 8);
    // char** envp = (char**)ep;

    get_current_task()->context0.rflags = 0 << 12 | 0b10 | 1 << 9;
    uint64_t func                       = get_current_task()->main;

    char *new_path = (char *)malloc(strlen("/dev/") + 10);
    sprintf(new_path, "/dev/stdio");
    vfs_node_t stdio        = vfs_open(new_path);
    stdio->refcount        += 3;
    fd_file_handle *stdout  = (fd_file_handle *)calloc(1, sizeof(fd_file_handle));
    stdout->node            = stdio;
    stdout->offset          = 0;
    fd_file_handle *stdin   = (fd_file_handle *)calloc(1, sizeof(fd_file_handle));
    stdin->node             = stdio;
    stdin->offset           = 0;
    fd_file_handle *stderr  = (fd_file_handle *)calloc(1, sizeof(fd_file_handle));
    stderr->node            = stdio;
    stderr->offset          = 0;
    stdin->fd               = queue_enqueue(get_current_task()->parent_group->file_open, stdin);  // stdin
    stdout->fd              = queue_enqueue(get_current_task()->parent_group->file_open, stdout); // stdout
    stderr->fd              = queue_enqueue(get_current_task()->parent_group->file_open, stderr); // stderr

    free(new_path);

    __asm__ volatile("mov %0, %%es\n"
                     "mov %0, %%ds\n"
                     "pushq %5\n"      // SS
                     "pushq %1\n"      // RSP
                     "pushq %2\n"      // RFLAGS
                     "pushq %3\n"      // CS
                     "pushq %4\n"      // RIP
                     "mov %6, %%rdi\n" // argc (arg1)
                     "mov %7, %%rsi\n" // argv (arg2)
                     "mov %8, %%rdx\n" // envp (arg3)
                     "iretq\n"
                     :
                     : "r"((uint64_t)GET_SEL(4 * 8, SA_RPL3)), "r"(rsp), "r"(get_current_task()->context0.rflags),
                       "r"((uint64_t)0x23), "r"(func), "r"((uint64_t)0x1b),
                       "r"(argc),           // %6 -> rdi
                       "r"((uint64_t)argv), // %7 -> rsi
                       "r"((uint64_t)ep)    // %8 -> rdx
                     : "memory");
}

void jump_to_signal(uint64_t msg_type, uint64_t data1, uint64_t data2, uint64_t rf)
{
    close_interrupt;
    uint64_t *rsp = (uint64_t *)get_current_task()->user_stack_top;
    PUSH_STACK(rsp, 0);  // 对齐
    PUSH_STACK(rsp, rf); // 返回程序
    get_current_task()->context0.rflags = 0 << 12 | 0b10 | 1 << 9;
    uint64_t func                       = get_current_task()->main;

    __asm__ volatile("mov %0, %%es\n"
                     "mov %0, %%ds\n"
                     "pushq %5\n"      // SS
                     "pushq %1\n"      // RSP
                     "pushq %2\n"      // RFLAGS
                     "pushq %3\n"      // CS
                     "pushq %4\n"      // RIP
                     "mov %6, %%rdi\n" // arg1
                     "mov %7, %%rsi\n" // arg2
                     "mov %8, %%rdx\n" // arg3
                     "iretq\n"
                     :
                     : "r"((uint64_t)GET_SEL(4 * 8, SA_RPL3)), "r"((uint64_t)rsp),
                       "r"(get_current_task()->context0.rflags), "r"((uint64_t)0x23), "r"(func), "r"((uint64_t)0x1b),
                       "r"(msg_type), "r"(data1), "r"(data2)
                     : "memory");
}

size_t envp_length(char **envp)
{
    size_t count = 0;
    while (envp[count] != NULL)
    {
        count++;
    }
    return count;
}

char **copy_envp(char **envp)
{
    size_t count = 0;
    while (envp[count] != NULL)
    {
        count++;
    }
    char **new_envp = (char **)malloc((count + 1) * sizeof(char *));
    if (!new_envp) return NULL;
    for (size_t i = 0; i < count; i++)
    {
        new_envp[i] = strdup(envp[i]);
        if (!new_envp[i])
        {
            for (size_t j = 0; j < i; j++)
            {
                free(new_envp[j]);
            }
            free(new_envp);
            return NULL;
        }
    }
    new_envp[count] = NULL;
    return new_envp;
}

extern bool no_interrupt;
extern bool is_scheduler;

uint64_t process_execve(char *path, char **argv, char **envp)
{
    while (true)
    {
        if (is_scheduler) break;
    }

    write_serial_fmt("path in execve %s\n", path);
    char      *norm_path = strdup(path);
    vfs_node_t node      = vfs_open(norm_path);
    if (node == NULL) { return (uint64_t)-ENOENT; }
    uint64_t buf_len = (node->size + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1));

    pcb_t process = get_current_task()->parent_group;

    close_interrupt;
    disable_scheduler();
    char cmdline[PAGE_SIZE];
    memset(cmdline, 0, sizeof(cmdline));
    char *cmdline_ptr = cmdline;

    write_serial_fmt("norm path %s\n", norm_path);
    if (argv != NULL)
    {
        for (int i = 0; argv[i]; i++)
        {
            int len      = sprintf(cmdline_ptr, "%s ", argv[i]);
            cmdline_ptr += len;
        }

        char *old_cmdline = process->cmdline;
        process->cmdline  = strdup(cmdline);
    }

    get_thread_name_from_filepath(norm_path, process->name);

    if (envp != NULL)
    {
        char **old_envp = process->envp;
        size_t old_envc = process->envc;
        process->envp   = copy_envp(envp);
        process->envc   = envp_length(envp);
    }
    else
    {
        process->envp = copy_envp(current_user->envp);
        process->envc = current_user->envc;
    }

    if (!process->vfork) vma_manager_exit_cleanup(&process->vma_manager);

    write_serial_fmt(" process name :%s \n", process->name);

    page_directory_t *old_page_dir = process->pagedir;
    switch_process_page_directory(clone_page_directory(get_current_directory(), false));

    uint8_t *pcb_buffer = (uint8_t *)malloc(node->size);
    vfs_read(node, pcb_buffer, 0, node->size);
    uint64_t e_entry    = 0;
    uint64_t load_start = 0;
    e_entry             = (uint64_t)parse_elf_file(path, process);
    if (e_entry == NULL)
    {
        switch_page_directory(old_page_dir);
        return 0;
    }
    if (process->vfork)
    {
        ipc_message_t message = (ipc_message_t)calloc(1, sizeof(struct ipc_message));
        message->type         = IPC_MSG_TYPE_EXEC;
        message->pid          = process->pid;
        ipc_send(process->parent_task, message);
    }
    if (!process->vfork) free_page_directory(old_page_dir);
    process->pagedir = get_current_directory();
    process->vfork   = false;

    vfs_close(node);

    spin_lock(process->file_open->lock);
    queue_foreach(process->file_open, node)
    {
        fd_file_handle *file_node = (fd_file_handle *)node->data;
        vfs_close(file_node->node);
        free(file_node);
    }
    spin_unlock(process->file_open->lock);
    queue_destroy(process->file_open);
    process->file_open = queue_init();
    spin_lock(process->virt_queue->lock);
    queue_foreach(process->virt_queue, node)
    {
        mm_virtual_page_t *vpage = (mm_virtual_page_t *)node->data;
        free(vpage);
    }
    spin_unlock(process->virt_queue->lock);
    queue_destroy(process->virt_queue);
    process->virt_queue = queue_init();

    spin_lock(process->ipc_queue->lock);
    queue_foreach(process->ipc_queue, node)
    {
        ipc_message_t msg = (ipc_message_t)node->data;
        free(msg);
    }
    spin_unlock(process->ipc_queue->lock);
    queue_destroy(process->ipc_queue);
    process->ipc_queue = queue_init();

    free(norm_path);

    uint64_t stack = page_alloc_random(get_current_directory(), BIG_USER_STACK, PTE_PRESENT | PTE_WRITEABLE | PTE_USER);
    get_current_task()->user_stack     = stack;
    get_current_task()->user_stack_top = stack + BIG_USER_STACK;
    get_current_task()->main           = e_entry;
    get_current_task()->hasfscr        = false;
    get_current_task()->winnum         = 0;

    // if (!no_interrupt)
    // {
    enable_scheduler();
    open_interrupt;
    // }
    switch_to_user_mode();
    return (uint64_t)-(EAGAIN);
}

static void *file_copy(void *ptr)
{
    if (ptr == NULL) return NULL;
    fd_file_handle *src = (fd_file_handle *)ptr;
    return fd_dup(src);
}

uint64_t process_fork(struct X64_REGS *reg, bool is_vfork, uint64_t user_stack)
{
    while (true)
    {
        if (is_scheduler) break;
    }

    close_interrupt;
    disable_scheduler();

    pcb_t current_pcb = get_current_task()->parent_group;

    pcb_t new_pcb = (pcb_t)malloc(sizeof(struct process_control_block));
    memset(new_pcb, 0, sizeof(struct process_control_block));
    new_pcb->pid = now_pid++;
    strcpy(new_pcb->name, current_pcb->name);
    write_serial_fmt("fork name: %s\n", new_pcb->name);
    new_pcb->task_level = TASK_APPLICATION_LEVEL;
    new_pcb->status     = START;

    new_pcb->pagedir = is_vfork ? current_pcb->pagedir : clone_page_directory(current_pcb->pagedir, false);

    if (!vma_manager_clone(&current_pcb->vma_manager, &new_pcb->vma_manager))
    {
        write_serial_fmt("task: cannot clone process vma information.\n");
        free(new_pcb);
        return -ENOMEM;
    }
    new_pcb->cmdline      = strdup(current_pcb->cmdline);
    new_pcb->thread_queue = queue_init();
    new_pcb->parent_task  = current_pcb;
    new_pcb->queue_index  = queue_enqueue(pcb_group_queue, new_pcb);
    new_pcb->vfork        = is_vfork;
    new_pcb->child_pcb    = queue_init();
    new_pcb->child_index  = queue_enqueue(current_pcb->child_pcb, new_pcb);
    new_pcb->virt_queue   = queue_copy(current_pcb->virt_queue, virt_copy);
    new_pcb->file_open    = queue_copy(current_pcb->file_open, file_copy);
    new_pcb->ipc_queue    = queue_init();

    new_pcb->xtttp_stc = (xtttp_dtt *)malloc(sizeof(xtttp_dtt));
    memset(new_pcb->xtttp_stc, 0, sizeof(xtttp_dtt));

    tcb_t parent_task = get_current_task();

    tcb_t new_task = (tcb_t)aligned_alloc(KERNEL_STACK_SIZE, KERNEL_STACK_SIZE);
    if (new_task == NULL) return (uint64_t)-(ENOMEM);
    memset(new_task, 0, sizeof(struct thread_control_block));
    new_task->task_level     = TASK_APPLICATION_LEVEL;
    new_task->cpu_id         = get_current_cpu()->processor_id;
    new_task->status         = START;
    new_task->user_stack     = parent_task->user_stack;
    new_task->user_stack_top = parent_task->user_stack_top;
    new_task->kernel_stack   = ((uint64_t)new_task) + KERNEL_STACK_SIZE;
    new_task->main           = parent_task->main;
    new_task->cwd            = parent_task->cwd;
    new_task->user_info      = parent_task->user_info;
    strcpy(new_task->name, parent_task->name);
    new_task->winnum  = 0;
    new_task->hasfscr = false;

    new_task->context0.rip    = reg->rcx; // syscall 指令中 rcx 寄存器为 rip
    new_task->context0.rflags = reg->r11; // syscall 指令中 r11 寄存器为 rflags
    new_task->context0.cs     = reg->cs;
    new_task->context0.ss     = reg->ss;
    new_task->context0.es     = reg->es;
    new_task->context0.ds     = reg->ds;
    new_task->context0.rax    = 0; // 子线程返回 0
    new_task->context0.rdi    = reg->rdi;
    new_task->context0.rsi    = reg->rsi;
    new_task->context0.rdx    = reg->rdx;
    new_task->context0.r9     = reg->r9;
    new_task->context0.r8     = reg->r8;
    new_task->context0.r10    = reg->r10;
    new_task->context0.r11    = reg->r11;
    new_task->context0.r12    = reg->r12;
    new_task->context0.r13    = reg->r13;
    new_task->context0.r14    = reg->r14;
    new_task->context0.r15    = reg->r15;
    new_task->context0.rbx    = reg->rbx;
    new_task->context0.rbp    = reg->rbp;
    new_task->context0.rsp    = user_stack == 0 ? reg->rsp : user_stack;
    new_task->context0.rcx    = reg->rcx;

    memcpy(new_task->fpu_context.fxsave_area, parent_task->fpu_context.fxsave_area, 512);

    new_task->fs      = parent_task->fs;
    new_task->fs_base = parent_task->fs_base;

    new_task->parent_group = new_pcb;
    new_task->group_index  = queue_enqueue(new_pcb->thread_queue, new_task);
    new_task->tid          = now_tid++;

    new_task->window_count = 0;

    add_task(new_task);
    if (!no_interrupt)
    {
        enable_scheduler();
        open_interrupt;
    }
    procfs_on_new_task(new_pcb);

    write_serial_fmt("PID: %d\n", new_pcb->pid);
    if (!is_vfork) return new_pcb->pid;

    while (true)
    {
        ipc_message_t msg = ipc_recv_wait2(IPC_MSG_TYPE_EXEC, IPC_MSG_TYPE_EPID);
        if (msg->pid == new_pcb->pid)
        {
            free(msg);
            return new_pcb->pid;
        }
        ipc_send(current_pcb->parent_task, msg);
    }

    int npid = new_pcb->pid;
    do
    {
        ipc_message_t msg = ipc_recv_wait(IPC_MSG_TYPE_EXEC);
        if (npid == msg->pid)
        {
            free(msg);
            return npid;
        }
        ipc_send(current_pcb->parent_task, msg);
    } while (true);
}

spin_t create_thread_lock = SPIN_INIT;

size_t create_kernel_thread(void *_start, void *args, char *name, pcb_t pcb)
{
    spin_lock(create_thread_lock);
    close_interrupt;
    disable_scheduler();
    tcb_t new_task = (tcb_t)aligned_alloc(KERNEL_STACK_SIZE, KERNEL_STACK_SIZE);

    memset(new_task, 0, KERNEL_STACK_SIZE);

    if (pcb == NULL)
    {
        new_task->group_index  = queue_enqueue(kernel_group->thread_queue, new_task);
        new_task->tid          = now_tid++;
        new_task->parent_group = kernel_group;
    }
    else
    {
        queue_enqueue(pcb->thread_queue, new_task);
        new_task->tid          = now_tid++;
        new_task->parent_group = pcb;
    }

    new_task->task_level = TASK_KERNEL_LEVEL;
    new_task->cpu_id     = get_current_cpu()->processor_id;
    memcpy(new_task->name, name, strlen(name) + 1);

    ptr_cast_t cast;
    cast.ptr                   = new_task;
    cast.val                  += KERNEL_STACK_SIZE;
    uint64_t *stack_top        = (uint64_t *)(cast.ptr);
    new_task->context0.rflags  = 0x202;
    new_task->context0.rip     = (uint64_t)_start;
    new_task->context0.rsp     = (uint64_t)stack_top & ~(uint64_t)0xF; // 设置上下文
    new_task->kernel_stack     = (new_task->context0.rsp -= 8);        // 栈16字节对齐
    new_task->context0.cs      = 0x8;
    new_task->context0.ss      = 0x10;
    new_task->context0.es      = 0x10;
    new_task->context0.ds      = 0x10;
    new_task->status           = CREATE;
    new_task->main             = (uint64_t)_start;

    new_task->winnum  = 0;
    new_task->hasfscr = false;

    new_task->user_info            = current_user;
    new_task->user_info->user_type = XUT_Root;

    stack_top = (uint64_t *)new_task->kernel_stack;
    PUSH_STACK(stack_top, 0);
    PUSH_STACK(stack_top, 0);
    PUSH_STACK(stack_top, (uint64_t)process_exit);
    new_task->context0.rsp = (uint64_t)stack_top;
    new_task->kernel_stack = (new_task->context0.rsp);
    new_task->context0.rdi = (uint64_t)args; // first argument in rdi

    new_task->fs_base = (uint64_t)new_task;

    new_task->str_cwd = "/";
    new_task->cwd     = rootdir;
    new_task->cwd->refcount++;
    new_task->window_count = 0;
    add_task(new_task);
    if (!no_interrupt)
    {
        enable_scheduler();
        open_interrupt;
    }
    spin_unlock(create_thread_lock);
    return new_task->tid;
}

size_t create_user_thread(void *_start, void *args, int argc, char *name, pcb_t pcb, char *cwd)
{
    spin_lock(create_thread_lock);
    close_interrupt;
    disable_scheduler();
    if (!cwd)
    {
        write_serial_fmt("You can't create an user thread WITHOUT A CWD \n");
        return (size_t)-(EINVAL);
    }
    tcb_t new_task = (tcb_t)aligned_alloc(KERNEL_STACK_SIZE, KERNEL_STACK_SIZE);

    memset(new_task, 0, KERNEL_STACK_SIZE);

    if (pcb == NULL)
    {
        new_task->group_index  = queue_enqueue(kernel_group->thread_queue, new_task);
        new_task->tid          = now_tid++;
        new_task->parent_group = kernel_group;
    }
    else
    {
        queue_enqueue(pcb->thread_queue, new_task);
        new_task->tid          = now_tid++;
        new_task->parent_group = pcb;
    }

    new_task->task_level = TASK_APPLICATION_LEVEL;
    new_task->cpu_id     = get_current_cpu()->processor_id;
    memcpy(new_task->name, name, strlen(name) + 1);

    uint64_t *stack_top       = (uint64_t *)((uint64_t)new_task + KERNEL_STACK_SIZE);
    *(--stack_top)            = 0;
    *(--stack_top)            = 0;
    *(--stack_top)            = (uint64_t)switch_to_user_mode;
    new_task->context0.rflags = 0x202;
    new_task->context0.rip    = (uint64_t)switch_to_user_mode;
    new_task->context0.rsp    = (uint64_t)stack_top;
    new_task->kernel_stack    = (uint64_t)new_task + KERNEL_STACK_SIZE;
    new_task->syscall_stack   = (uint64_t)aligned_alloc(KERNEL_STACK_SIZE, KERNEL_STACK_SIZE) + KERNEL_STACK_SIZE;

    new_task->cwd = vfs_open(cwd);
    new_task->cwd->refcount++;
    write_serial_fmt("CWD %s\n", new_task->cwd->name);
    new_task->str_cwd = cwd;

    new_task->winnum  = 0;
    new_task->hasfscr = false;

    new_task->argv = (char **)args;
    new_task->argc = argc;

    new_task->user_info = current_user;

    new_task->user_stack     = page_alloc_random(pcb->pagedir, BIG_USER_STACK, PTE_PRESENT | PTE_WRITEABLE | PTE_USER);
    new_task->user_stack_top = new_task->user_stack + BIG_USER_STACK;
    new_task->main           = (uint64_t)_start;
    new_task->context0.cs    = 0x8;
    new_task->context0.ss = new_task->context0.es = new_task->context0.ds = 0x10;
    new_task->status                                                      = CREATE;

    new_task->fs_base = (uint64_t)new_task;

    new_task->window_count = 0;

    add_task(new_task);
    if (!no_interrupt)
    {
        enable_scheduler();
        open_interrupt;
    }
    spin_unlock(create_thread_lock);
    return new_task->tid;
}

size_t create_signal_thread(void *_start, uint64_t msg_type, uint64_t data1, uint64_t data2, uint64_t revert_func,
                            pcb_t pcb)
{
    spin_lock(create_thread_lock);
    close_interrupt;
    disable_scheduler();
    tcb_t new_task = (tcb_t)aligned_alloc(KERNEL_STACK_SIZE, KERNEL_STACK_SIZE);

    memset(new_task, 0, KERNEL_STACK_SIZE);

    if (pcb == NULL)
    {
        new_task->group_index  = queue_enqueue(kernel_group->thread_queue, new_task);
        new_task->tid          = now_tid++;
        new_task->parent_group = kernel_group;
    }
    else
    {
        queue_enqueue(pcb->thread_queue, new_task);
        new_task->tid          = now_tid++;
        new_task->parent_group = pcb;
    }

    new_task->task_level = TASK_APPLICATION_LEVEL;
    new_task->cpu_id     = get_current_cpu()->processor_id;
    memcpy(new_task->name, "Window Message Thread", strlen("Window Message Thread") + 1);

    ptr_cast_t cast;
    cast.ptr                   = new_task;
    cast.val                  += KERNEL_STACK_SIZE;
    uint64_t *stack_top        = (uint64_t *)(cast.ptr);
    new_task->context0.rflags  = 0x202;
    new_task->context0.rip     = (uint64_t)jump_to_signal;
    new_task->context0.rsp     = (uint64_t)stack_top;                        // 设置上下文
    new_task->kernel_stack     = (new_task->context0.rsp &= ~(uint64_t)0xF); // 栈16字节对齐
    new_task->syscall_stack    = (uint64_t)aligned_alloc(KERNEL_STACK_SIZE, KERNEL_STACK_SIZE) + KERNEL_STACK_SIZE;
    new_task->context0.cs      = 0x8;
    new_task->context0.ss      = 0x10;
    new_task->context0.es      = 0x10;
    new_task->context0.ds      = 0x10;
    new_task->status           = CREATE;
    new_task->main             = (uint64_t)_start;

    new_task->context0.rdi = msg_type;
    new_task->context0.rsi = data1;
    new_task->context0.rdx = data2;
    new_task->context0.rcx = revert_func;

    stack_top = (uint64_t *)new_task->kernel_stack;
    PUSH_STACK(stack_top, (uint64_t)_start);
    PUSH_STACK(stack_top, (uint64_t)jump_to_signal);
    new_task->context0.rsp = (uint64_t)stack_top;
    new_task->kernel_stack = (new_task->context0.rsp);

    new_task->user_stack     = page_alloc_random(pcb->pagedir, BIG_USER_STACK, PTE_PRESENT | PTE_WRITEABLE | PTE_USER);
    new_task->user_stack_top = new_task->user_stack + BIG_USER_STACK;

    new_task->fs_base = (uint64_t)new_task;
    add_task(new_task);
    if (!no_interrupt)
    {
        enable_scheduler();
        open_interrupt;
    }
    spin_unlock(create_thread_lock);
    return new_task->tid;
}

pcb_t create_process_group(const char *name, pcb_t parent, page_directory_t *directory, char *cmdline)
{
    pcb_t pcb        = (pcb_t)calloc(1, sizeof(struct process_control_block));
    pcb->pid         = now_pid++;
    pcb->parent_task = parent == NULL ? kernel_group : parent;
    pcb->status      = START;
    pcb->queue_index = queue_enqueue(pcb_group_queue, pcb);
    strcpy(pcb->name, name);
    pcb->thread_queue = queue_init();
    pcb->ppid         = 0;
    pcb->pagedir      = directory ? directory : get_kernel_pagedir();
    pcb->child_pcb    = queue_init();
    pcb->vfork        = false;
    pcb->file_open    = queue_init();
    pcb->virt_queue   = queue_init();
    pcb->ipc_queue    = queue_init();

    if (parent == NULL)
    {
        pcb->envc = current_user->envc;
        pcb->envp = (char **)malloc(pcb->envc * sizeof(char *));
        pcb->envp = copy_envp(current_user->envp);
    }
    else
    {
        pcb->envc = parent->envp == NULL ? current_user->envc : parent->envc;
        pcb->envp = (char **)malloc(pcb->envc * sizeof(char *));
        pcb->envp = copy_envp(parent->envp == NULL ? current_user->envp : parent->envp);
    }

    pcb->file_open = queue_init();

    pcb->cmdline     = (char *)malloc(strlen(cmdline));
    pcb->child_index = queue_enqueue(pcb->parent_task->child_pcb, pcb);

    // pcb->msgprci = false;

    pcb->xtttp_stc = (xtttp_dtt *)malloc(sizeof(xtttp_dtt));
    memset(pcb->xtttp_stc, 0, sizeof(xtttp_dtt));
    procfs_on_new_task(pcb);

    return pcb;
}

extern "C" uint64_t switch_to_kernel_stack()
{
    return get_current_task()->syscall_stack;
}

bool       smp_scheduler_lock = true;
extern int scheduler_is_ready;

void process_setup()
{
    pcb_group_queue     = queue_init();
    pcb_t kernel_pcb    = (pcb_t)calloc(1, sizeof(struct process_control_block));
    kernel_pcb->pid     = now_pid++;
    kernel_pcb->pagedir = get_kernel_pagedir();
    strcpy(kernel_pcb->name, "XSK System");
    kernel_pcb->parent_task  = kernel_pcb;
    kernel_pcb->status       = RUNNING;
    kernel_pcb->queue_index  = queue_enqueue(pcb_group_queue, kernel_pcb);
    kernel_pcb->thread_queue = queue_init();
    kernel_pcb->child_pcb    = queue_init();
    kernel_pcb->xtttp_stc    = (xtttp_dtt *)malloc(sizeof(xtttp_dtt));
    memset(kernel_pcb->xtttp_stc, 0, sizeof(xtttp_dtt));
    kernel_pcb->file_open    = queue_init();
    kernel_group->virt_queue = queue_init();
    kernel_group->ipc_queue  = queue_init();

    kernel_pcb->child_index = 0;

    kernel_group = kernel_pcb;

    smp_scheduler_lock = false;
    scheduler_is_ready++;

    write_serial_fmt("Setup process <%s> PID:%zu\n", kernel_pcb->name, kernel_pcb->pid);
}