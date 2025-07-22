#pragma once

#include "stdint.h"
#include "mm/page.h"

typedef enum {
    CREATE = 0, // 创建中
    RUNNING = 1, // 运行中
    WAIT = 2, // 线程阻塞
    DEATH = 3, // 死亡(无法被调度, 线程状态为等待处死)
    START = 4, // 准备调度
    FUTEX = 5, // 被挂起(无法被调度, 线程状态为等待唤醒)
    OUT = 6, // 已被处死(无法被调度)
} TaskStatus;

typedef struct {
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

struct process_control_block {
    size_t pid;                 // 进程ID
    uint64_t ppid;              // 父进程ID
    char name[32];              // 进程名称
    TaskStatus state;           // 进程状态
    page_directory_t *pagedir;  // 页表目录
    TaskContext context;        // 进程上下文
    size_t cpuid;               // 运行的CPU ID
    uint64_t kernel_stack;      // 内核栈地址
    size_t group_index;         // 进程组索引
};

typedef struct process_control_block *pcb_t;

size_t add_task(pcb_t task);
void process_setup();
