#pragma once

#define TASK_KERNEL_LEVEL      0 // 内核任务 (崩溃后会挂起内核)
#define TASK_IDLE_LEVEL        1 // IDLE (崩溃后与普通内核任务相同行为)
#define TASK_APPLICATION_LEVEL 2 // 应用程序

#include "cpu/fpu.h"
#include "lock_queue.h"
#include "mm/page.h"
#include "stdint.h"
#include "user/user.h"

typedef struct registers
{
    uint64_t ds;
    uint64_t es;
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rbp;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t vector;   // 保留
    uint64_t err_code; // 保留
    // CPU自动压入
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} registers_t;

typedef struct process_control_block *pcb_t;
typedef struct thread_control_block  *tcb_t;

typedef enum
{
    CREATE  = 0, // 创建中
    RUNNING = 1, // 运行中
    WAIT    = 2, // 线程阻塞
    DEATH   = 3, // 死亡(无法被调度, 线程状态为等待处死)
    START   = 4, // 准备调度
    FUTEX   = 5, // 被挂起(无法被调度, 线程状态为等待唤醒)
    OUT     = 6, // 已被处死(无法被调度)
} TaskStatus;

typedef struct
{
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbx;
    uint64_t rbp;
    uint64_t rip;
    uint64_t rsp;
    uint64_t rflags;
    uint64_t rax;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t ss, cs, ds, es;
} TaskContext;

struct process_control_block
{
    size_t   pid;  // 进程ID
    uint64_t ppid; // 父进程ID

    char              name[32];     // 进程名称
    page_directory_t *pagedir;      // 页表目录
    pcb_t             parent_task;  // 父进程
    lock_queue       *thread_queue; // 线程队列
    size_t            queue_index;  // 进程队列索引
    TaskStatus        status;       // 进程状态

    char    *cwd;       // 工作目录路径
    char   **envp;      // 环境变量指针
    size_t   envc;      // 环境变量数量
    UserInfo user_info; // 用户信息

    bool        vfork;       // 是否是 vfork 创建的进程
    size_t      child_index; // 子进程队列索引
    lock_queue *child_pcb;   // 子进程列表
    char       *cmdline;
    int         task_level; // 进程等级
};
#define EHDR_START_ADDR 0x0000300000000000 // ELF头起始地址
uint64_t parse_elf_file(char *path, pcb_t group);
uint64_t process_execve(char *path, char **argv, char **envp);
struct vfs_node;
typedef struct vfs_node *vfs_node_t;
struct thread_control_block
{
    pcb_t parent_group; // 父进程
    int   task_level;   // 进程等级

    size_t        tid;         // 线程ID
    char          name[32];    // 线程名称
    TaskStatus    status;      // 线程状态
    size_t        cpu_id;      // 运行的CPU ID
    TaskContext   context0;    // 线程上下文
    fpu_context_t fpu_context; // 浮点寄存器上下文

    uint64_t   kernel_stack;   // 内核栈地址
    size_t     queue_index;    // 调度队列索引
    size_t     group_index;    // 进程队列索引
    uint64_t   main;           // 线程入口地址
    uint64_t   user_stack;     // 用户栈
    uint64_t   user_stack_top; // 用户栈顶部地址
    vfs_node_t cwd;
    vfs_node_t fds[256];
    uint64_t   syscall_stack;
    uint64_t   brk_start;
    uint64_t   brk_end;
    uint64_t   load_start;
    uint64_t   load_end;

    uint64_t fs_base; // FS段基址
    uint64_t fs;
};

extern pcb_t kernel_group;

size_t   add_task(tcb_t task);
void     enable_scheduler();
void     disable_scheduler();
tcb_t    get_current_task();
size_t   create_kernel_thread(void *_start, void *args, char *name, pcb_t pcb);
size_t   create_user_thread(void *_start, void *args, char *name, pcb_t pcb);
pcb_t    create_process_group(const char *name, pcb_t parent, page_directory_t *directory, char *cmdline);
uint64_t process_fork(struct X64_REGS *reg, bool is_vfork, uint64_t user_stack);
void     process_setup();
void switch_to_user_mode();