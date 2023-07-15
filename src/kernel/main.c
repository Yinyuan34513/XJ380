#include "include.h"

u32 buf_win[160 * 68];

void TestA()
{
    int i = 0;
    while (1) {
        if (fifo_status(&decoded_key) > 0) {
            printf("%c", fifo_get(&decoded_key));
            sheet_refresh(&shtctl.sheets0[0], 0, 0, binfo->scrnx, 114);
        }
    }
}

void TestB()
{
    int i = 0x1000;
    while (1) {
        put_str("B");
        put_int(i++);
        put_str(".");
        milli_delay(200);
    }
}

void TestC()
{
    int i = 0x2000;
    while (1) {
        put_str("C");
        put_int(i++);
        put_str(".");
        milli_delay(200);
    }
}

PUBLIC int kernel_main()
{
    shtctl_init(&shtctl);
    SHEET *sht_back = sheet_alloc(&shtctl);
    SHEET *sht_win  = sheet_alloc(&shtctl);
    sheet_setbuf(sht_back, buf_back, binfo->scrnx, binfo->scrny, -1);
    sheet_setbuf(sht_win, buf_win, 160, 68, 99);
    init_screen();
    make_window(buf_win, 160, 68, "Window", 99);
    putstr_atbuf(buf_win, 160, 53, 25, 0, "XJ380OS");
    putstr_atbuf(buf_win, 160, 52, 24, 0xffffff, "XJ380OS");
    putstr_atbuf(buf_win, 160, 13, 45, 0, "Codename \"Beluga\"");
    putstr_atbuf(buf_win, 160, 12, 44, 0xffffff, "Codename \"Beluga\"");

    sheet_slide(sht_back, 0, 0);
    sheet_updown(sht_back, 0);
    sheet_slide(sht_win, 80, 72);
    sheet_updown(sht_win, 1);

    sheet_refresh(sht_back, 0, 0, binfo->scrnx, 48);

    TASK    *p_task       = task_table;
    PROCESS *p_proc       = proc_table;
    char    *p_task_stack = task_stack + STACK_SIZE_TOTAL;
    u16      selector_ldt = SELECTOR_LDT_FIRST;
    int i;
    for (i = 0; i < NR_TASKS; i++) {
        strcpy(p_proc->p_name, p_task->name);
        p_proc->pid = i;

        p_proc->ldt_sel = selector_ldt;
        
        memcpy(&p_proc->ldts[0], &gdt[SELECTOR_KERNEL_CS >> 3], sizeof(DESCRIPTOR));
        p_proc->ldts[0].attr1 = DA_C | PRIVILEGE_USER << 5;
        memcpy(&p_proc->ldts[1], &gdt[SELECTOR_KERNEL_DS >> 3], sizeof(DESCRIPTOR));
        p_proc->ldts[1].attr1 = DA_DRW | PRIVILEGE_USER << 5;

        p_proc->regs.cs = ((0 * 8) & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | RPL_USER;
        p_proc->regs.ds = ((1 * 8) & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | RPL_USER;
        p_proc->regs.es = ((1 * 8) & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | RPL_USER;
        p_proc->regs.fs = ((1 * 8) & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | RPL_USER;
        p_proc->regs.ss = ((1 * 8) & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | RPL_USER;
        p_proc->regs.gs = ((1 * 8) & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | RPL_USER;

        p_proc->regs.eip = (u32) p_task->initial_eip;
        p_proc->regs.esp = (u32) p_task_stack;
        p_proc->regs.eflags = 0x1202;

        p_task_stack -= p_task->stacksize;
        p_proc++;
        p_task++;
        selector_ldt += 1 << 3;
    }

    proc_table[0].ticks = proc_table[0].priority = 150;
    proc_table[1].ticks = proc_table[1].priority = 0;
    proc_table[2].ticks = proc_table[2].priority = 0;

    init_clock();
    init_keyboard();
    init_mouse();

    k_reenter = 0;
    ticks = 0;

    p_proc_ready = proc_table;
    restart();

    while (1);
}