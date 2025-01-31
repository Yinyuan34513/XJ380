#include "../../include/include.h"

struct schedule task_schedule[MAX_CPU_NUM];

struct task_struct *get_next_task()
{
    struct task_struct *tsk = NULL;

    if (list_is_empty(&task_schedule[current_task->cpu_id].task_queue.list))
    {
        return init_task[current_task->cpu_id];
    }

    tsk = container_of(list_next(&task_schedule[current_task->cpu_id].task_queue.list), struct task_struct, list);
    list_del(&tsk->list);

    task_schedule[current_task->cpu_id].running_task_count -= 1;

    return tsk;
}

void insert_task_queue(struct task_struct *tsk)
{
    struct task_struct *tmp = NULL;

    if (tsk == init_task[current_task->cpu_id])
        return;

    tmp = container_of(list_next(&task_schedule[current_task->cpu_id].task_queue.list), struct task_struct, list);

    if (list_is_empty(&task_schedule[current_task->cpu_id].task_queue.list))
    {
    }
    else
    {
        while (tmp->vrun_time < tsk->vrun_time)
            tmp = container_of(list_next(&tmp->list), struct task_struct, list);
    }

    list_add_to_before(&tmp->list, &tsk->list);
    task_schedule[current_task->cpu_id].running_task_count += 1;
}

void schedule()
{
    struct task_struct *tsk = NULL;
    long cpu_id = current_task->cpu_id;

    current_task->flags &= ~NEED_SCHEDULE;
    tsk = get_next_task();

    if (current_task->vrun_time >= tsk->vrun_time)
    {
        if (current_task->state == TASK_RUNNING)
            insert_task_queue(current_task);

        if (!task_schedule[cpu_id].CPU_exec_task_jiffies)
            switch (tsk->priority)
            {
            case 0:
            case 1:
                task_schedule[cpu_id].CPU_exec_task_jiffies = 4 / task_schedule[cpu_id].running_task_count;
                break;
            case 2:
            default:
                task_schedule[cpu_id].CPU_exec_task_jiffies = 4 / task_schedule[cpu_id].running_task_count * 3;
                break;
            }

        switch_to(current_task, tsk);
    }
    else
    {
        insert_task_queue(tsk);

        if (!task_schedule[cpu_id].CPU_exec_task_jiffies)
            switch (tsk->priority)
            {
            case 0:
            case 1:
                task_schedule[cpu_id].CPU_exec_task_jiffies = 4 / task_schedule[cpu_id].running_task_count;
                break;
            case 2:
            default:
                task_schedule[cpu_id].CPU_exec_task_jiffies = 4 / task_schedule[cpu_id].running_task_count * 3;
                break;
            }
    }
}

void schedule_init()
{
    int i = 0;
    xmemset(&task_schedule, 0, sizeof(struct schedule) * MAX_CPU_NUM);

    for (i = 0; i < MAX_CPU_NUM; i++)
    {
        list_init(&task_schedule[i].task_queue.list);
        task_schedule[i].task_queue.vrun_time = 0x7fffffffffffffff;

        task_schedule[i].running_task_count = 1;
        task_schedule[i].CPU_exec_task_jiffies = 4;
    }
}