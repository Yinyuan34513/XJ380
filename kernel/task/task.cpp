#include "../../include/include.h"
#include "../alloc/page.h"

struct mm_struct init_mm = {0};

struct thread_struct init_thread =
    {
        .rsp0 = (uint64_t)(init_task_union.stack + STACK_SIZE / sizeof(uint64_t)),
        .rsp = (uint64_t)(init_task_union.stack + STACK_SIZE / sizeof(uint64_t)),
        .fs = SELECTOR_KERNEL_DS,
        .gs = SELECTOR_KERNEL_DS,
        .cr2 = 0,
        .trap_nr = 0,
        .error_code = 0};

union task_union init_task_union __attribute__((__section__(".data.init_task"))) = {INIT_TASK(init_task_union.task)};

struct task_struct *init_task[MAX_CPU_NUM] = {&init_task_union.task, 0};

struct tss_struct init_tss[MAX_CPU_NUM] = {INIT_TSS};

uint64_t idle_entry(uint64_t arg)
{
    while (1)
    {
        __asm__ __volatile__("sti\n\thlt\n\t");
    }
}

uint64_t init_kernel_thread(uint64_t arg)
{
    write_serial_string("init task is running,arg:");
    write_serial_dec(arg);
    write_serial_string(".\n");

    init_vdisk();
    init_pci();
    write_serial_string("PCI initialized.\n");

    init_ahci();
    write_serial_string("AHCI initialized.\n");

    while (1)
    {
        __asm__ __volatile__("sti\n\thlt\n\t");
    }
}

extern const FrameBufferConfig *fbc_addr;
extern EFI_SYSTEM_TABLE *ST;
extern BOOT_CONFIG *BC;

uint64_t desktop_flusher(uint64_t arg)
{
    write_serial_string("desktop flusher is running,arg:");
    write_serial_dec(arg);
    write_serial_string(".\n");

    EFI_SYSTEM_TABLE *SystemTable = ST;

    UINT16 desktop_sheet_id = 1145; // 如果SheetTest那行输出的是1145的话那么绝对有问题
    UINT16 dock_sheet_id = 1145;
    UINT16 mouse_sheet_id = 1145;
    SHEET_INFO shtinf;
    init_sheet(*fbc_addr, &shtinf);

    create_sheet(&shtinf, 0, 0, (*fbc_addr).horizontal_resolution, (*fbc_addr).vertical_resolution, FixedSheetType, 0, &desktop_sheet_id);
    create_sheet(&shtinf, 0, 0, (*fbc_addr).horizontal_resolution, (*fbc_addr).vertical_resolution, FixedSheetType, 254, &dock_sheet_id);
    create_sheet(&shtinf, (get_hor() - 16) / 2, (get_ver() - 28 - 29) / 2, 10, 21, FixedSheetType, 255, &mouse_sheet_id);

    init_desktop(&shtinf, &desktop_sheet_id, &dock_sheet_id);

    int fontx = 8;
    int fonty = 50;

    // DEBUG
#if BUILD_EDITION == DEBUG_VERSION
    PrintString(&shtinf, &desktop_sheet_id, (*fbc_addr).horizontal_resolution - 64, (*fbc_addr).vertical_resolution - 40, "X:", WHITE);
    PrintString(&shtinf, &desktop_sheet_id, (*fbc_addr).horizontal_resolution - 64, (*fbc_addr).vertical_resolution - 24, "Y:", WHITE);
    PrintDec(&shtinf, &desktop_sheet_id, (*fbc_addr).horizontal_resolution - 48, (*fbc_addr).vertical_resolution - 40, shtinf.scrx, WHITE);
    PrintDec(&shtinf, &desktop_sheet_id, (*fbc_addr).horizontal_resolution - 48, (*fbc_addr).vertical_resolution - 24, shtinf.scry, WHITE);
    PrintString(&shtinf, &desktop_sheet_id, 8, 34, "KeyboardInputOut", WHITE);
    // PrintString(&shtinf, &desktop_sheet_id, 332, 34, "PointerInputOut", WHITE);
    PrintString(&shtinf, &desktop_sheet_id, 8, 480, "MapKey:", WHITE);
    PrintHex(&shtinf, &desktop_sheet_id, 8, 496, BC->MemoryMap.MapKey, WHITE);
    // 别问为什么这么多-40，问就是因为改起来太麻烦
    PrintString(&shtinf, &desktop_sheet_id, 8, 284, "PrintTestColor", WHITE);
    draw_rect(&shtinf, &desktop_sheet_id, 480 - 472, 300, 500 - 472, 320, WHITE);
    draw_rect(&shtinf, &desktop_sheet_id, 500 - 472, 300, 520 - 472, 320, BLACK);
    draw_rect(&shtinf, &desktop_sheet_id, 520 - 472, 300, 540 - 472, 320, XINGJI_BLUE);
    draw_rect(&shtinf, &desktop_sheet_id, 540 - 472, 300, 560 - 472, 320, XINGJI_YELLOW);
    draw_rect(&shtinf, &desktop_sheet_id, 560 - 472, 300, 580 - 472, 320, WIN_BLUE);
    draw_rect(&shtinf, &desktop_sheet_id, 580 - 472, 300, 600 - 472, 320, GRAY);
    draw_rect(&shtinf, &desktop_sheet_id, 600 - 472, 300, 620 - 472, 320, BGRAY);
    draw_rect(&shtinf, &desktop_sheet_id, 620 - 472, 300, 640 - 472, 320, PURPLE);
    draw_rect(&shtinf, &desktop_sheet_id, 640 - 472, 300, 660 - 472, 320, PINK);
    draw_rect(&shtinf, &desktop_sheet_id, 660 - 472, 300, 680 - 472, 320, RED);
    draw_rect(&shtinf, &desktop_sheet_id, 680 - 472, 300, 700 - 472, 320, CHINA_RED);
    draw_rect(&shtinf, &desktop_sheet_id, 700 - 472, 300, 720 - 472, 320, GOLD);
    draw_rect(&shtinf, &desktop_sheet_id, 720 - 472, 300, 740 - 472, 320, GREEN);
    draw_rect(&shtinf, &desktop_sheet_id, 740 - 472, 300, 760 - 472, 320, LIGHT_GREEN);
    draw_rect(&shtinf, &desktop_sheet_id, 760 - 472, 300, 780 - 472, 320, BLUE);
    draw_rect(&shtinf, &desktop_sheet_id, 780 - 472, 300, 800 - 472, 320, SKY_BLUE);

    PrintString(&shtinf, &desktop_sheet_id, 8, 332, "MemoryMapTypeSheet", WHITE);
    PrintString(&shtinf, &desktop_sheet_id, 8, 348, "0x0: Free", WHITE);
    PrintString(&shtinf, &desktop_sheet_id, 8, 364, "0xB: EFI Code & Data", WHITE);

    PrintString(&shtinf, &desktop_sheet_id, 8, 400, "SheetManagerTest", WHITE);
    PrintDec(&shtinf, &desktop_sheet_id, 8, 416, (UINT64)desktop_sheet_id, WHITE);
    // PrintDec(&shtinf, &desktop_sheet_id, 164, 416, (UINT64)*shtinf.sheet[0].number, WHITE);

    PrintHex(&shtinf, &desktop_sheet_id, 8, 432, (UINT64)shtinf.temp_buffer, WHITE);
    PrintDec(&shtinf, &desktop_sheet_id, 164, 432, (UINT64)shtinf.sheet_num, WHITE);

    PrintHex(&shtinf, &desktop_sheet_id, 8, 448, 0xFEDCBA9876543210, WHITE);
    // PrintHex(&shtinf, &desktop_sheet_id, 164, 448, (UINT64)shtinf.sheet[255].buffer, WHITE);

    // CPU型号
    char CPU_Model[48];
    get_cpu_name(CPU_Model);
    PrintString(&shtinf, &desktop_sheet_id, 8, 528, "CPU Model:", WHITE);
    PrintString(&shtinf, &desktop_sheet_id, 96, 528, CPU_Model, WHITE);

    draw_rect(&shtinf, &desktop_sheet_id, 440, (*fbc_addr).vertical_resolution - 70, 480, (*fbc_addr).vertical_resolution - 20, {0x00, 0xa2, 0xe8, 0xff});
#endif
    // wsod_debug();

    draw_mouse(&shtinf, &mouse_sheet_id);

    refresh_sheet(&shtinf); // 刷新

    keyboard_init();
    write_serial_string("Keyboard Initialize Success.\n");
    Mouse_init();
    write_serial_string("Operating System Initialize Success.\n");

    EFI_TIME time;
    EFI_TIME_CAPABILITIES time_capab;

    UINT8 k_input;
    int tby;
    int tbx;

    while (1)
    {
        enable_intr();

        tbx = getBX(&shtinf, &mouse_sheet_id);
        tby = getBY(&shtinf, &mouse_sheet_id);
        move_mouse(&shtinf, &mouse_sheet_id, get_mouse_x(), get_mouse_y());
        refresh_part_sheet(&shtinf, tbx, tby,
                           tbx + getXsize(&shtinf, &mouse_sheet_id),
                           tby + getYsize(&shtinf, &mouse_sheet_id));
        refresh_part_sheet(&shtinf, getBX(&shtinf, &mouse_sheet_id), getBY(&shtinf, &mouse_sheet_id),
                           getBX(&shtinf, &mouse_sheet_id) + getXsize(&shtinf, &mouse_sheet_id),
                           getBY(&shtinf, &mouse_sheet_id) + getYsize(&shtinf, &mouse_sheet_id));
        k_input = get_keyboard_input();
        if (k_input != NULL && k_input != KEY_ENTER && k_input != KEY_SHIFT && k_input != KEY_CTRL && k_input != KEY_CAPS && k_input != KEY_BACKSPACE)
        {
            PrintFont(&shtinf, &desktop_sheet_id, fontx, fonty, k_input, WHITE);
            refresh_part_sheet(&shtinf, fontx, fonty, fontx + 8, fonty + 16); // 刷新
            fontx += 8;
        }
        else if (k_input == KEY_ENTER)
        {
            fontx = 8;
            fonty += 16;
        }
        if (fontx > 304)
        {
            fontx = 8;
            fonty += 16;
        }

        SystemTable->RuntimeServices->GetTime(&time, &time_capab);
        int scdx = (*fbc_addr).horizontal_resolution * 0.8125;
        draw_rect(&shtinf, &dock_sheet_id, scdx - 44, 0, scdx - 4, 24, DOCK_COL);
        PrintDec(&shtinf, &dock_sheet_id, scdx - 44, 4, (time.Hour + 8) % 24, BLACK);
        PrintString(&shtinf, &dock_sheet_id, scdx - 28, 4, ":", BLACK);
        PrintDec(&shtinf, &dock_sheet_id, scdx - 20, 4, time.Minute, BLACK);
        refresh_part_sheet(&shtinf, scdx - 44, 0, scdx, 24);
    }

    return 0xFFFFFFFFFFFFFFFF;
}

spinlock_t do_fork_lock;

uint64_t do_fork(struct X64_REGS *regs, uint64_t clone_flags, uint64_t stack_start, uint64_t stack_size)
{
    spin_lock(&do_fork_lock);
    struct task_struct *tsk = NULL;
    struct thread_struct *thd = NULL;
    void *p = (void *)NULL;

    p = PAGE_ALLOC.alloc_page();
    tsk = (struct task_struct *)p;

    xmemset(tsk, 0, sizeof(*tsk));
    *tsk = *current_task;

    list_init(&tsk->list);

    tsk->priority = 2;
    tsk->pid++;
    tsk->preempt_count = 0;
    tsk->cpu_id = current_task->cpu_id;
    tsk->state = TASK_UNINTERRUPTIBLE;
    tsk->next = init_task_union.task.next;
    init_task_union.task.next = tsk;
    tsk->parent = current_task;

    thd = (struct thread_struct *)(tsk + 1);
    xmemset(thd, 0, sizeof(*thd));
    tsk->thread = thd;

    thd->rsp0 = (uint64_t)tsk + STACK_SIZE;
    thd->rip = regs->rip;
    thd->rsp = (uint64_t)tsk + STACK_SIZE - sizeof(struct X64_REGS);
    xmemcpy((void *)thd->rsp, regs, sizeof(struct X64_REGS));
    thd->fs = SELECTOR_KERNEL_DS;
    thd->gs = SELECTOR_KERNEL_DS;

    if (!(tsk->flags & PF_KTHREAD))
        thd->rip = regs->rip = (uint64_t)ret_from_intr;

    tsk->state = TASK_RUNNING;
    insert_task_queue(tsk);

    spin_unlock(&do_fork_lock);

    return 1;
}

extern "C" uint64_t do_exit(uint64_t code)
{
    while (1)
        __asm__ __volatile__("pause");
}

int kernel_thread(uint64_t (*fn)(uint64_t), uint64_t arg, uint64_t flags)
{
    struct X64_REGS regs;
    xmemset(&regs, 0, sizeof(regs));

    regs.rbx = (uint64_t)fn;
    regs.rdx = (uint64_t)arg;

    regs.cs = SELECTOR_KERNEL_CS;
    regs.ds = SELECTOR_KERNEL_DS;
    regs.es = SELECTOR_KERNEL_DS;
    regs.ss = SELECTOR_KERNEL_DS;
    regs.rflags = (1 << 9);

    regs.rip = (uint64_t)kernel_thread_func;

    return do_fork(&regs, flags, 0, 0);
}

extern "C" void __switch_to(struct task_struct *prev, struct task_struct *next)
{
    init_tss[next->cpu_id].rsp0 = next->thread->rsp0;

    __asm__ __volatile__("movq %%fs, %0\n\t" : "=a"(prev->thread->fs));
    __asm__ __volatile__("movq %%gs, %0\n\t" : "=a"(prev->thread->gs));

    __asm__ __volatile__("movq %0, %%fs\n\t" ::"a"(next->thread->fs));
    __asm__ __volatile__("movq %0, %%gs\n\t" ::"a"(next->thread->gs));
}

bool TASK_INITIALIZED = false;

void init_task_manager()
{
    spin_init(&do_fork_lock);

    uint64_t cr3;
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3));
    init_mm.pgd = (uint64_t *)cr3;

    init_tss[0].rsp0 = init_thread.rsp0;

    list_init(&init_task_union.task.list);

    for (int i = 0; i < SMP_MANAGER.ap_count(); i++)
    {
        kernel_thread(idle_entry, 0, CLONE_FS | CLONE_FILES | CLONE_SIGNAL);
    }

    kernel_thread(init_kernel_thread, 10, CLONE_FS | CLONE_FILES | CLONE_SIGNAL);
    kernel_thread(desktop_flusher, 0, CLONE_FS | CLONE_FILES | CLONE_SIGNAL);

    init_task_union.task.preempt_count = 0;
    init_task_union.task.state = TASK_RUNNING;
    init_task_union.task.cpu_id = 0;

    TASK_INITIALIZED = true;
}
