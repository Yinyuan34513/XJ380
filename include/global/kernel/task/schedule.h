#ifndef SCHEDULE_H
#define SCHEDULE_H

#include "task.h"

struct schedule
{
    spinlock_t lock;              // 每个CPU的调度锁
    long running_task_count;      // 运行中的任务数
    long CPU_exec_task_jiffies;   // CPU执行任务的时间片
    struct task_struct task_queue;// 任务队列
};

extern struct schedule task_schedule[MAX_CPU_NUM];

void schedule();
void init_schedule();
struct task_struct *get_next_task(int cpu_id);
void insert_task_queue(struct task_struct *tsk, int cpu_id);

#endif