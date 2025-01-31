#ifndef SCHEDULE_H
#define SCHEDULE_H

#include "task.h"

struct schedule
{
    long running_task_count;
    long CPU_exec_task_jiffies;
    struct task_struct task_queue;
};

extern struct schedule task_schedule[MAX_CPU_NUM];

void schedule();
void schedule_init();
struct task_struct *get_next_task();
void insert_task_queue(struct task_struct *tsk);

#endif