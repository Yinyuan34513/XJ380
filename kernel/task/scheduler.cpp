#include "cpu/fpu.h"
#include "cpu/fsgsbase.h"
#include "cpu/lock.h"
#include "lock_queue.h"
#include "proto.hpp"
#include "smp/smp.h"
#include "task/pcb.h"

volatile bool        is_scheduler = false;
extern lock_queue   *pcb_group_queue;
extern XSK_SMP_INFO *xsi;
extern bool          no_interrupt;

void enable_scheduler()
{
    is_scheduler = true;
}

void disable_scheduler()
{
    is_scheduler = false;
}

tcb_t get_current_task()
{
    return get_current_cpu()->current_task;
}

__attribute__((naked)) void save_registers()
{
    __asm__ volatile(".intel_syntax noprefix\n\t"
                     "cli\n\t"
                     "push 0\n\t" // 对齐
                     "push 0\n\t" // 对齐
                     "push r15\n\t"
                     "push r14\n\t"
                     "push r13\n\t"
                     "push r12\n\t"
                     "push r11\n\t"
                     "push r10\n\t"
                     "push r9\n\t"
                     "push r8\n\t"
                     "push rdi\n\t"
                     "push rsi\n\t"
                     "push rbp\n\t"
                     "push rdx\n\t"
                     "push rcx\n\t"
                     "push rbx\n\t"
                     "push rax\n\t"
                     "mov rax, es\n\t"
                     "push rax\n\t"
                     "mov rax, ds\n\t"
                     "push rax\n\t"
                     "mov rdi, rsp\n\t"
                     "call timer_handle\n\t"
                     "mov rsp, rax\n\t"
                     "pop rax\n\t"
                     "mov ds, rax\n\t"
                     "pop rax\n\t"
                     "mov es, rax\n\t"
                     "pop rax\n\t"
                     "pop rbx\n\t"
                     "pop rcx\n\t"
                     "pop rdx\n\t"
                     "pop rbp\n\t"
                     "pop rsi\n\t"
                     "pop rdi\n\t"
                     "pop r8\n\t"
                     "pop r9\n\t"
                     "pop r10\n\t"
                     "pop r11\n\t"
                     "pop r12\n\t"
                     "pop r13\n\t"
                     "pop r14\n\t"
                     "pop r15\n\t"
                     "add rsp, 16\n\t" // 越过对齐
                     "sti\n\t"
                     "iretq\n\t");
}

void change_proccess(registers_t *reg, tcb_t current_task0, tcb_t target)
{
    switch_page_directory(target->parent_group->pagedir);
    set_kernel_stack(target->kernel_stack);

    __asm__ __volatile__("movq %0, %%fs\n\t" ::"r"(target->fs));
    write_fsbase(target->fs_base);

    save_fpu_context(&current_task0->fpu_context);
    restore_fpu_context(&target->fpu_context);

    current_task0->context0.r15    = reg->r15;
    current_task0->context0.r14    = reg->r14;
    current_task0->context0.r13    = reg->r13;
    current_task0->context0.r12    = reg->r12;
    current_task0->context0.r11    = reg->r11;
    current_task0->context0.r10    = reg->r10;
    current_task0->context0.r9     = reg->r9;
    current_task0->context0.r8     = reg->r8;
    current_task0->context0.rax    = reg->rax;
    current_task0->context0.rbx    = reg->rbx;
    current_task0->context0.rcx    = reg->rcx;
    current_task0->context0.rdx    = reg->rdx;
    current_task0->context0.rdi    = reg->rdi;
    current_task0->context0.rsi    = reg->rsi;
    current_task0->context0.rbp    = reg->rbp;
    current_task0->context0.rflags = reg->rflags;
    current_task0->context0.rip    = reg->rip;
    current_task0->context0.rsp    = reg->rsp;
    current_task0->context0.ss     = reg->ss;
    current_task0->context0.es     = reg->es;
    current_task0->context0.cs     = reg->cs;
    current_task0->context0.ds     = reg->ds;

    reg->r15    = target->context0.r15;
    reg->r14    = target->context0.r14;
    reg->r13    = target->context0.r13;
    reg->r12    = target->context0.r12;
    reg->r11    = target->context0.r11;
    reg->r10    = target->context0.r10;
    reg->r9     = target->context0.r9;
    reg->r8     = target->context0.r8;
    reg->rax    = target->context0.rax;
    reg->rbx    = target->context0.rbx;
    reg->rcx    = target->context0.rcx;
    reg->rdx    = target->context0.rdx;
    reg->rdi    = target->context0.rdi;
    reg->rsi    = target->context0.rsi;
    reg->rbp    = target->context0.rbp;
    reg->rflags = target->context0.rflags;
    reg->rip    = target->context0.rip;
    reg->rsp    = target->context0.rsp;
    reg->ss     = target->context0.ss;
    reg->es     = target->context0.es;
    reg->ds     = target->context0.ds;
    reg->cs     = target->context0.cs;
}

tcb_t select_next_task()
{
    tcb_t next;
    if (get_current_cpu()->scheduler_queue->size == 1) { return get_current_task(); }
    if (get_current_cpu()->iter_node == NULL)
    {
    iter_head:
        get_current_cpu()->iter_node = get_current_cpu()->scheduler_queue->head;
        next                         = (tcb_t)get_current_cpu()->iter_node->data;
    }
    else
    {
    resche:
        get_current_cpu()->iter_node = get_current_cpu()->iter_node->next;
        if (get_current_cpu()->iter_node == NULL) goto iter_head;
        next = (tcb_t)get_current_cpu()->iter_node->data;

        if (next == NULL) goto iter_head;   // 这个很明显有问题，理论上不用加判断（既然有这个节点就应该已经完成创建了，怎么会是空的呢？）

        // 死亡/回收/挂起的线程不调度
        switch (next->status)
        {
        case FUTEX:
        case DEATH:
        case OUT: goto resche;
        default:
            if (next->parent_group->status == DEATH || next->parent_group->status == FUTEX) { goto resche; }
        }

        if (next->task_level == TASK_IDLE_LEVEL) goto resche;
    }
    return next;
}

extern pcb_t kernel_group;

extern "C" registers_t *timer_handle(registers_t *reg)
{
    // send_eoi();
    if (!is_scheduler)
    {
        send_eoi();
        return reg;
    }

    tcb_t current = get_current_task();
    tcb_t best    = select_next_task();
    if (best == current)
    {
        send_eoi();
        return reg;
    }
    write_fsbase((uint64_t)current); // 下面要用内核态的fs，换上

    // 任务寻父处理
    if (best->parent_group->parent_task == NULL || best->parent_group->parent_task->status == DEATH ||
        best->parent_group->parent_task->status == OUT)
    {
        best->parent_group->parent_task = kernel_group;
    }

    // 正式切换
    if (get_current_cpu()->current_task != best)
    {
        disable_scheduler();
        if (current->status == RUNNING) { current->status = START; }
        if (best->status == START || best->status == CREATE) { best->status = RUNNING; }
        if (best->context0.rip != 0)
        {
            change_proccess(reg, get_current_cpu()->current_task, best);
            // write_serial_fmt("best.name=%s, RIP=0x%016llx\n", best->name, best->context0.rip);
            get_current_cpu()->current_task = best;
        }
        enable_scheduler();
    }
    else
        write_fsbase(current->fs_base); // 同样，没切任务，换回来

    send_eoi();
    return reg;
}

void scheduler_yield()
{
    if (!is_scheduler) return;
    __asm__ volatile("int %0" ::"i"(32));
}

void debug_sched_queue(lock_queue *q)
{
    if (q == NULL) return;
    spin_lock(q->lock);
    lock_node *current = q->head;
    write_serial_fmt("Process %d Scheduler Queue: ", get_current_cpu()->lapic_id);
    while (current != NULL)
    {
        write_serial_fmt("-> %s", ((tcb_t)current->data)->name);
        current = current->next;
    }
    write_serial_string("\n");
    spin_unlock(q->lock);
}

size_t add_task(tcb_t new_task)
{
    bool interrupt_yes = false;
    if (!no_interrupt) interrupt_yes = true;
    no_interrupt = true;

    close_interrupt;
    disable_scheduler();
    if (new_task == NULL) return -1;
    PROCESSOR_INFO *cpu0 = get_cpu(get_bsp());
    for (int i = 0; i < get_cpu_num(); i++)
    {
        PROCESSOR_INFO *cpui = get_cpu(i);
        if (cpui->scheduler_queue->size < cpu0->scheduler_queue->size) { cpu0 = cpui; }
    }
    new_task->cpu_id      = cpu0->processor_id;
    new_task->queue_index = queue_enqueue(cpu0->scheduler_queue, new_task);
    // debug_sched_queue(cpu0->scheduler_queue);
    if (interrupt_yes) no_interrupt = false;
    return new_task->queue_index;
}
