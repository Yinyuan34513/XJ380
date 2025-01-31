#include "../../include/include.h"
#include "../alloc/page.h"

struct mm_struct init_mm = {0};

struct thread_struct init_thread =
    {
        .rsp0 = (uint64_t)(init_task_union.stack + STACK_SIZE / sizeof(uint64_t)),
        .rsp = (uint64_t)(init_task_union.stack + STACK_SIZE / sizeof(uint64_t)),
        .fs = SELECTOR_KERNEL_DS,
        .gs = SELECTOR_KERNEL_DS,
        .cr2 = 0,
        .trap_nr = 0,
        .error_code = 0};

union task_union init_task_union __attribute__((__section__(".data.init_task"))) = {INIT_TASK(init_task_union.task)};

struct task_struct *init_task[MAX_CPU_NUM] = {&init_task_union.task, 0};

struct tss_struct init_tss[MAX_CPU_NUM] = {INIT_TSS};

uint64_t init(uint64_t arg)
{
    write_serial_string("init task is running,arg:");
    write_serial_dec(arg);
    write_serial_string(".\n");

    while (1)
    {
        __asm__ __volatile__("hlt");
    }
}

uint64_t do_fork(struct X64_REGS *regs, uint64_t clone_flags, uint64_t stack_start, uint64_t stack_size)
{
    struct task_struct *tsk = NULL;
    struct thread_struct *thd = NULL;
    void *p = (void *)NULL;

    p = PAGE_ALLOC.alloc_page();
    tsk = (struct task_struct *)p;

    xmemset(tsk, 0, sizeof(*tsk));
    *tsk = *current_task;

    list_init(&tsk->list);

    tsk->priority = 2;
    tsk->pid++;
    tsk->preempt_count = 0;
    tsk->cpu_id = current_task->cpu_id;
    tsk->state = TASK_UNINTERRUPTIBLE;

    thd = (struct thread_struct *)(tsk + 1);
    xmemset(thd, 0, sizeof(*thd));
    tsk->thread = thd;

    thd->rsp0 = (uint64_t)tsk + STACK_SIZE;
    thd->rip = regs->rip;
    thd->rsp = (uint64_t)tsk + STACK_SIZE - sizeof(struct X64_REGS);
    xmemcpy((void *)thd->rsp, regs, sizeof(struct X64_REGS));
    thd->fs = SELECTOR_KERNEL_DS;
    thd->gs = SELECTOR_KERNEL_DS;

    if (!(tsk->flags & PF_KTHREAD))
        thd->rip = regs->rip = (uint64_t)ret_from_intr;

    tsk->state = TASK_RUNNING;
    insert_task_queue(tsk);

    return 1;
}

extern "C" uint64_t do_exit(uint64_t code)
{
    while (1)
        __asm__ __volatile__("pause");
}

extern "C" void kernel_thread_func(void);

int kernel_thread(uint64_t (*fn)(uint64_t), uint64_t arg, uint64_t flags)
{
    struct X64_REGS regs;
    xmemset(&regs, 0, sizeof(regs));

    regs.rbx = (uint64_t)fn;
    regs.rdx = (uint64_t)arg;

    regs.ds = SELECTOR_KERNEL_DS;
    regs.es = SELECTOR_KERNEL_DS;
    regs.cs = SELECTOR_KERNEL_CS;
    regs.ss = SELECTOR_KERNEL_DS;
    regs.rflags = (1 << 9);

    regs.rip = (uint64_t)kernel_thread_func;

    return do_fork(&regs, flags, 0, 0);
}

extern "C" void __switch_to(struct task_struct *prev, struct task_struct *next)
{
    init_tss[current_task->cpu_id].rsp0 = next->thread->rsp0;

    __asm__ __volatile__("movq	%%fs,	%0 \n\t" : "=a"(prev->thread->fs));
    __asm__ __volatile__("movq	%%gs,	%0 \n\t" : "=a"(prev->thread->gs));

    __asm__ __volatile__("movq	%0,	%%fs \n\t" ::"a"(next->thread->fs));
    __asm__ __volatile__("movq	%0,	%%gs \n\t" ::"a"(next->thread->gs));
}

void init_task_manager()
{
    uint64_t cr3;
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3));
    init_mm.pgd = (uint64_t *)cr3;

    init_tss[current_task->cpu_id].rsp0 = init_thread.rsp0;

    list_init(&init_task_union.task.list);

    kernel_thread(init, 10, CLONE_FS | CLONE_FILES | CLONE_SIGNAL);

    init_task_union.task.preempt_count = 0;
    init_task_union.task.state = TASK_RUNNING;
    init_task_union.task.cpu_id = 0;
}
