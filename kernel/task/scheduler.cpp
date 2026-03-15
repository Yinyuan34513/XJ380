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

spin_t scheduler_lock = SPIN_INIT;

tcb_t select_next_task_safe()
{
    spin_lock(&scheduler_lock);
    
    struct PROCESSOR_INFO *cpu = get_current_cpu();
    lock_queue *queue = cpu->scheduler_queue;
    
    if (queue == NULL || queue->size == 0) {
        spin_unlock(&scheduler_lock);
        return NULL;
    }
    
    // 如果只有一个任务，直接返回
    if (queue->size == 1) {
        tcb_t only_task = (tcb_t)queue->head->data;
        spin_unlock(&scheduler_lock);
        
        // 检查任务状态
        if (only_task->status == RUNNING || only_task->status == START) {
            return only_task;
        }
        return NULL;
    }
    
    // 从当前任务开始寻找下一个可运行任务
    tcb_t current = get_current_task();
    lock_node *start_node = NULL;
    lock_node *node = queue->head;
    
    // 找到当前任务在队列中的位置
    while (node != NULL) {
        if (node->data == current) {
            start_node = node;
            break;
        }
        node = node->next;
    }
    
    // 如果没找到当前任务，从头开始
    if (start_node == NULL) {
        start_node = queue->head;
    }
    
    // 从下一个节点开始循环查找
    lock_node *current_node = start_node->next ? start_node->next : queue->head;
    int attempts = 0;
    const int max_attempts = queue->size * 2; // 防止无限循环
    
    while (attempts < max_attempts) {
        tcb_t candidate = (tcb_t)current_node->data;
        
        // 检查任务状态是否可调度
        if (candidate != current && 
            candidate->status != DEATH && 
            candidate->status != OUT && 
            candidate->status != FUTEX &&
            candidate->task_level != TASK_IDLE_LEVEL) {
            
            // 检查父进程状态
            if (candidate->parent_group && 
                candidate->parent_group->status != DEATH && 
                candidate->parent_group->status != FUTEX) {
                
                spin_unlock(&scheduler_lock);
                return candidate;
            }
        }
        
        // 移动到下一个节点
        current_node = current_node->next ? current_node->next : queue->head;
        attempts++;
        
        // 如果又回到起点，说明没找到合适的任务
        if (current_node == start_node) {
            break;
        }
    }
    
    // 没找到其他任务，返回当前任务或NULL
    tcb_t result = (current->status == RUNNING || current->status == START) ? current : NULL;
    spin_unlock(&scheduler_lock);
    return result;
}

tcb_t select_next_task()
{
    tcb_t next = select_next_task_safe();
    
    // 如果没有找到合适的任务，返回当前任务（如果可运行）
    if (next == NULL) {
        tcb_t current = get_current_task();
        if (current && (current->status == RUNNING || current->status == START)) {
            return current;
        }
        return NULL;
    }
    
    return next;
}

extern pcb_t kernel_group;
extern "C" registers_t *timer_handle(registers_t *reg)
{
    // send_eoi();
    if (!is_scheduler) { 
        send_eoi();
        return reg; 
    }

    tcb_t current = get_current_task();
    if (current == NULL) {
        send_eoi();
        return reg;
    }

    tcb_t best = select_next_task();
    if (best == NULL || best == current) {
        send_eoi();
        return reg;
    }

    // 保存当前FSBASE
    write_fsbase(current->fs_base);

    // 任务寻父处理
    if (best->parent_group && 
        (best->parent_group->parent_task == NULL || 
         best->parent_group->parent_task->status == DEATH || 
         best->parent_group->parent_task->status == OUT)) {
        best->parent_group->parent_task = kernel_group;
    }

    // 正式切换
    if (get_current_cpu()->current_task != best) {
        disable_scheduler();
        
        // 更新任务状态
        if (current->status == RUNNING) { 
            current->status = START; 
        }
        if (best->status == START || best->status == CREATE) { 
            best->status = RUNNING; 
        }
        
        // 只有上下文有效的任务才切换
        if (best->context0.rip != 0) {
            change_proccess(reg, current, best);
            get_current_cpu()->current_task = best;
        } else {
            // 如果上下文无效，不切换并恢复状态
            current->status = RUNNING;
        }
        
        enable_scheduler();
    } else {
        // 没切换任务，恢复原来的FSBASE
        write_fsbase(current->fs_base);
    }
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
    spin_lock(&q->lock);
    lock_node *current = q->head;
    write_serial_fmt("Process %d Scheduler Queue: ", get_current_cpu()->lapic_id);
    while (current != NULL)
    {
        write_serial_fmt("-> %s", ((tcb_t)current->data)->name);
        current = current->next;
    }
    write_serial_string("\n");
    spin_unlock(&q->lock);
}

size_t add_task(tcb_t new_task)
{
    if (new_task == NULL) return -1;
    
    spin_lock(&scheduler_lock);
    
    // 确保任务有基本设置
    if (new_task->status == CREATE) {
        new_task->status = START;
    }
    
    // 选择负载最少的CPU
    struct PROCESSOR_INFO *min_cpu = get_cpu(0);
    for (int i = 1; i < get_cpu_num(); i++) {
        struct PROCESSOR_INFO *cpui = get_cpu(i);
        if (cpui->scheduler_queue->size < min_cpu->scheduler_queue->size) { 
            min_cpu = cpui; 
        }
    }
    
    new_task->cpu_id = min_cpu->processor_id;
    new_task->queue_index = queue_enqueue(min_cpu->scheduler_queue, new_task);

    // debug_sched_queue(min_cpu->scheduler_queue);
    
    spin_unlock(&scheduler_lock);
    return new_task->queue_index;
}

void remove_task(tcb_t task) {
    if (task == NULL) return;
    spin_lock(&scheduler_lock);

    PROCESSOR_INFO *smp = get_cpu(task->cpu_id);
    queue_remove_at(smp->scheduler_queue, task->queue_index);
    spin_unlock(&scheduler_lock);
}