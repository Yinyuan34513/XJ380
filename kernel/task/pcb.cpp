#include "task/pcb.h"
#include "cpu/regio.h"
#include "krlibc.h"
#include "mm/heap.h"
#include "mm/page.h"
#include "proto.hpp"
#include <errno.h>
#include <fs/vfs/vfs.h>
#include <cpu/longm.h>

#define SA_RPL3 3

#define SA_RPL_MASK      0xFFFC
#define SA_TI_MASK       0xFFFB
#define GET_SEL(cs, rpl) ((cs & SA_RPL_MASK & SA_TI_MASK) | (rpl))

#define PUSH_STACK(stack, value) *(--stack) = value
static size_t now_pid         = 0;
size_t        now_tid         = 0;
lock_queue   *pcb_group_queue = NULL;
pcb_t         kernel_group    = NULL;

extern bool no_interrupt;

[[noreturn]] void process_exit()
{
    uint64_t rax = 0;
    __asm__("movq %%rax,%0" ::"r"(rax) :);
    write_serial_string("Kernel thread exit, Code: ");
    write_serial_dec(rax);
    write_serial_string("\n");
    //    kill_thread(get_current_task());
    while (true)
        __asm__ volatile("hlt");
}

void switch_to_user_mode()
{
    close_interrupt;
    uint64_t rsp                        = (uint64_t)get_current_task()->user_stack_top;
    get_current_task()->context0.rflags = 0 << 12 | 0b10 | 1 << 9;
    uint64_t func                       = get_current_task()->main;

    __asm__ volatile("mov %0, %%es\n"
                     "mov %0, %%ds\n"
                     "pushq %5\n" // SS
                     "pushq %1\n" // RSP
                     "pushq %2\n" // RFLAGS
                     "pushq %3\n" // CS
                     "pushq %4\n" // RIP
                     "iretq\n"
                     :
                     : "r"((uint64_t)GET_SEL(4 * 8, SA_RPL3)), "r"(rsp), "r"(get_current_task()->context0.rflags),
                       "r"((uint64_t)0x23), "r"(func), "r"((uint64_t)0x1b)
                     : "memory");
}

char *normalize_path(const char *path)
{
    if (!path) return NULL;

    size_t len    = strlen(path);
    char  *result = (char *)malloc(len + 1);
    if (!result) return NULL;

    char *dup = strdup(path);
    if (!dup)
    {
        free(result);
        return NULL;
    }

    strcpy(result, "/");
    if (strcmp(path, "/") == 0)
    {
        free(dup);
        return result;
    }

    char *start = dup;
    if (*start == '/') start++;

    char *token = strtok(start, "/");
    while (token)
    {
        if (strcmp(token, ".") == 0) {}
        else if (strcmp(token, "..") == 0)
        {
            char *last_slash = strrchr(result, '/');
            if (last_slash != result)
                *last_slash = '\0';
            else
                result[1] = '\0';
        }
        else
        {
            if (result[strlen(result) - 1] != '/') strcat(result, "/");
            strcat(result, token);
        }

        token = strtok(NULL, "/");
    }

    free(dup);
    return result;
}

char *vfs_cwd_path_build(char *src)
{
    char *s = src;
    char *path;
    if (s[0] == '/') { path = strdup(s); }
    else { path = pathacat(get_current_task()->parent_group->cwd, s); }
    char *normalized_path = normalize_path(path);
    free(path);
    return normalized_path;
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
        if (is_scheduler)
            break;
    }

    write_serial_fmt("path in execve %s\n", path);
    char      *norm_path = vfs_cwd_path_build(path);
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

    strncpy(process->name, norm_path, 50);

    if (envp != NULL)
    {
        char **old_envp = process->envp;
        size_t old_envc = process->envc;
        process->envp   = copy_envp(envp);
        process->envc   = envp_length(envp);
    }

    write_serial_fmt(" process name :%s \n", process->name);

    page_directory_t *old_page_dir = process->pagedir;
    switch_process_page_directory(clone_page_directory(get_current_directory(), false));
    process->pagedir = clone_page_directory(get_current_directory(), false);

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

    vfs_close(node);

    for (uint64_t i = 3; i < 256; i++)
    {
        if (!get_current_task()->fds[i]) continue;

        if (get_current_task()->fds[i]->flags & O_CLOEXEC)
        {
            vfs_close(get_current_task()->fds[i]);
            free(get_current_task()->fds[i]);
            get_current_task()->fds[i] = NULL;
        }
    }

    free(norm_path);

    uint64_t stack = page_alloc_random(get_current_directory(), BIG_USER_STACK, PTE_PRESENT | PTE_WRITEABLE | PTE_USER);
    get_current_task()->user_stack     = stack;
    get_current_task()->user_stack_top = stack + BIG_USER_STACK;
    get_current_task()->main           = e_entry;

    if (!no_interrupt) 
    {
        enable_scheduler();
        open_interrupt;
    }
    switch_to_user_mode();
    return (uint64_t)-(EAGAIN);
}

uint64_t process_fork(struct X64_REGS *reg, bool is_vfork, uint64_t user_stack)
{
    while (true)
    {
        if (is_scheduler)
            break;
    }
    
    close_interrupt;
    disable_scheduler();

    pcb_t current_pcb = get_current_task()->parent_group;

    pcb_t new_pcb = (pcb_t)malloc(sizeof(struct process_control_block));
    memset(new_pcb, 0, sizeof(struct process_control_block));
    new_pcb->pid = now_pid++;
    strcpy(new_pcb->name, current_pcb->name);
    write_serial_fmt("fork name: %s\n",new_pcb->name);
    new_pcb->task_level = TASK_APPLICATION_LEVEL;
    new_pcb->status     = START;

    new_pcb->pagedir = is_vfork ? current_pcb->pagedir : clone_page_directory(current_pcb->pagedir, false);

    new_pcb->cmdline      = strdup(current_pcb->cmdline);
    new_pcb->thread_queue = queue_init();
    new_pcb->cwd          = (char *)malloc(strlen(current_pcb->cwd));
    strcpy(new_pcb->cwd, current_pcb->cwd);
    new_pcb->parent_task = current_pcb;
    new_pcb->queue_index = queue_enqueue(pcb_group_queue, new_pcb);
    new_pcb->vfork       = is_vfork;
    new_pcb->child_pcb   = queue_init();
    new_pcb->child_index = queue_enqueue(current_pcb->child_pcb, new_pcb);

    tcb_t parent_task = get_current_task();

    tcb_t new_task = (tcb_t)malloc(KERNEL_STACK_SIZE);
    if (new_task == NULL) return (uint64_t)-(ENOMEM);
    memset(new_task, 0, sizeof(struct thread_control_block));
    new_task->task_level     = TASK_APPLICATION_LEVEL;
    new_task->cpu_id         = get_current_cpu()->processor_id;
    new_task->status         = START;
    new_task->user_stack     = parent_task->user_stack;
    new_task->user_stack_top = parent_task->user_stack_top;
    new_task->kernel_stack   = ((uint64_t)new_task) + KERNEL_STACK_SIZE;
    new_task->main           = parent_task->main;
    strcpy(new_task->name, parent_task->name);

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

    add_task(new_task);
    if (!no_interrupt) 
    {
        enable_scheduler();
        open_interrupt;
    }

    if (!is_vfork) return new_pcb->pid;

    /*
    if (!is_vfork) return new_pcb->pid;
    int npid = new_pcb->pid;
    do {
        ipc_message_t msg = ipc_recv_wait(IPC_MSG_TYPE_EXEC);
        if (npid == msg->pid) {
            free(msg);
            return npid;
        }
        ipc_send(current_pcb->parent_task, msg);
    } while (true);
     */

    /*
    if (is_vfork) {
        int npid = new_pcb->pid;
        do {
            ipc_message_t msg = ipc_recv_wait(IPC_MSG_TYPE_EXEC);
            if (npid == msg->pid) {
                free(msg);
                break;
            }
            ipc_send(current_pcb->parent_task, msg);
        } while (true);
    }

    return new_pcb->pid;
     */
    // return 0;
}

size_t create_kernel_thread(void *_start, void *args, char *name, pcb_t pcb)
{
    close_interrupt;
    disable_scheduler();
    tcb_t new_task = (tcb_t)phys_to_virt(alloc_frames(KERNEL_STACK_SIZE / PAGE_SIZE));

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
    cast.ptr             = new_task;
    cast.val            += KERNEL_STACK_SIZE;
    uint64_t *stack_top  = (uint64_t *)(cast.ptr);
    new_task->context0.rflags = 0x202;
    new_task->context0.rip    = (uint64_t)_start;
    new_task->context0.rsp    = (uint64_t)stack_top & ~(uint64_t)0xF; // 设置上下文
    new_task->kernel_stack    = (new_task->context0.rsp -= 8);        // 栈16字节对齐
    new_task->context0.cs     = 0x8;
    new_task->context0.ss     = 0x10;
    new_task->context0.es     = 0x10;
    new_task->context0.ds     = 0x10;
    new_task->status          = CREATE;
    new_task->main            = (uint64_t)_start;

    stack_top = (uint64_t *)new_task->kernel_stack;
    PUSH_STACK(stack_top, (uint64_t)args);
    PUSH_STACK(stack_top, (uint64_t)process_exit);
    new_task->context0.rsp    = (uint64_t)stack_top;
    new_task->kernel_stack    = (new_task->context0.rsp);
    new_task->signal_handler_stack = (uint64_t)phys_to_virt(
        alloc_frames(KERNEL_STACK_SIZE)
    );
    new_task->in_signal_handler = false;
    memset(&new_task->signal_frame, 0, sizeof(SignalFrame));

    new_task->fs_base = (uint64_t)new_task;
    add_task(new_task);
    if (!no_interrupt)
    {
        enable_scheduler();
        open_interrupt;
    }
    return new_task->tid;
}

size_t create_user_thread(void *_start, void *args, char *name, pcb_t pcb)
{
    close_interrupt;
    disable_scheduler();
    tcb_t new_task = (tcb_t)phys_to_virt(alloc_frames(KERNEL_STACK_SIZE / PAGE_SIZE));

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

    ptr_cast_t cast;
    cast.ptr             = new_task;
    cast.val            += KERNEL_STACK_SIZE;
    uint64_t *stack_top  = (uint64_t *)(cast.ptr);
    new_task->context0.rflags = 0x202;
    new_task->context0.rip    = (uint64_t)switch_to_user_mode;
    new_task->context0.rsp    = (uint64_t)stack_top;                        // 设置上下文
    new_task->kernel_stack    = (new_task->context0.rsp &= ~(uint64_t)0xF); // 栈16字节对齐
    new_task->context0.cs     = 0x8;
    new_task->context0.ss     = 0x10;
    new_task->context0.es     = 0x10;
    new_task->context0.ds     = 0x10;
    new_task->status          = CREATE;
    new_task->main            = (uint64_t)_start;

    stack_top = (uint64_t *)new_task->kernel_stack;
    PUSH_STACK(stack_top, (uint64_t)_start);
    PUSH_STACK(stack_top, (uint64_t)switch_to_user_mode);
    new_task->context0.rsp    = (uint64_t)stack_top;
    new_task->kernel_stack    = (new_task->context0.rsp);

    new_task->user_stack     = page_alloc_random(pcb->pagedir, BIG_USER_STACK, PTE_PRESENT | PTE_WRITEABLE | PTE_USER);
    new_task->user_stack_top = new_task->user_stack + BIG_USER_STACK;
    new_task->signal_handler_stack = page_alloc_random(
        pcb->pagedir, 
        KERNEL_STACK_SIZE, 
        PTE_PRESENT | PTE_WRITEABLE | PTE_USER
    );
    new_task->in_signal_handler = false;
    memset(&new_task->signal_frame, 0, sizeof(SignalFrame));

    new_task->fs_base = (uint64_t)new_task;
    add_task(new_task);
    if (!no_interrupt)
    {
        enable_scheduler();
        open_interrupt;
    }
    return new_task->tid;
}

pcb_t create_process_group(const char *name, pcb_t parent, page_directory_t *directory,char *cmdline)
{
    pcb_t pcb        = (pcb_t)malloc(sizeof(struct process_control_block));
    pcb->pid         = now_pid++;
    pcb->parent_task = parent == NULL ? kernel_group : parent;
    pcb->status      = START;
    pcb->queue_index = queue_enqueue(pcb_group_queue, pcb);
    strcpy(pcb->name, name);
    pcb->thread_queue  = queue_init();
    pcb->ppid          = 0;
    pcb->pagedir       = directory ? directory : get_kernel_pagedir();
    pcb->cwd           = (char *)malloc(1024);
    pcb->child_pcb = queue_init();
    pcb->vfork     = false;
    pcb->envp      = NULL;
    pcb->envc      = 0;
    pcb->cmdline     = (char*)malloc(strlen(cmdline));
    memset(pcb->cwd, 0, 1024);
    strcpy(pcb->cwd, pcb->parent_task->cwd);
    pcb->child_index = queue_enqueue(pcb->parent_task->child_pcb, pcb);

    return pcb;
}

extern "C" uint64_t switch_to_kernel_stack()
{
    return get_current_task()->kernel_stack;
}

bool       smp_scheduler_lock = true;
extern int scheduler_is_ready;

void process_setup()
{
    pcb_group_queue     = queue_init();
    pcb_t kernel_pcb    = (pcb_t)malloc(sizeof(struct process_control_block));
    kernel_pcb->pid     = now_pid++;
    kernel_pcb->pagedir = get_kernel_pagedir();
    strcpy(kernel_pcb->name, "XSK System");
    kernel_pcb->parent_task  = kernel_pcb;
    kernel_pcb->status       = RUNNING;
    kernel_pcb->queue_index  = queue_enqueue(pcb_group_queue, kernel_pcb);
    kernel_pcb->thread_queue = queue_init();
    kernel_pcb->cwd          = (char *)malloc(1024);
    memset(kernel_pcb->cwd, 0, 1024);
    kernel_pcb->cwd[0]      = '/';
    kernel_pcb->child_pcb   = queue_init();
    kernel_pcb->child_index = 0;
    
    kernel_group             = kernel_pcb;

    tcb_t idle_thread         = (tcb_t)phys_to_virt(alloc_frames(KERNEL_STACK_SIZE / PAGE_SIZE));
    idle_thread->tid          = now_tid++;
    idle_thread->parent_group = kernel_pcb;
    idle_thread->kernel_stack = get_rsp();
    idle_thread->status       = RUNNING;
    strcpy(idle_thread->name, "idle thread");
    idle_thread->group_index  = queue_enqueue(kernel_pcb->thread_queue, idle_thread);
    idle_thread->context0.rsp = get_rsp();
    set_kernel_stack(get_rsp());
    idle_thread->queue_index = queue_enqueue(get_current_cpu()->scheduler_queue, idle_thread);

    get_current_cpu()->current_task = idle_thread;

    smp_scheduler_lock = false;
    scheduler_is_ready++;

    write_serial_fmt("Setup process <%s> PID:%zu\n", kernel_pcb->name, kernel_pcb->pid);
}
