#include "task/pcb.h"
#include "mm/heap.h"
#include "mm/page.h"
#include "krlibc.h"
#include "proto.h"

static size_t now_pid = 0;
lock_queue *pcb_group_queue = NULL;



void process_setup(){
    pcb_group_queue = queue_init();
    pcb_t kernel_pcb = (pcb_t)malloc(KERNEL_STACK_SIZE);
    kernel_pcb->pid = now_pid++;
    kernel_pcb->pagedir = get_kernel_pagedir();
    strcpy(kernel_pcb->name,"XSK System");
    add_task(kernel_pcb);
}
