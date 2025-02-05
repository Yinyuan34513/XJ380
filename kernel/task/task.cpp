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

struct tss_struct init_tss[MAX_CPU_NUM];

uint64_t idle_entry(uint64_t arg)
{
    while (1)
    {
        __asm__ __volatile__("sti\n\thlt");
    }
}

VNode *open_exec_file(const char *name)
{
    return path_walk(nullptr, name);
}

uint64_t fail_parse_elf(void *buf, int retval)
{
    free(buf);
    return (uint64_t)retval;
}

uint64_t parse_elf_file(struct X64_REGS *regs, VNode *file_node)
{
    int retval = 0;
    KERNEL_MAPPER mapper(PAGE_TABLE_BASE_ADDR);

    void *buf = malloc(PAGE_SIZE);
    xmemset(buf, 0, PAGE_SIZE);
    uint64_t pos = 0;
    pos = file_node->ops->lseek(file_node, 0, SEEK_SET);
    retval = file_node->ops->read(file_node, (uint8_t *)buf, pos, file_node->file_size);
    retval = 0;

    Elf32_Ehdr *ptr = (Elf32_Ehdr *)buf;
    if (!(ptr->e_ident[EI_MAG0] == ELFMAG0 && ptr->e_ident[EI_MAG1] == ELFMAG1 && ptr->e_ident[EI_MAG2] == ELFMAG2 && ptr->e_ident[EI_MAG3] == ELFMAG3))
    {
        write_serial_string("Not an ELF file: %s");
        retval = -1;
        return fail_parse_elf(buf, retval);
    }

    // 暂时只支持64位的文件
    if (ptr->e_ident[EI_CLASS] != ELFCLASS64)
    {
        write_serial_string("ptr->e_ident[EI_CLASS]=");
        write_serial_dec(ptr->e_ident[EI_CLASS]);
        retval = -1;
        return fail_parse_elf(buf, retval);
    }
    Elf64_Ehdr ehdr = *(Elf64_Ehdr *)buf;
    // 暂时只支持AMD64架构
    if (ehdr.e_machine != EM_AMD64)
    {
        ("e_machine=%d", ehdr.e_machine);
        retval = -1;
        return fail_parse_elf(buf, retval);
    }

    if (ehdr.e_type != ET_EXEC)
    {
        retval = -1;
        return fail_parse_elf(buf, retval);
    }

    regs->rip = ehdr.e_entry;

    Elf64_Phdr *phdr = (Elf64_Phdr *)((uint64_t)buf + ehdr.e_phoff);

    for (int i = 0; i < ehdr.e_phnum; i++)
    {
        if (phdr[i].p_type != PT_LOAD)
            continue;

        mapper.map_range(phdr[i].p_vaddr, 0, phdr[i].p_filesz, 0x7);
        flush_tlb();

        uint64_t segm_in_file = (uint64_t)buf + phdr[i].p_offset;
        xmemcpy((VOID *)phdr[i].p_vaddr, (VOID *)segm_in_file, phdr[i].p_filesz);
        uint64_t remain_bytes = phdr[i].p_memsz - phdr[i].p_filesz;
        xmemset((VOID *)(phdr[i].p_vaddr + phdr[i].p_filesz), 0, remain_bytes);
    }

    free(buf);

    uint64_t stack_start_addr = current_task->mm->start_stack - STACK_SIZE;
    mapper.map_range(stack_start_addr, 0, STACK_SIZE, 0x7);
    flush_tlb();

    xmemset((void *)stack_start_addr, 0, STACK_SIZE);

    return retval;
}

extern "C" uint64_t do_execve(struct X64_REGS *regs, const char *name, char *argv[], char *envp[])
{
    uint64_t stack_start_addr = 0x6ffff0a00000UL;
    const uint64_t brk_start_addr = 0x700000000000UL;
    uint64_t retval = 0;
    uint64_t pos = 0;
    VNode *file_node;

    switch_mm(nullptr, current_task);

    file_node = open_exec_file(name);
    if (!file_node)
    {
        write_serial_string("Cannot open file: ");
        write_serial_string(name);
        write_serial_string("\n");
        return 1;
    }

    if (!(current_task->flags & PF_KTHREAD))
        current_task->addr_limit = 0x00007FFFFFFFFFFF;

    current_task->mm->end_code = 0;
    current_task->mm->start_data = 0;
    current_task->mm->end_data = 0;
    current_task->mm->start_rodata = 0;
    current_task->mm->end_rodata = 0;
    current_task->mm->start_brk = brk_start_addr;
    current_task->mm->end_brk = brk_start_addr;
    current_task->mm->start_stack = stack_start_addr;

    current_task->flags &= ~PF_KTHREAD;

    if (argv != NULL)
    {
        int len = 0;
        int i = 0;
        char **dargv = (char **)(stack_start_addr - 10 * sizeof(char *));
        pos = (uint64_t)dargv;

        for (i = 0; i < 10 && argv[i] != NULL; i++)
        {
            len = strlen(argv[i]) + 1;
            strcpy((char *)(pos - len), argv[i]);
            dargv[i] = (char *)(pos - len);
            pos -= len;
        }
        stack_start_addr = pos - 10;
        regs->rdi = i;               // argc
        regs->rsi = (uint64_t)dargv; // argv
    }

    __asm__ __volatile__("movq	%0, %%gs\n\t"
                         "movq	%0, %%fs\n\t" ::"r"(0UL));

    retval = parse_elf_file(regs, file_node);

    regs->cs = SELECTOR_USER_CS | 0x3;
    regs->ds = SELECTOR_USER_DS | 0x3;
    regs->es = SELECTOR_USER_DS | 0x3;
    regs->ss = SELECTOR_USER_DS | 0x3;

    regs->rsp = current_task->mm->start_stack;
    regs->rbp = current_task->mm->start_stack;

    regs->rflags = 0x200246;

    regs->rax = current_task->mm->start_stack - STACK_SIZE;

    return retval;
}

extern const FrameBufferConfig *fbc_addr;
extern EFI_SYSTEM_TABLE *ST;
extern BOOT_CONFIG *BC;

bool keyboard_mouse_init_done = false;

uint64_t init_kernel_thread(uint64_t arg)
{
    write_serial_string("init task is running,arg:");
    write_serial_dec(arg);
    write_serial_string(".\n");

    disable_intr();

    keyboard_init();
    write_serial_string("Keyboard Initialize Success.\n");
    Mouse_init();
    write_serial_string("Mouse Initialize Success.\n");
    keyboard_mouse_init_done = true;

    init_device();
    init_pci(convert_physical_to_virtual((uint64_t)BC->MCFG));
    write_serial_string("PCI initialized.\n");

    init_ahci();
    write_serial_string("AHCI initialized.\n");

    init_nvme();
    write_serial_string("NVME initialized.\n");

    init_partition();
    write_serial_string("PARTITION initialized.\n");

    init_fat();
    init_vfs();

    write_serial_string("FS initialized.\n");

    write_serial_string("Operating System Initialize Success.\n");

    disable_intr();

    current_task->thread->rip = (uint64_t)ret_from_intr;
    current_task->thread->rsp = (uint64_t)current_task + STACK_SIZE - sizeof(struct X64_REGS);
    current_task->thread->gs = SELECTOR_USER_DS | 0x3;
    current_task->thread->fs = SELECTOR_USER_DS | 0x3;
    current_task->flags &= ~PF_KTHREAD;

    __asm__ __volatile__("movq	%1,	%%rsp	\n\t"
                         "pushq	%2		\n\t"
                         "jmp	do_execve	\n\t"
                         :
                         : "D"(current_task->thread->rsp), "m"(current_task->thread->rsp), "m"(current_task->thread->rip), "S"("/usr/init.elf"), "d"(nullptr), "c"(nullptr)
                         : "memory");
}

extern spinlock_t fb_print_lock;

uint64_t desktop_flusher(uint64_t arg)
{
    write_serial_string("desktop flusher is running,arg:");
    write_serial_dec(arg);
    write_serial_string(".\n");

    spin_init(&fb_print_lock);

    UINT16 desktop_sheet_id = 1145; // 如果SheetTest那行输出的是1145的话那么绝对有问题
    UINT16 dock_sheet_id = 1145;
    UINT16 mouse_sheet_id = 1145;
    SHEET_INFO shtinf;
    init_sheet(*fbc_addr, &shtinf);

    create_sheet(&shtinf, 0, 0, fbc_addr->horizontal_resolution, fbc_addr->vertical_resolution, FixedSheetType, 0, &desktop_sheet_id);
    create_sheet(&shtinf, 0, 0, fbc_addr->horizontal_resolution, fbc_addr->vertical_resolution, FixedSheetType, 254, &dock_sheet_id);
    create_sheet(&shtinf, (get_hor() - 16) / 2, (get_ver() - 28 - 29) / 2, 10, 21, FixedSheetType, 255, &mouse_sheet_id);

    init_desktop(&shtinf, &desktop_sheet_id, &dock_sheet_id);

    write_serial_string("desktop initialized.\n");

    int fontx = 8;
    int fonty = 50;

    // DEBUG
#if BUILD_EDITION == DEBUG_VERSION
    PrintString(&shtinf, &desktop_sheet_id, fbc_addr->horizontal_resolution - 64, fbc_addr->vertical_resolution - 40, "X:", WHITE);
    PrintString(&shtinf, &desktop_sheet_id, fbc_addr->horizontal_resolution - 64, fbc_addr->vertical_resolution - 24, "Y:", WHITE);
    PrintDec(&shtinf, &desktop_sheet_id, fbc_addr->horizontal_resolution - 48, fbc_addr->vertical_resolution - 40, shtinf.scrx, WHITE);
    PrintDec(&shtinf, &desktop_sheet_id, fbc_addr->horizontal_resolution - 48, fbc_addr->vertical_resolution - 24, shtinf.scry, WHITE);
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

    draw_rect(&shtinf, &desktop_sheet_id, 440, fbc_addr->vertical_resolution - 70, 480, fbc_addr->vertical_resolution - 20, {0x00, 0xa2, 0xe8, 0xff});
#endif
    // wsod_debug();

    draw_mouse(&shtinf, &mouse_sheet_id);

    refresh_sheet(&shtinf); // 刷新

    while (!keyboard_mouse_init_done)
    {
        enable_intr();
        __asm__ __volatile__("pause");
    }

    EFI_TIME time;
    EFI_TIME_CAPABILITIES time_capab;

    UINT8 k_input;
    int tby;
    int tbx;

    // struct EFI_RUNTIME_SERVICES *RuntimeServices = (struct EFI_RUNTIME_SERVICES *)convert_physical_to_virtual((UINT64)(ST->RuntimeServices));
    // EFIAPI EFI_STATUS (*GetTime)(EFI_TIME *, EFI_TIME_CAPABILITIES *) = (EFIAPI EFI_STATUS(*)(EFI_TIME *, EFI_TIME_CAPABILITIES *))convert_physical_to_virtual((UINT64)RuntimeServices->GetTime);

    while (1)
    {
        enable_intr();

        tbx = getBX(&shtinf, &mouse_sheet_id);
        tby = getBY(&shtinf, &mouse_sheet_id);
        int mouse_x = get_mouse_x();
        int mouse_y = get_mouse_y();
        move_mouse(&shtinf, &mouse_sheet_id, mouse_x, mouse_y);
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

        // EFI_STATUS status = GetTime(&time, &time_capab);
        // if (status != EFI_SUCCESS)
        // {
        //     write_serial_string("GetTime failed.\n");
        // }
        int scdx = fbc_addr->horizontal_resolution * 0.8125;
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

    p = (void *)convert_physical_to_virtual((uint64_t)PAGE_ALLOC.alloc_pages(STACK_SIZE / PAGE_SIZE));
    if (!p)
    {
        spin_unlock(&do_fork_lock);
        return 0;
    }
    xmemset(p, 0, STACK_SIZE);
    tsk = (struct task_struct *)p;
    xmemcpy(tsk, current_task, sizeof(struct task_struct));

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
    xmemset(thd, 0, sizeof(struct thread_struct));
    tsk->thread = thd;

    thd->rsp0 = (uint64_t)tsk + STACK_SIZE;
    thd->rip = regs->rip;
    thd->rsp = (uint64_t)tsk + STACK_SIZE - sizeof(struct X64_REGS);
    xmemcpy((void *)thd->rsp, regs, sizeof(struct X64_REGS));

    if (!(tsk->flags & PF_KTHREAD))
    {
        thd->fs = SELECTOR_USER_DS;
        thd->gs = SELECTOR_USER_DS;

        thd->rip = regs->rip = (uint64_t)ret_from_intr;
    }
    else
    {
        thd->fs = SELECTOR_KERNEL_DS;
        thd->gs = SELECTOR_KERNEL_DS;
    }

    tsk->state = TASK_RUNNING;
    insert_task_queue(tsk);

    spin_unlock(&do_fork_lock);

    return tsk->pid;
}

extern "C" uint64_t do_exit(uint64_t code)
{
    enable_intr();
    while (1)
        __asm__ __volatile__("pause");
}

int kernel_thread(uint64_t (*fn)(uint64_t), uint64_t arg, uint64_t flags)
{
    struct X64_REGS regs;
    xmemset(&regs, 0, sizeof(struct X64_REGS));

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
    init_tss[current_task->cpu_id].rsp0 = next->thread->rsp0;

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

    init_mm.start_code = 0x0;
    init_mm.end_code = 0x0;
    init_mm.start_data = 0x0;
    init_mm.end_data = 0x0;
    init_mm.start_rodata = 0x0;
    init_mm.end_rodata = 0x0;
    init_mm.start_brk = 0x0;
    init_mm.end_brk = 0x0;
    init_mm.start_stack = 0x0;

    init_tss[current_task->cpu_id].rsp0 = init_thread.rsp0;

    list_init(&init_task_union.task.list);

    init_task_union.task.preempt_count = 0;
    init_task_union.task.state = TASK_RUNNING;
    init_task_union.task.cpu_id = 0;

    kernel_thread(init_kernel_thread, 100, CLONE_FS | CLONE_FILES | CLONE_SIGNAL);
    kernel_thread(desktop_flusher, 0, CLONE_FS | CLONE_FILES | CLONE_SIGNAL);

    TASK_INITIALIZED = true;
}
