#include "../../include/include.h"

uint64_t sys_brk(uint64_t addr)
{
    unsigned long new_brk = addr & ~(PAGE_SIZE - 1);

    //	color_printk(GREEN,BLACK,"sys_brk\n");
    //	color_printk(RED,BLACK,"brk:%#018lx,new_brk:%#018lx,current->mm->end_brk:%#018lx\n",brk,new_brk,current->mm->end_brk);
    if (new_brk == 0)
        return current_task->mm->start_brk;
    if (new_brk < current_task->mm->end_brk) // release  brk space
        return 0;

    new_brk = mm_do_brk(current_task->mm->end_brk, new_brk - current_task->mm->end_brk); // expand brk space

    current_task->mm->end_brk = new_brk;
    return new_brk;
}
