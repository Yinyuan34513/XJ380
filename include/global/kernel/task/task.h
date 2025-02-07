#ifndef __TASK_H__
#define __TASK_H__

#include "../../../include.h"

#define CLONE_FS (1 << 0)
#define CLONE_FILES (1 << 1)
#define CLONE_SIGNAL (1 << 2)

// stack size 16K
// UL一定要加，否则获取current_task之后读取值的时候会出错
#define STACK_SIZE 16384UL
#define USER_STACK_START 0x6ffff0a00000UL

#define TASK_RUNNING (1 << 0)
#define TASK_INTERRUPTIBLE (1 << 1)
#define TASK_UNINTERRUPTIBLE (1 << 2)
#define TASK_ZOMBIE (1 << 3)
#define TASK_STOPPED (1 << 4)

struct elf_mapping_space
{
    uint64_t load_start;
    uint64_t load_end;
};

struct mm_struct
{
    uint64_t *pgd; // page table point

    uint64_t start_code, end_code;
    uint64_t start_data, end_data;
    uint64_t start_rodata, end_rodata;
    uint64_t start_brk, end_brk;
    uint64_t start_stack;
};

struct thread_struct
{
    struct elf_mapping_space *elf_mappings;

    uint64_t rsp0; // in tss

    uint64_t rip;
    uint64_t rsp;

    uint64_t fs;
    uint64_t gs;

    uint64_t cr2;
    uint64_t trap_nr;
    uint64_t error_code;
};

#define MAX_FD_NUM 16

struct task_struct
{
    volatile long state;
    uint64_t flags;
    long preempt_count;
    long signal;
    long cpu_id; // CPU ID

    struct mm_struct *mm;
    struct thread_struct *thread;

    struct List list;

    uint64_t addr_limit; /*0x0000,0000,0000,0000 - 0x0000,7fff,ffff,ffff user*/ /*0xffff,8000,0000,0000 - 0xffff,ffff,ffff,ffff kernel*/

    long pid;
    long priority;
    long vrun_time;

    struct VFile *fds[MAX_FD_NUM];
    struct v_node *cwd_node;
    int fd_count;

    wait_queue_t wait_childexit;
    uint64_t exit_code;

    struct task_struct *next;
    struct task_struct *parent;
};

///////struct task_struct->flags:

#define PF_KTHREAD (1UL << 0)
#define NEED_SCHEDULE (1UL << 1)

union task_union
{
    struct task_struct task;
    uint64_t stack[STACK_SIZE / sizeof(uint64_t)];
} __attribute__((aligned(STACK_SIZE)));

#define INIT_TASK(tsk)                    \
    {                                     \
        .state = TASK_UNINTERRUPTIBLE,    \
        .flags = PF_KTHREAD,              \
        .preempt_count = 0,               \
        .signal = 0,                      \
        .cpu_id = 0,                      \
        .mm = &init_mm,                   \
        .thread = &init_thread,           \
        .addr_limit = 0xffff800000000000, \
        .pid = 0,                         \
        .priority = 2,                    \
        .vrun_time = 0}

struct tss_struct
{
    unsigned int reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    unsigned short reserved3;
    unsigned short iomapbaseaddr;
} __attribute__((packed));

#define INIT_TSS                                                                \
    {.reserved0 = 0,                                                            \
     .rsp0 = (uint64_t)(init_task_union.stack + STACK_SIZE / sizeof(uint64_t)), \
     .rsp1 = (uint64_t)(init_task_union.stack + STACK_SIZE / sizeof(uint64_t)), \
     .rsp2 = (uint64_t)(init_task_union.stack + STACK_SIZE / sizeof(uint64_t)), \
     .reserved1 = 0,                                                            \
     .ist1 = 0xffff800000007c00,                                                \
     .ist2 = 0xffff800000007c00,                                                \
     .ist3 = 0xffff800000007c00,                                                \
     .ist4 = 0xffff800000007c00,                                                \
     .ist5 = 0xffff800000007c00,                                                \
     .ist6 = 0xffff800000007c00,                                                \
     .ist7 = 0xffff800000007c00,                                                \
     .reserved2 = 0,                                                            \
     .reserved3 = 0,                                                            \
     .iomapbaseaddr = 0}

extern struct task_struct *user_current[MAX_CPU_NUM];

struct task_struct *get_current()
{
    struct task_struct *current = NULL;
    __asm__ __volatile__("andq %%rsp, %0\n\t" : "=r"(current) : "0"(~(STACK_SIZE - 1)));
    if ((uint64_t)current < 0xFFFF800000000000)
    {
        uint64_t lapicid = lapic_id();
        uint64_t current_cpu_cpuid = get_cpuid_by_lapicid(lapicid);
        current = user_current[current_cpu_cpuid];
    }
    return current;
}

#define current_task get_current()

static inline void switch_mm(struct task_struct *prev, struct task_struct *next)
{
    __asm__ __volatile__("movq %0, %%cr3" : : "r"(next->mm->pgd));
}

#define switch_to(prev, next)                                                                       \
    do                                                                                              \
    {                                                                                               \
        __asm__ __volatile__("pushq	%%rbp	\n\t"                                                     \
                             "pushq	%%rax	\n\t"                                                     \
                             "movq	%%rsp,	%0	\n\t"                                                  \
                             "movq	%2,	%%rsp	\n\t"                                                  \
                             "leaq	1f(%%rip),	%%rax	\n\t"                                           \
                             "movq	%%rax,	%1	\n\t"                                                  \
                             "pushq	%3		\n\t"                                                       \
                             "jmp	__switch_to	\n\t"                                                 \
                             "1:	\n\t"                                                              \
                             "popq	%%rax	\n\t"                                                      \
                             "popq	%%rbp	\n\t"                                                      \
                             : "=m"(prev->thread->rsp), "=m"(prev->thread->rip)                     \
                             : "m"(next->thread->rsp), "m"(next->thread->rip), "D"(prev), "S"(next) \
                             : "memory");                                                           \
    } while (0)

void insert_task_queue(struct task_struct *tsk, int cpu_id);

static inline void wakeup_task(struct task_struct *tsk)
{
    tsk->state = TASK_RUNNING;
    insert_task_queue(tsk, current_task->cpu_id);
    current_task->flags |= NEED_SCHEDULE;
}

uint64_t do_fork(struct X64_REGS *regs, uint64_t clone_flags, uint64_t stack_start, uint64_t stack_size);
extern "C" uint64_t do_exit(uint64_t code);
extern "C" uint64_t do_execve(struct X64_REGS_SYSCALL *regs, const char *name, char *argv[], char *envp[]);
void init_task_manager();

extern "C" void ret_from_syscall(void);
extern "C" void kernel_thread_func(void);

extern struct task_struct *init_task[MAX_CPU_NUM];
extern union task_union init_task_union;
extern struct mm_struct init_mm;
extern struct thread_struct init_thread;

extern struct tss_struct init_tss[MAX_CPU_NUM];

#endif
