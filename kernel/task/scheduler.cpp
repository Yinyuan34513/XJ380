#include "cpu/fpu.h"
#include "cpu/fsgsbase.h"
#include "cpu/lock.h"
#include "dlinker.h"
#include "lock_queue.h"
#include "proto.hpp"
#include "smp/smp.h"
#include "task/pcb.h"

volatile bool        is_scheduler = false;
extern lock_queue   *pcb_group_queue;
extern XSK_SMP_INFO *xsi;
extern bool          no_interrupt;
const uint64_t       TIME_SLICE = 4;
const uint64_t       MIN_SLICE  = 1;

static bool trace_fsbase_task(tcb_t task)
{
    return task != NULL && task->parent_group != NULL && task->parent_group->linux_abi &&
           (strstr(task->name, "opkg") != NULL || strstr(task->parent_group->name, "opkg") != NULL);
}

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
EXPORT_SYMBOL(get_current_task);

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
    uint64_t current_fs_base = read_fsbase();
    if (current_fs_base != 0 || current_task0->fs_base == 0)
        current_task0->fs_base = current_fs_base;
    else if (trace_fsbase_task(current_task0))
        write_serial_fmt("[fsbase] switch-save-zero-ignored task=%s tid=%llu tcb=0x%llx hw=0x%llx rip=0x%llx target=%s target_fs=0x%llx\n",
                         current_task0->name, current_task0->tid, current_task0->fs_base, current_fs_base,
                         reg != NULL ? reg->rip : 0, target != NULL ? target->name : "(null)",
                         target != NULL ? target->fs_base : 0);

    switch_page_directory(target->parent_group->pagedir);
    set_kernel_stack(target->kernel_stack);
    write_kgsbase((uint64_t)get_current_cpu());

    if ((target->context0.cs & 0x3) == 0x3) {
        // Returning to user mode: keep user GSBASE clear and leave CPU-local
        // data in KGSBASE for the next syscall/swapgs entry.
        write_gsbase(0);
        __asm__ __volatile__("movq %0, %%fs\n\t" ::"r"(task_user_fs_selector(target)));
    } else {
        // Returning to kernel mode (for example, resuming inside a syscall):
        // kernel code expects %gs to point at the current CPU area.
        uint64_t fs_selector = target->fs;
        if (target->task_level != TASK_APPLICATION_LEVEL && fs_selector == 0)
        {
            fs_selector = 0x10;
        }
        write_gsbase((uint64_t)get_current_cpu());
        __asm__ __volatile__("movq %0, %%fs\n\t" ::"r"(fs_selector));
    }
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

static inline bool is_task_schedulable(tcb_t task, tcb_t current)
{
    if (task == NULL) return false;
    if (task == current) return false;
    if (task->status == DEATH || task->status == OUT || task->status == FUTEX) return false;
    if (task->task_level == TASK_IDLE_LEVEL) return false;
    if (task->parent_group == NULL) return false;
    if (task->parent_group->status == DEATH || task->parent_group->status == FUTEX) return false;
    return true;
}

tcb_t select_next_task_safe()
{
    spin_lock(&scheduler_lock);
    
    struct PROCESSOR_INFO *cpu = get_current_cpu();
    lock_queue *queue = cpu->scheduler_queue;
    
    if (queue == NULL || queue->size == 0) {
        spin_unlock(&scheduler_lock);
        return NULL;
    }
    
    tcb_t current = get_current_task();
    if (queue->head == NULL) {
        spin_unlock(&scheduler_lock);
        return NULL;
    }

    if (queue->rr_cursor == NULL) {
        queue->rr_cursor = queue->head;
    }

    lock_node *start_node = queue->rr_cursor;
    lock_node *node = start_node;
    size_t scanned = 0;
    while (node != NULL && scanned < queue->size) {
        tcb_t candidate = (tcb_t)node->data;
        if (is_task_schedulable(candidate, current)) {
            queue->rr_cursor = node->next != NULL ? node->next : queue->head;
            spin_unlock(&scheduler_lock);
            return candidate;
        }

        node = node->next != NULL ? node->next : queue->head;
        ++scanned;
        if (node == start_node) break;
    }

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

    PROCESSOR_INFO *cpu = get_current_cpu();
    if (current->status == RUNNING &&
        current->task_level != TASK_IDLE_LEVEL &&
        current->status != FUTEX &&
        current->status != DEATH &&
        current->status != OUT) {
        cpu->scheduler_ticks++;
        if (cpu->scheduler_ticks < TIME_SLICE) {
            send_eoi();
            return reg;
        }
    } else {
        cpu->scheduler_ticks = 0;
    }

    tcb_t best = select_next_task();
    if (best == NULL || best == current) {
        cpu->scheduler_ticks = 0;
        send_eoi();
        return reg;
    }

    // 记录当前线程运行时最新的 FSBASE，避免调度后丢失用户态 TLS 环境。
    uint64_t current_fs_base = read_fsbase();
    if (current_fs_base != 0 || current->fs_base == 0)
        current->fs_base = current_fs_base;
    else if (trace_fsbase_task(current))
        write_serial_fmt("[fsbase] timer-save-zero-ignored task=%s tid=%llu tcb=0x%llx hw=0x%llx rip=0x%llx next=%s next_fs=0x%llx\n",
                         current->name, current->tid, current->fs_base, current_fs_base, reg != NULL ? reg->rip : 0,
                         best != NULL ? best->name : "(null)", best != NULL ? best->fs_base : 0);

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
            cpu->scheduler_ticks = 0;
            change_proccess(reg, current, best);
            cpu->current_task = best;
        } else {
            // 如果上下文无效，不切换并恢复状态
            current->status = RUNNING;
            cpu->scheduler_ticks = 0;
        }
        
        enable_scheduler();
    }
    send_eoi();
    return reg;
}

void scheduler_yield()
{
    if (!is_scheduler) return;
    get_current_cpu()->scheduler_ticks = TIME_SLICE;
    __asm__ volatile("int %0" ::"i"(32));
}
EXPORT_SYMBOL(scheduler_yield);

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

static PROCESSOR_INFO *find_cpu_by_lapic_id(uint64_t lapic_id, size_t *cpu_index)
{
    for (size_t i = 0; i < get_cpu_num(); i++) {
        PROCESSOR_INFO *cpu = get_cpu(i);
        if (cpu != NULL && cpu->lapic_id == lapic_id) {
            if (cpu_index != NULL) *cpu_index = i;
            return cpu;
        }
    }

    if (cpu_index != NULL) *cpu_index = 0;
    return NULL;
}

static PROCESSOR_INFO *get_bsp_cpu_info(size_t *cpu_index)
{
    PROCESSOR_INFO *cpu = find_cpu_by_lapic_id(get_bsp(), cpu_index);
    if (cpu != NULL) return cpu;

    if (cpu_index != NULL) *cpu_index = 0;
    return get_cpu(0);
}

size_t add_task(tcb_t new_task)
{
    if (new_task == NULL) return -1;
	    
    spin_lock(&scheduler_lock);
    
    // 确保任务有基本设置
    if (new_task->status == CREATE) {
        new_task->status = START;
    }
    
    struct PROCESSOR_INFO *min_cpu = NULL;
    size_t                 min_cpu_index = 0;

    if (new_task->task_level == TASK_APPLICATION_LEVEL) {
        // User/message threads are not stable on APs yet. Force them onto the
        // BSP until per-CPU syscall GS state is reliable on every core.
        min_cpu = get_bsp_cpu_info(&min_cpu_index);
    } else {
        // 内核线程仍然按负载最少分配
        min_cpu = get_cpu(0);
        for (int i = 1; i < get_cpu_num(); i++) {
            struct PROCESSOR_INFO *cpui = get_cpu(i);
            if (cpui->scheduler_queue->size < min_cpu->scheduler_queue->size) {
                min_cpu       = cpui;
                min_cpu_index = i;
            }
        }
    }
	    
    new_task->cpu_id      = min_cpu_index;
    new_task->queue_index = queue_enqueue_ref(min_cpu->scheduler_queue, new_task, &new_task->sched_node);

    // debug_sched_queue(min_cpu->scheduler_queue);
    
    spin_unlock(&scheduler_lock);
    return new_task->queue_index;
}

void remove_task(tcb_t task) {
    if (task == NULL) return;
    spin_lock(&scheduler_lock);

    if (task->cpu_id < get_cpu_num()) {
        PROCESSOR_INFO *cpu = get_cpu(task->cpu_id);
        if (cpu != NULL &&
            cpu->scheduler_queue != NULL &&
            task->sched_node != NULL &&
            task->sched_node->data == task) {
            queue_remove_node(cpu->scheduler_queue, task->sched_node);
            task->sched_node = NULL;
            spin_unlock(&scheduler_lock);
            return;
        }
    }

    for (size_t i = 0; i < get_cpu_num(); i++) {
        PROCESSOR_INFO *cpu = get_cpu(i);
        if (cpu == NULL || cpu->scheduler_queue == NULL) continue;
        lock_node *node = cpu->scheduler_queue->head;
        while (node != NULL && node->data != task) node = node->next;
        if (node == NULL) continue;

        task->cpu_id = i;
        task->sched_node = node;
        queue_remove_node(cpu->scheduler_queue, task->sched_node);
        task->sched_node = NULL;
        break;
    }

    spin_unlock(&scheduler_lock);
}
