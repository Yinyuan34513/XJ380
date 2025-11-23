#include "task/pcb.h"
#include "cpu/regio.h"
#include "krlibc.h"
#include "mm/heap.h"
#include "mm/page.h"
#include "proto.hpp"
#include <cpu/longm.h>
#include <errno.h>
#include <fs/vfs/vfs.h>

pcb_t   wait_kill_queue[10];
uint8_t wait_kill_count = 0;

extern bool no_interrupt; // 为true时代表严禁开启中断

void killer_thread()
{
    while (true)
    {
        if (!no_interrupt)
        {
            open_interrupt;
            enable_scheduler();
        }
        if (wait_kill_count)
        {
            for (int i = 0; i < 10; i++)
            {
                if (wait_kill_queue[i] != NULL)
                {
                    recycle_process(wait_kill_queue[i]);
                    wait_kill_queue[i] = NULL;
                    wait_kill_count--;
                }
            }
        }
        __asm__ volatile("pause\n\t");
    }
}

void addque_killer(pcb_t group)
{
    for (int i = 0; i < 10; i++)
    {
        if (wait_kill_queue[i] == NULL)
        {
            wait_kill_queue[i] = group;
            wait_kill_count++;
            return;
        }
    }
}

void init_killer()
{
    create_kernel_thread((void *)killer_thread, NULL, "Process Killer", NULL);
    memset(wait_kill_queue, 0, sizeof(pcb_t) * 10);
}