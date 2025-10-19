#include "task/pcb.h"
#include "cpu/regio.h"
#include "krlibc.h"
#include "mm/heap.h"
#include "mm/page.h"
#include "proto.hpp"
#include <cpu/longm.h>
#include <errno.h>
#include <fs/vfs/vfs.h>

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

void thread_exit(int exit_code)
{
    close_interrupt;
    disable_scheduler();
    spin_lock(scheduler_lock);  // 添加锁

    tcb_t current = get_current_task();
    pcb_t group   = current->parent_group;

    // 设置线程状态为死亡
    current->status = DEATH;

    // 从调度队列中移除
    struct PROCESSOR_INFO *cpu = get_current_cpu();
    queue_remove_at(cpu->scheduler_queue, current->queue_index);

    // 从进程线程队列中移除
    queue_remove_at(group->thread_queue, current->group_index);

    // 如果这是进程的最后一个线程，标记进程为僵尸状态
    if (group->thread_queue->size == 0)
    {
        group->status = DEATH;
    }

    // 如果进程没有线程了，安排回收
    if (group->thread_queue->size == 0 && group->status == DEATH) { 
        addque_killer(group); 
    }
    
    switch_page_directory(get_current_directory());
    spin_unlock(scheduler_lock);  // 释放锁

    if (!no_interrupt)
    {
        enable_scheduler();
        open_interrupt;
    }
}
pcb_t found_pcb(int pid) {
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

void kill_thread(int tid, pcb_t parent_group)
{
    bool interrupt_yes = false;
    if (!no_interrupt)
        interrupt_yes = true;
    close_interrupt;
    disable_scheduler();
    no_interrupt = true;

    tcb_t current = found_thread(parent_group, tid);
    if (current == NULL) {
        write_serial_fmt("[Warning] kill_thread: Cannot found target.\n");
        return;
    }

    pcb_t group   = current->parent_group;
    // write_serial_fmt("kill thread %s\n", current->name);

    // 设置线程状态为死亡
    current->status = DEATH;

    // 从调度队列中移除
    PROCESSOR_INFO *cpu = get_current_cpu();
    queue_remove_at(cpu->scheduler_queue, current->queue_index);

    // 从进程线程队列中移除
    queue_remove_at(group->thread_queue, current->group_index);

    // 如果这是进程的最后一个线程，标记进程为僵尸状态
    if (group->thread_queue->size == 0)
    {
        group->status = DEATH;
        // // 如果是vfork创建的进程，需要特殊处理
        // if (group->vfork) {
        //     // 添加vfork的特殊处理逻辑
        // }
    }

    // 如果进程没有线程了，安排回收
    if (group->thread_queue->size == 0 && group->status == DEATH) { addque_killer(group); }

    if (interrupt_yes)
        no_interrupt = false;
    if (!no_interrupt)
    {
        enable_scheduler();
        open_interrupt;
    }
    switch_page_directory(get_current_directory());

}

void kill_thread(int tid)
{
    kill_thread(tid, kernel_group);
}

void recycle_process(pcb_t pcb) {
    bool interrupt_yes = false;
    if (!no_interrupt)
        interrupt_yes = true;
    no_interrupt = true;

    // 不要回收内核进程
    if (pcb == kernel_group) return;

    // 释放进程资源
    if (pcb->pagedir != get_kernel_pagedir()) { free_page_directory(pcb->pagedir); }

    if (pcb->cmdline) free(pcb->cmdline);

    // 释放环境变量
    if (pcb->envp)
    {
        for (size_t i = 0; i < pcb->envc; i++)
        {
            free(pcb->envp[i]);
        }
        free(pcb->envp);
    }

    // 从进程队列中移除
    queue_remove_at(pcb_group_queue, pcb->queue_index);

    // 从父进程的子进程队列中移除
    if (pcb->parent_task && pcb->parent_task->child_pcb)
    {
        queue_remove_at(pcb->parent_task->child_pcb, pcb->child_index);
    }

    // 释放进程本身
    free(pcb);

    if (interrupt_yes)
        no_interrupt = false;
    if (!no_interrupt)
    {   
        enable_scheduler();
        open_interrupt;
    }
}

[[noreturn]] void process_exit()
{
    uint64_t rax = 0;
    __asm__("movq %%rax,%0" ::"r"(rax) :);
    write_serial_string("Kernel thread exit, Code: ");
    write_serial_dec(rax);
    write_serial_string("\n");
    thread_exit(rax);
    //    kill_thread(get_current_task()->tid);
    while (true)
        __asm__ volatile("hlt");
}

// 在用户栈上构建 argc, argv, envp 参数块
uint64_t build_user_stack_args(tcb_t task)
{
    uint64_t stack_top = task->user_stack_top;
    char *stack_ptr = (char *)stack_top;
    
    int argc = task->argc;
    char **argv = task->argv;
    char **envp = task->parent_group->envp;
    
    // 计算环境变量数量
    int envc = 0;
    while (envp && envp[envc]) envc++;
    
    // 调试信息
    write_serial_fmt("Building user stack: argc=%d, envc=%d\n", argc, envc);
    
    // ========== 从栈顶向下构建 ==========
    
    // 1. 首先放置所有字符串数据（从栈顶开始）
    
    // 环境变量字符串
    size_t total_string_size = 0;
    for (int i = 0; i < envc; i++) {
        if (envp[i]) {
            size_t len = strlen(envp[i]) + 1;
            stack_ptr -= len;
            memcpy(stack_ptr, envp[i], len);
            total_string_size += len;
        }
    }
    
    // 参数字符串
    for (int i = 0; i < argc; i++) {
        if (argv && argv[i]) {
            size_t len = strlen(argv[i]) + 1;
            stack_ptr -= len;
            memcpy(stack_ptr, argv[i], len);
            total_string_size += len;
        }
    }
    
    // 2. 16字节对齐
    stack_ptr = (char *)((uint64_t)stack_ptr & ~0xF);
    
    // 3. 放置 envp 指针数组（NULL结尾）
    uint64_t *envp_pointers_start = (uint64_t *)stack_ptr;
    stack_ptr -= (envc + 1) * sizeof(uint64_t);
    uint64_t *envp_pointers = (uint64_t *)stack_ptr;
    
    // 重新计算envp字符串地址并填充指针数组
    char *env_string_walker = (char *)envp_pointers_start;
    for (int i = 0; i < envc; i++) {
        if (envp[i]) {
            size_t len = strlen(envp[i]) + 1;
            env_string_walker -= len;
            envp_pointers[i] = (uint64_t)env_string_walker;
        } else {
            envp_pointers[i] = 0;
        }
    }
    envp_pointers[envc] = 0; // NULL结尾
    
    // 4. 放置 argv 指针数组（NULL结尾）
    stack_ptr -= (argc + 1) * sizeof(uint64_t);
    uint64_t *argv_pointers = (uint64_t *)stack_ptr;
    
    // 重新计算argv字符串地址并填充指针数组
    char *arg_string_walker = (char *)envp_pointers_start;
    for (int i = 0; i < argc; i++) {
        if (argv && argv[i]) {
            size_t len = strlen(argv[i]) + 1;
            arg_string_walker -= len;
            argv_pointers[i] = (uint64_t)arg_string_walker;
        } else {
            argv_pointers[i] = 0;
        }
    }
    argv_pointers[argc] = 0; // NULL结尾
    
    // 5. 放置 argc
    stack_ptr -= sizeof(uint64_t);
    *(uint64_t *)stack_ptr = (uint64_t)argc;
    
    // 6. 设置寄存器System V AMD64 ABI    hhhhh
    task->context0.rdi = argc;                          // argc
    task->context0.rsi = (uint64_t)argv_pointers;       // argv 
    task->context0.rdx = (uint64_t)envp_pointers;       // envp
    
    write_serial_fmt("User stack built: rsp=0x%llx\n", (uint64_t)stack_ptr);
    write_serial_fmt("  argc=%d at 0x%llx\n", argc, (uint64_t)stack_ptr);
    write_serial_fmt("  argv at 0x%llx\n", (uint64_t)argv_pointers);
    write_serial_fmt("  envp at 0x%llx\n", (uint64_t)envp_pointers);
    
    return (uint64_t)stack_ptr;
}


void switch_to_user_mode()
{
    close_interrupt;
    
    tcb_t current = get_current_task();
    
    // 在用户栈上构建参数块
    uint64_t user_rsp = build_user_stack_args(current);
    
    current->context0.rflags = 0 << 12 | 0b10 | 1 << 9;
    uint64_t func = current->main;

    // 现在不需要通过寄存器传递参数了，参数已经在栈上
    __asm__ volatile("mov %0, %%es\n"
                     "mov %0, %%ds\n"
                     "pushq %5\n" // SS
                     "pushq %1\n" // RSP (已经包含参数的栈指针)
                     "pushq %2\n" // RFLAGS
                     "pushq %3\n" // CS
                     "pushq %4\n" // RIP
                     "mov %6, %%rdi\n"  // arg1
                     "mov %7, %%rsi\n"  // arg2
                     "mov %8, %%rdx\n"  // arg3
                     "iretq\n"
                     :
                     : "r"((uint64_t)GET_SEL(4 * 8, SA_RPL3)), 
                       "r"(user_rsp), 
                       "r"(current->context0.rflags),
                       "r"((uint64_t)0x23), 
                       "r"(func), 
                       "r"((uint64_t)0x1b),
                       "r"(current->context0.rdi), "r"(current->context0.rsi), "r"(current->context0.rdx)
                     : "memory");
}


void jump_to_signal(uint64_t msg_type, uint64_t data1, uint64_t data2, uint64_t rf)
{
    close_interrupt;
    uint64_t *rsp                        = (uint64_t *)get_current_task()->user_stack_top;
    PUSH_STACK(rsp, 0);     // 对齐
    PUSH_STACK(rsp, rf);    // 返回程序
    get_current_task()->context0.rflags = 0 << 12 | 0b10 | 1 << 9;
    uint64_t func                       = get_current_task()->main;

    __asm__ volatile("mov %0, %%es\n"
                     "mov %0, %%ds\n"
                     "pushq %5\n" // SS
                     "pushq %1\n" // RSP
                     "pushq %2\n" // RFLAGS
                     "pushq %3\n" // CS
                     "pushq %4\n" // RIP
                     "mov %6, %%rdi\n"  // arg1
                     "mov %7, %%rsi\n"  // arg2
                     "mov %8, %%rdx\n"  // arg3
                     "iretq\n"
                     :
                     : "r"((uint64_t)GET_SEL(4 * 8, SA_RPL3)), "r"((uint64_t)rsp), "r"(get_current_task()->context0.rflags),
                       "r"((uint64_t)0x23), "r"(func), "r"((uint64_t)0x1b), 
                       "r"(msg_type), "r"(data1), "r"(data2)
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
    else { path = pathacat(get_current_task()->cwd->name, s); }
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
        if (is_scheduler) break;
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

    get_thread_name_from_filepath(norm_path, process->name);

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
        if (!get_current_task()->parent_group->fds[i]) continue;

        vfs_close(get_current_task()->parent_group->fds[i]);
        free(get_current_task()->parent_group->fds[i]);
        get_current_task()->parent_group->fds[i] = NULL;
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
    return (uint64_t) - (EAGAIN);
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

    new_pcb->cmdline      = strdup(current_pcb->cmdline);
    new_pcb->thread_queue = queue_init();
    new_pcb->parent_task = current_pcb;
    new_pcb->queue_index = queue_enqueue(pcb_group_queue, new_pcb);
    new_pcb->vfork       = is_vfork;
    new_pcb->child_pcb   = queue_init();
    new_pcb->child_index = queue_enqueue(current_pcb->child_pcb, new_pcb);

    tcb_t parent_task = get_current_task();

    tcb_t new_task = (tcb_t)malloc(KERNEL_STACK_SIZE);
    if (new_task == NULL) return (uint64_t) - (ENOMEM);
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
    return 0;
}

spin_t create_thread_lock = SPIN_INIT;

size_t create_kernel_thread(void *_start, void *args, char *name, pcb_t pcb)
{
    spin_lock(create_thread_lock);
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

    new_task->user_info             = current_user;
    new_task->user_info->user_type  = XUT_Root;

    stack_top = (uint64_t *)new_task->kernel_stack;
    PUSH_STACK(stack_top, 0);
    PUSH_STACK(stack_top, 0);
    PUSH_STACK(stack_top, (uint64_t)process_exit);
    new_task->context0.rsp = (uint64_t)stack_top;
    new_task->kernel_stack = (new_task->context0.rsp);
    new_task->context0.rdi    = (uint64_t)args; // first argument in rdi

    new_task->fs_base = (uint64_t)new_task;

    new_task->str_cwd = "/";
    new_task->cwd     = rootdir;
    add_task(new_task);
    if (!no_interrupt)
    {
        enable_scheduler();
        open_interrupt;
    }
    spin_unlock(create_thread_lock);
    return new_task->tid;
}

size_t create_user_thread(void *_start, void *args, int argc, char *name, pcb_t pcb, char* cwd)
{
    spin_lock(create_thread_lock);
    close_interrupt;
    disable_scheduler();
    if(!cwd)
    {
        write_serial_fmt("You can't create an user thread WITHOUT A CWD \n");
        return (size_t)-(EINVAL);
    }
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

    if (args != NULL)
        new_task->argv = (char**)args;
    new_task->argc = 1;

    new_task->task_level = TASK_APPLICATION_LEVEL;
    new_task->cpu_id     = get_current_cpu()->processor_id;
    memcpy(new_task->name, name, strlen(name) + 1);

    ptr_cast_t cast;
    cast.ptr                   = new_task;
    cast.val                  += KERNEL_STACK_SIZE;
    uint64_t *stack_top        = (uint64_t *)(cast.ptr);
    new_task->context0.rflags  = 0x202;
    new_task->context0.rip     = (uint64_t)switch_to_user_mode;
    new_task->context0.rsp     = (uint64_t)stack_top;                        // 设置上下文
    new_task->kernel_stack     = (new_task->context0.rsp &= ~(uint64_t)0xF); // 栈16字节对齐
    new_task->context0.cs      = 0x8;
    new_task->context0.ss      = 0x10;
    new_task->context0.es      = 0x10;
    new_task->context0.ds      = 0x10;
    new_task->status           = CREATE;
    new_task->main             = (uint64_t)_start;

    new_task->cwd     = vfs_open(cwd);
    new_task->str_cwd = cwd;

    new_task->user_info             = current_user;

    stack_top = (uint64_t *)new_task->kernel_stack;
    PUSH_STACK(stack_top, (uint64_t)_start);
    PUSH_STACK(stack_top, (uint64_t)switch_to_user_mode);
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

size_t create_signal_thread(void *_start, 
                            uint64_t msg_type, uint64_t data1, uint64_t data2, uint64_t revert_func, 
                            pcb_t pcb)
{
    spin_lock(create_thread_lock);
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
    memcpy(new_task->name, "Window Message Thread", strlen("Window Message Thread") + 1);

    ptr_cast_t cast;
    cast.ptr                   = new_task;
    cast.val                  += KERNEL_STACK_SIZE;
    uint64_t *stack_top        = (uint64_t *)(cast.ptr);
    new_task->context0.rflags  = 0x202;
    new_task->context0.rip     = (uint64_t)jump_to_signal;
    new_task->context0.rsp     = (uint64_t)stack_top;                        // 设置上下文
    new_task->kernel_stack     = (new_task->context0.rsp &= ~(uint64_t)0xF); // 栈16字节对齐
    new_task->context0.cs      = 0x8;
    new_task->context0.ss      = 0x10;
    new_task->context0.es      = 0x10;
    new_task->context0.ds      = 0x10;
    new_task->status           = CREATE;
    new_task->main             = (uint64_t)_start;

    new_task->context0.rdi     = msg_type;
    new_task->context0.rsi     = data1;
    new_task->context0.rdx     = data2;
    new_task->context0.rcx     = revert_func;

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
    pcb_t pcb        = (pcb_t)malloc(sizeof(struct process_control_block));
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

    pcb->cmdline      = (char *)malloc(strlen(cmdline));
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
    kernel_pcb->child_pcb   = queue_init();
    kernel_pcb->child_index = 0;

    kernel_group = kernel_pcb;

    

    smp_scheduler_lock = false;
    scheduler_is_ready++;

    write_serial_fmt("Setup process <%s> PID:%zu\n", kernel_pcb->name, kernel_pcb->pid);
}
