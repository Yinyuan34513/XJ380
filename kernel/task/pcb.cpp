#include "task/pcb.h"
#include "cpu/regio.h"
#include "krlibc.h"
#include "mm/heap.h"
#include "mm/page.h"
#include "proto.h"

#define SA_RPL3 3

#define SA_RPL_MASK      0xFFFC
#define SA_TI_MASK       0xFFFB
#define GET_SEL(cs, rpl) ((cs & SA_RPL_MASK & SA_TI_MASK) | (rpl))

#define PUSH_STACK(stack, value) *(--stack) = value
static size_t now_pid         = 0;
size_t        now_tid         = 0;
lock_queue   *pcb_group_queue = NULL;
pcb_t         kernel_group    = NULL;

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

void switch_to_user_mode() {
    close_interrupt;
    uint64_t rsp = (uint64_t)get_current_task()->user_stack_top;
    get_current_task()->context0.rflags = 0 << 12 | 0b10 | 1 << 9;
    uint64_t func = get_current_task()->main;

    __asm__ volatile("mov %0, %%es\n"
                     "mov %0, %%ds\n"
                     "pushq %5\n" // SS
                     "pushq %1\n" // RSP
                     "pushq %2\n" // RFLAGS
                     "pushq %3\n" // CS
                     "pushq %4\n" // RIP
                     "iretq\n"
            :
            : "r"((uint64_t)GET_SEL(4 * 8, SA_RPL3)), "r"(rsp),
              "r"(get_current_task()->context0.rflags), "r"((uint64_t)0x23),
              "r"(func), "r"((uint64_t)0x1b)
            : "memory");
}

size_t create_kernel_thread(void *_start, void *args, char *name, pcb_t pcb)
{
    close_interrupt;
    disable_scheduler();
    tcb_t new_task = (tcb_t)malloc(KERNEL_STACK_SIZE);

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
    PUSH_STACK(stack_top, (uint64_t)args);
    PUSH_STACK(stack_top, (uint64_t)process_exit);
    PUSH_STACK(stack_top, (uint64_t)_start);
    new_task->context0.rflags = 0x202;
    new_task->context0.rip    = (uint64_t)_start;
    new_task->context0.rsp    = (uint64_t)stack_top;                        // 设置上下文
    new_task->kernel_stack    = (new_task->context0.rsp &= ~(uint64_t)0xF); // 栈16字节对齐
    new_task->context0.cs     = 0x8;
    new_task->context0.ss     = 0x10;
    new_task->context0.es     = 0x10;
    new_task->context0.ds     = 0x10;
    new_task->status          = CREATE;
    new_task->main            = (uint64_t)_start;

    new_task->fs_base = (uint64_t)new_task;
    add_task(new_task);
    enable_scheduler();
    open_interrupt;
    return new_task->tid;
}

size_t create_user_thread(void *_start, void *args, char *name, pcb_t pcb) 
{
    close_interrupt;
    disable_scheduler();
    tcb_t new_task = (tcb_t)malloc(KERNEL_STACK_SIZE);

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
    PUSH_STACK(stack_top, (uint64_t)args);
    PUSH_STACK(stack_top, (uint64_t)process_exit);
    PUSH_STACK(stack_top, (uint64_t)switch_to_user_mode);
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

    new_task->user_stack =
            page_alloc_random(pcb->pagedir, BIG_USER_STACK, PTE_PRESENT | PTE_WRITEABLE | PTE_USER);
    new_task->user_stack_top = new_task->user_stack + BIG_USER_STACK;

    new_task->fs_base = (uint64_t)new_task;
    add_task(new_task);
    enable_scheduler();
    open_interrupt;
    return new_task->tid;
}

size_t create_process_group(const char* name,pcb_t parent,page_directory_t *directory){
    pcb_t pcb = (pcb_t)malloc(sizeof(struct process_control_block));
    pcb->pid = now_pid++;
    pcb->parent_task = parent == NULL ? kernel_group : parent;
    pcb->status = START;
    pcb->queue_index = queue_enqueue(pcb_group_queue, pcb);
    strcpy(pcb->name,name);
    pcb->thread_queue = queue_init();
    pcb->ppid = 0;
    pcb->pagedir = directory ? directory : get_kernel_pagedir();
    return pcb->pid;
}

void process_setup()
{
    pcb_group_queue     = queue_init();
    pcb_t kernel_pcb    = (pcb_t)malloc(sizeof(struct process_control_block));
    kernel_pcb->pid     = now_pid++;
    kernel_pcb->pagedir = get_kernel_pagedir();
    strcpy(kernel_pcb->name, "XSK System");
    kernel_pcb->parent_task = kernel_pcb;
    kernel_pcb->status      = RUNNING;
    kernel_pcb->queue_index = queue_enqueue(pcb_group_queue, kernel_pcb);
    kernel_group            = kernel_pcb;

    tcb_t idle_thread         = (tcb_t)malloc(KERNEL_STACK_SIZE);
    idle_thread->tid          = now_tid++;
    idle_thread->parent_group = kernel_pcb;
    idle_thread->kernel_stack = get_rsp();
    idle_thread->status       = RUNNING;
    strcpy(idle_thread->name, "idle thread");
    idle_thread->group_index  = queue_enqueue(kernel_pcb->thread_queue, idle_thread);
    idle_thread->context0.rsp = get_rsp();
    set_kernel_stack(get_rsp());
    idle_thread->queue_index = queue_enqueue(get_current_cpu()->scheduler_queue, idle_thread);

    write_serial_fmt("Setup process <%s> PID:%zu\n", kernel_pcb->name, kernel_pcb->pid);
}
