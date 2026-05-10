#include "cpu/regio.h"
#include "krlibc.h"
#include "mm/heap.h"
#include "mm/page.h"
#include "proto.hpp"
#include "task/pcb.h"
#include "task/scheduler.h"
#include <cpu/longm.h>
#include <errno.h>
#include <fs/vfs/vfs.h>

extern bool no_interrupt; // 涓簍rue鏃朵唬琛ㄤ弗绂佸紑鍚腑鏂?

static pcb_t find_reapable_child(pcb_t parent)
{
    if (parent == NULL || parent->child_pcb == NULL) return NULL;

    pcb_t target = NULL;
    spin_lock(&parent->child_pcb->lock);
    for (lock_node *node = parent->child_pcb->head; node != NULL; node = node->next)
    {
        pcb_t child = (pcb_t)node->data;
        if (child != NULL && child->status == DEATH)
        {
            target = child;
            break;
        }
    }
    spin_unlock(&parent->child_pcb->lock);
    return target;
}

void reaper_thread()
{
    while (true)
    {
        if (!no_interrupt)
        {
            open_interrupt;
            enable_scheduler();
        }
        pcb_t target = find_reapable_child(kernel_group);
        if (target != NULL) kill_proc(target, 0, false);
        scheduler_yield();
    }
}

void init_reaper()
{
    create_kernel_thread((void *)reaper_thread, NULL, "Process Reaper", NULL);
}
