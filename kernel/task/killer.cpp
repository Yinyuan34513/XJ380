#include "cpu/regio.h"
#include "krlibc.h"
#include "mm/heap.h"
#include "mm/page.h"
#include "proto.hpp"
#include "task/pcb.h"
#include <cpu/longm.h>
#include <errno.h>
#include <fs/vfs/vfs.h>

extern bool no_interrupt; // 为true时代表严禁开启中断

void check_group(pcb_t group)
{
    if (group == NULL) return;

    if (group->child_index != 0)
    {
        lock_node *front_p = group->child_pcb->head;
        for (int i = 0; i < group->child_index; i++)
        {
            check_group((pcb_t)front_p->data);
            front_p = front_p->next;
        }
    }

    lock_node *front_p    = group->thread_queue->head;
    bool       group_dead = true;
    for (int i = 0; i < group->thread_queue->size && front_p != NULL; i++)
    {
        tcb_t thread = (tcb_t)front_p->data;
        if (thread->status != OUT || thread->status != DEATH)
        {
            if (strcmp(thread->name, "Window Message Thread") != 0)
            {
                // 不是消息线程才算
                group_dead = false;
            }
        }
        front_p = front_p->next;
    }

    if (group_dead)
    {
        kill_proc(group, 0, false);
        kill_proc0(group);
    }
}

// void killer_thread()
// {
//     while (true)
//     {
//         if (!no_interrupt)
//         {
//             open_interrupt;
//             enable_scheduler();
//         }
//         check_group(kernel_group);
//         cpu_relax();
//     }
// }

void init_killer()
{
    // create_kernel_thread((void *)killer_thread, NULL, "Process Killer", NULL);
}