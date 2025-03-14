#include "../../include/cinc/elf.h"
#include "../../include/include.h"
#include "../alloc/page.h"

static long global_pid = 0;

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

struct task_struct *init_task[MAX_CPU_NUM] = {&init_task_union.task, nullptr};

struct tss_struct init_tss[MAX_CPU_NUM];

struct task_struct *user_current[MAX_CPU_NUM] = {nullptr};

uint64_t idle_entry(uint64_t arg)
{
    while (1)
    {
        __asm__ __volatile__("sti\n\tpause");
    }
}

VNode *open_exec_file(const char *name)
{
    return path_walk(current_task->cwd_node, name);
}

uint64_t fail_parse_elf(void *buf, int retval)
{
    if (buf)
    {
        free(buf);
    }
    return (uint64_t)retval;
}

uint64_t parse_elf_file(struct task_struct *task, struct X64_REGS *regs, VNode *file_node)
{
    Elf64_Ehdr ehdr;
    Elf64_Phdr *phdrs = nullptr;
    KERNEL_MAPPER mapper((uint64_t *)convert_physical_to_virtual((uint64_t)task->mm->pgd));
    uint64_t pos = 0;
    int retval = 0;
    uint8_t *buf = nullptr;
    uint64_t file_size = file_node->file_size;

    // 读取整个文件
    buf = (uint8_t *)malloc(file_size);
    if (!buf)
    {
        return -1;
    }
    xmemset(buf, 0, file_size);

    pos = file_node->ops->lseek(file_node, 0, SEEK_SET);
    file_node->ops->read(file_node, (uint8_t *)buf, pos, file_size);

    // 解析ELF头
    ehdr = *(Elf64_Ehdr *)buf;

    // 验证ELF魔数 (0x7F,'E','L','F')
    if (*(uint32_t *)ehdr.e_ident != 0x464C457F)
    {
        free(buf);
        return -1;
    }

    // 设置程序入口点
    regs->rip = ehdr.e_entry;

    // 保存加载信息
    if (!task->thread->elf_mappings)
    {
        task->thread->elf_mappings = (struct elf_mapping_space *)malloc(sizeof(struct elf_mapping_space));
    }
    struct elf_mapping_space *mapping_space = task->thread->elf_mappings;
    CalcLoadAddressRange((Elf64_Ehdr *)buf, (UINT64 *)&mapping_space->load_start, (UINT64 *)&mapping_space->load_end);

    uint64_t load_phys_start = mapper.map_range(mapping_space->load_start, 0, mapping_space->load_end - mapping_space->load_start, 0x7);
    flush_tlb();
    if (load_phys_start != 0)
    {
        mapping_space->load_phys_start = load_phys_start;
    }

    xmemset((void *)mapping_space->load_start, 0, mapping_space->load_end - mapping_space->load_start);

    // 获取程序头表
    phdrs = (Elf64_Phdr *)(buf + ehdr.e_phoff);

    // 遍历并加载程序段
    for (int i = 0; i < ehdr.e_phnum; i++)
    {
        if (phdrs[i].p_type != PT_LOAD)
        {
            continue;
        }

        // 对齐到页大小
        uint64_t v_addr = phdrs[i].p_vaddr & ~(PAGE_SIZE - 1);
        uint64_t v_size = (phdrs[i].p_memsz + (phdrs[i].p_vaddr & (PAGE_SIZE - 1)) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

        // 更新内存映射信息
        if (phdrs[i].p_flags & 0x1)
        { // 可执行
            task->mm->start_code = min(task->mm->start_code, v_addr);
            task->mm->end_code = max(task->mm->end_code, v_addr + v_size);
        }
        else if (phdrs[i].p_flags & 0x2)
        { // 可写
            task->mm->start_data = min(task->mm->start_data, v_addr);
            task->mm->end_data = max(task->mm->end_data, v_addr + v_size);
        }
        else
        {
            task->mm->start_rodata = min(task->mm->start_rodata, v_addr);
            task->mm->end_rodata = max(task->mm->end_rodata, v_addr + v_size);
        }

        // 复制段内容
        uint64_t segm_in_file = (uint64_t)buf + phdrs[i].p_offset;

        xmemcpy((void *)phdrs[i].p_vaddr,
                (void *)segm_in_file,
                phdrs[i].p_filesz);

        // 清零未初始化数据
        if (phdrs[i].p_memsz > phdrs[i].p_filesz)
        {
            xmemset((void *)(phdrs[i].p_vaddr + phdrs[i].p_filesz),
                    0,
                    phdrs[i].p_memsz - phdrs[i].p_filesz);
        }
    }

    free(buf);

    // 设置并映射用户栈
    uint64_t stack_start_addr = task->mm->start_stack - STACK_SIZE;
    mapper.map_range(stack_start_addr, 0, STACK_SIZE + PAGE_SIZE, 0x7); // 用户栈可读可写可执行
    flush_tlb();

    return 0;
}

extern "C" uint64_t do_execve(struct X64_REGS *regs, const char *name, char *argv[], char *envp[])
{
    uint64_t stack_start_addr = USER_STACK_START;
    const uint64_t brk_start_addr = 0x700000000000UL;
    uint64_t retval = 0;
    uint64_t pos = 0;
    VNode *file_node;

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
    current_task->mm->start_data = 0xFFFFFFFFFFFFFFFF;
    current_task->mm->end_data = 0;
    current_task->mm->start_rodata = 0xFFFFFFFFFFFFFFFF;
    current_task->mm->end_rodata = 0;
    current_task->mm->start_brk = brk_start_addr;
    current_task->mm->end_brk = brk_start_addr;
    current_task->mm->start_stack = stack_start_addr;

    retval = parse_elf_file(current_task, regs, file_node);
    if (retval < 0)
    {
        write_serial_string("Cannot parse elf file: ");
        write_serial_string(name);
        write_serial_string("\n");
        return -1;
    }

    if (argv != nullptr)
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
        regs->rdx = (uint64_t)envp;  // envp
    }
    else
    {
        regs->rdi = 0;
        regs->rsi = 0;
        regs->rdx = 0;
    }

    regs->rsp = current_task->mm->start_stack;
    regs->rbp = current_task->mm->start_stack;

    __asm__ __volatile__("movq	%0, %%gs\n\t"
                         "movq	%0, %%fs\n\t" ::"r"(0UL));

    regs->rflags = 0x200246;

    regs->cs = SELECTOR_USER_CS;
    regs->ss = SELECTOR_USER_DS;

    regs->ds = SELECTOR_USER_DS;
    regs->es = SELECTOR_USER_DS;

    regs->rax = 1;

    __asm__ __volatile__(
        "movq %0, %%rsp\n\t"
        "jmp ret_from_intr\n\t" ::"r"((uint64_t)regs));
}

extern const FrameBufferConfig *fbc_addr;
extern EFI_SYSTEM_TABLE *ST;
extern BOOT_CONFIG *BC;

bool keyboard_mouse_init_done = false;

uint64_t init_kernel_thread(uint64_t arg)
{
    disable_intr();

    write_serial_string("init task is running, arg:");
    write_serial_dec(arg);
    write_serial_string(".\n");

    keyboard_init();
    write_serial_string("Keyboard Initialize Success.\n");
    mouse_init();
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
    current_task->thread->rsp0 = (uint64_t)current_task + STACK_SIZE;
    current_task->thread->rsp = current_task->thread->rsp0 - sizeof(struct X64_REGS);
    current_task->thread->gs = SELECTOR_USER_DS;
    current_task->thread->fs = SELECTOR_USER_DS;
    current_task->flags &= ~PF_KTHREAD;

    __asm__ __volatile__(
        "movq	%1,	%%rsp	\n\t"
        "pushq	%2		\n\t"
        "jmp   do_execve	\n\t"
        :
        : "D"(current_task->thread->rsp), "m"(current_task->thread->rsp), "m"(current_task->thread->rip), "S"("/usr/init.elf"), "d"(nullptr), "c"(nullptr)
        : "memory");

    return (uint64_t)-1;
}

extern spinlock_t fb_print_lock;

uint64_t desktop_flusher(uint64_t arg)
{
    write_serial_string("desktop flusher is running, arg:");
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
    write_serial_string("CPU Model: ");
    write_serial_string(CPU_Model);
    write_serial_string("\n");

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

uint64_t alloc_cpu_id()
{
    static uint64_t cpu_id = 0;
    uint64_t ap_count = SMP_MANAGER.ap_count();

    uint64_t retval = cpu_id++;
    if (cpu_id >= ap_count)
    {
        cpu_id = 0;
    }

    return retval;
}

spinlock_t do_fork_lock;

uint64_t fork_copy_mm(struct task_struct *tsk, uint64_t clone_flags)
{
    if (clone_flags & CLONE_VM)
    {
        tsk->mm = current_task->mm;
        return 0;
    }

    struct mm_struct *new_mm = (struct mm_struct *)malloc(sizeof(struct mm_struct));
    if (!new_mm)
    {
        return (uint64_t)-ENOMEM;
    }

    uint64_t new_pgd = (uint64_t)PAGE_ALLOC.alloc_page();
    xmemset((void *)convert_physical_to_virtual(new_pgd), 0, PAGE_SIZE);
    uint64_t old_page_table;
    __asm__ __volatile__("movq %%cr3, %0" : "=r"(old_page_table));
    xmemcpy((uint8_t *)convert_physical_to_virtual(new_pgd), (uint8_t *)convert_physical_to_virtual(old_page_table), PAGE_SIZE);

    // // Map paddr to vaddr
    // uint64_t *pml4 = (uint64_t *)convert_physical_to_virtual(new_pgd);
    // uint64_t pml4_id = ((uint64_t)PAGE_TABLE_BASE_ADDR >> 39) & 0x1FF;
    // if (pml4[pml4_id] == 0)
    // {
    // uint64_t pml4_addr = (uint64_t)PAGE_ALLOC.alloc_page();
    // xmemset((void *)convert_physical_to_virtual(pml4_addr), 0, PAGE_SIZE);
    // pml4[pml4_id] = pml4_addr | 0x3;
    // }
    // uint64_t *pdpt = (uint64_t *)convert_physical_to_virtual(pml4[pml4_id] & ~(0xFFF));
    // uint64_t pdpt_id = ((uint64_t)PAGE_TABLE_BASE_ADDR >> 30) & 0x1FF;
    // if (pdpt[pdpt_id] == 0)
    // {
    // uint64_t pdpt_addr = (uint64_t)PAGE_ALLOC.alloc_page();
    // xmemset((void *)convert_physical_to_virtual(pdpt_addr), 0, PAGE_SIZE);
    // pdpt[pdpt_id] = pdpt_addr | 0x3;
    // }
    // uint64_t *pd = (uint64_t *)convert_physical_to_virtual(pdpt[pdpt_id] & ~(0xFFF));
    // uint64_t pd_id = ((uint64_t)PAGE_TABLE_BASE_ADDR >> 21) & 0x1FF;
    // if (pd[pd_id] == 0)
    // {
    // uint64_t pd_addr = (uint64_t)PAGE_ALLOC.alloc_page();
    // xmemset((void *)convert_physical_to_virtual(pd_addr), 0, PAGE_SIZE);
    // pd[pd_id] = pd_addr | 0x3;
    // }
    // uint64_t *pt = (uint64_t *)convert_physical_to_virtual(pd[pd_id] & ~(0xFFF));
    // uint64_t pt_id = ((uint64_t)PAGE_TABLE_BASE_ADDR >> 12) & 0x1FF;
    // pt[pt_id] = new_pgd | 0x3;

    new_mm->pgd = (uint64_t *)new_pgd;
    tsk->mm = new_mm;

    return 0;
}

uint64_t fork_exit_mm(struct task_struct *tsk)
{
    PAGE_ALLOC.free_page((void *)tsk->mm->pgd);

    if (tsk->mm)
    {
        free(tsk->mm);
    }

    // uint64_t *pml4 = (uint64_t *)convert_physical_to_virtual((uint64_t)current_task->mm->pgd);
    // uint64_t pml4_id = ((uint64_t)PAGE_TABLE_BASE_ADDR >> 39) & 0x1FF;
    // uint64_t *pdpt = (uint64_t *)convert_physical_to_virtual(pml4[pml4_id] & ~(0xFFF));
    // uint64_t pdpt_id = ((uint64_t)PAGE_TABLE_BASE_ADDR >> 30) & 0x1FF;
    // uint64_t *pd = (uint64_t *)convert_physical_to_virtual(pdpt[pdpt_id] & ~(0xFFF));
    // uint64_t pd_id = ((uint64_t)PAGE_TABLE_BASE_ADDR >> 21) & 0x1FF;

    // pd[pd_id] = 0;
    // PAGE_ALLOC.free_page((void *)pd);
    // pdpt[pdpt_id] = 0;
    // PAGE_ALLOC.free_page((void *)pdpt);
    // pml4[pml4_id] = 0;
    // PAGE_ALLOC.free_page((void *)pml4);

    return 0;
}

uint64_t fork_copy_thread(uint64_t clone_flags, uint64_t stack_start, uint64_t stack_size, struct task_struct *tsk, struct X64_REGS *regs)
{
    struct thread_struct *thd = NULL;
    struct X64_REGS *childregs = NULL;

    thd = (struct thread_struct *)(tsk + 1);
    xmemset(thd, 0, sizeof(struct thread_struct));
    tsk->thread = thd;

    childregs = (struct X64_REGS *)((uint64_t)tsk + STACK_SIZE) - 1;

    xmemcpy(childregs, regs, sizeof(struct X64_REGS));
    childregs->rax = 0;
    childregs->rsp = stack_start;

    thd->rsp0 = (uint64_t)tsk + STACK_SIZE;
    thd->rsp = (uint64_t)childregs;
    thd->fs = current_task->thread->fs;
    thd->gs = current_task->thread->gs;

    if (tsk->flags & PF_KTHREAD)
        thd->rip = (uint64_t)kernel_thread_func;
    else
        thd->rip = (uint64_t)ret_from_syscall;

    return 0;
}

void fork_exit_thread(struct task_struct *tsk) {}

uint64_t do_fork(struct X64_REGS *regs, uint64_t clone_flags, uint64_t stack_start, uint64_t stack_size)
{
    // 获取fork锁，防止多个进程同时创建导致竞争
    spin_lock(&do_fork_lock);

    struct task_struct *tsk = NULL;   // 新任务结构体指针
    struct thread_struct *thd = NULL; // 新线程结构体指针
    void *p = (void *)NULL;           // 临时指针变量

    // 为新进程分配内核栈空间并转换为虚拟地址
    p = (void *)convert_physical_to_virtual((uint64_t)PAGE_ALLOC.alloc_pages(STACK_SIZE / PAGE_SIZE));
    if (!p)
    {
        // 如果内存分配失败，释放锁并返回0
        spin_unlock(&do_fork_lock);
        return 0;
    }
    // 清零分配的内存空间
    xmemset(p, 0, STACK_SIZE);

    // 将分配的空间起始处作为task_struct结构体
    tsk = (struct task_struct *)p;
    // 复制父进程的task_struct到子进程
    xmemcpy(tsk, current_task, sizeof(struct task_struct));

    // 初始化新进程的任务链表
    list_init(&tsk->list);

    // 设置新进程的基本属性
    tsk->priority = 2;                     // 设置默认优先级
    tsk->pid = global_pid++;               // 分配新的进程ID
    tsk->preempt_count = 0;                // 重置抢占计数
    tsk->cpu_id = alloc_cpu_id();          // 分配CPU核心ID
    tsk->state = TASK_UNINTERRUPTIBLE;     // 设置初始状态为不可中断
    tsk->next = init_task_union.task.next; // 加入任务链表
    init_task_union.task.next = tsk;
    tsk->parent = current_task; // 设置父进程
    // 初始化等待子进程退出的等待队列
    wait_queue_init(&tsk->wait_childexit, NULL);

    // 设置线程结构体
    thd = (struct thread_struct *)(tsk + 1); // 线程结构体紧跟在task_struct后面
    xmemset(thd, 0, sizeof(struct thread_struct));
    thd->elf_mappings = current_task->thread->elf_mappings; // 复制内存映射区域信息
    tsk->thread = thd;

    if ((int64_t)fork_copy_mm(tsk, clone_flags) < 0)
    {
        fork_exit_mm(tsk);
        PAGE_ALLOC.free_pages((void *)convert_virtual_to_physical((uint64_t)tsk), STACK_SIZE / PAGE_SIZE);
        return (uint64_t)-ENOMEM;
    }

    if ((int64_t)fork_copy_thread(clone_flags, stack_start, stack_size, tsk, regs) < 0)
    {
        fork_exit_thread(tsk);
        PAGE_ALLOC.free_pages((void *)convert_virtual_to_physical((uint64_t)tsk), STACK_SIZE / PAGE_SIZE);
        return (uint64_t)-ENOMEM;
    }

    // 初始化文件描述符表
    for (int i = 3; i < MAX_FD_NUM; i++)
    {
        tsk->fds[i] = (struct VFile *)malloc(sizeof(struct VFile));
        tsk->fds[i]->file = nullptr;
        tsk->fds[i]->offset = 0;
        tsk->fds[i]->readable = false;
        tsk->fds[i]->writeable = false;
    }
    tsk->fd_count = 3; // 设置初始文件描述符数量（0,1,2为标准输入输出错误）

    // 将新进程加入调度队列
    wakeup_task(tsk);

    // 释放fork锁
    spin_unlock(&do_fork_lock);

    // 返回子进程的PID
    return tsk->pid;
}

extern "C" uint64_t do_exit(uint64_t code)
{
    disable_intr();

    fork_exit_mm(current_task);
    fork_exit_thread(current_task);

    current_task->state = TASK_ZOMBIE;
    for (int i = 0; i < MAX_FD_NUM; i++)
    {
        if (current_task->fds[i])
        {
            free(current_task->fds[i]);
        }
    }
    list_del(&current_task->list);

    current_task->flags |= NEED_SCHEDULE;

    enable_intr();

    while (1)
    {
        __asm__ __volatile__("pause");
    }

failed:
    return -1;
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

static inline void wrmsr(UINT32 msr, UINT64 value)
{
    UINT32 eax = (UINT32)value;
    UINT32 edx = value >> 32;
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(eax), "d"(edx));
}

extern "C" void __switch_to(struct task_struct *prev, struct task_struct *next)
{
    init_tss[next->cpu_id].rsp0 = next->thread->rsp0;

    __asm__ __volatile__("movq  %%fs,    %0\n\t" : "=a"(prev->thread->fs));
    __asm__ __volatile__("movq  %%gs,    %0\n\t" : "=a"(prev->thread->gs));

    __asm__ __volatile__("movq  %0,     %%fs\n\t" ::"a"(next->thread->fs));
    __asm__ __volatile__("movq  %0,     %%gs\n\t" ::"a"(next->thread->gs));

    user_current[next->cpu_id] = next;
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

    current_task->thread->elf_mappings = nullptr;

    init_tss[current_task->cpu_id].rsp0 = init_thread.rsp0;

    list_init(&init_task_union.task.list);

    kernel_thread(init_kernel_thread, 10, CLONE_FS | CLONE_FILES | CLONE_SIGNAL | CLONE_VM);
    // kernel_thread(desktop_flusher, 0, CLONE_FS | CLONE_FILES | CLONE_SIGNAL | CLONE_VM);

    init_task_union.task.preempt_count = 0;
    init_task_union.task.state = TASK_RUNNING;
    init_task_union.task.cpu_id = 0;

    TASK_INITIALIZED = true;
}
