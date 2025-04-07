#include "../../include/include.h"

spinlock_t get_next_task_lock;
struct schedule task_schedule[MAX_CPU_NUM];

struct task_struct *get_next_task(int cpu_id)
{
    spin_lock(&get_next_task_lock);
    struct task_struct *tsk = NULL;

    if (list_is_empty(&task_schedule[cpu_id].task_queue.list))
    {
        spin_unlock(&get_next_task_lock);
        return init_task[cpu_id];
    }

    tsk = container_of(list_next(&task_schedule[cpu_id].task_queue.list), struct task_struct, list);
    list_del(&tsk->list);
    task_schedule[cpu_id].running_task_count -= 1;

    spin_unlock(&get_next_task_lock);
    return tsk;
}

void insert_task_queue(struct task_struct *tsk, int cpu_id)
{
    if (tsk == init_task[cpu_id])
        return;

    struct task_struct *tmp = container_of(list_next(&task_schedule[cpu_id].task_queue.list),
                                           struct task_struct, list);

    if (!list_is_empty(&task_schedule[cpu_id].task_queue.list))
    {
        while (tmp->vrun_time < tsk->vrun_time)
            tmp = container_of(list_next(&tmp->list), struct task_struct, list);
    }

    list_add_to_before(&tmp->list, &tsk->list);
    task_schedule[cpu_id].running_task_count += 1;
}

void schedule()
{
    disable_intr();

    uint64_t lapicid = lapic_id();
    uint64_t cpu_id = get_cpuid_by_lapicid(lapicid);
    spinlock_t *lock = &task_schedule[cpu_id].lock;

    spin_lock(lock);

    struct task_struct *tsk = NULL;
    current_task->flags &= ~NEED_SCHEDULE;
    tsk = get_next_task(cpu_id);

    if (tsk == &init_task_union.task)
    {
        goto finish;
    }

    if (current_task->vrun_time >= tsk->vrun_time)
    {
        if (current_task->state == TASK_RUNNING)
            insert_task_queue(current_task, cpu_id);

        if (!task_schedule[cpu_id].CPU_exec_task_jiffies)
        {
            switch (tsk->priority)
            {
            case 0:
            case 1:
                task_schedule[cpu_id].CPU_exec_task_jiffies =
                    4 / task_schedule[cpu_id].running_task_count;
                break;
            case 2:
            default:
                task_schedule[cpu_id].CPU_exec_task_jiffies =
                    4 / task_schedule[cpu_id].running_task_count * 3;
                break;
            }
        }

        switch_mm(current_task, tsk);
        spin_unlock(lock);
        switch_to(current_task, tsk);
    }
    else
    {
        insert_task_queue(tsk, cpu_id);

        if (!task_schedule[cpu_id].CPU_exec_task_jiffies)
        {
            switch (current_task->priority)
            {
            case 0:
            case 1:
                task_schedule[cpu_id].CPU_exec_task_jiffies =
                    4 / task_schedule[cpu_id].running_task_count;
                break;
            case 2:
            default:
                task_schedule[cpu_id].CPU_exec_task_jiffies =
                    4 / task_schedule[cpu_id].running_task_count * 3;
                break;
            }
        }
    }

finish:
    spin_unlock(lock);
    enable_intr();
}

void schedule_init()
{
    spin_init(&get_next_task_lock);

    xmemset(&task_schedule, 0, sizeof(struct schedule) * MAX_CPU_NUM);

    for (int i = 0; i < MAX_CPU_NUM; i++)
    {
        spin_init(&task_schedule[i].lock);
        list_init(&task_schedule[i].task_queue.list);
        task_schedule[i].task_queue.vrun_time = 0x7fffffffffffffff;
        task_schedule[i].running_task_count = 1;
        task_schedule[i].CPU_exec_task_jiffies = 4;
    }
}