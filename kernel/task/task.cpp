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
        .fsbase = 0,
        .gsbase = 0,
        .cr2 = 0,
        .trap_nr = 0,
        .error_code = 0};

union task_union init_task_union __attribute__((__section__(".data.init_task"))) = {INIT_TASK(init_task_union.task)};

struct task_struct *init_task[MAX_CPU_NUM] = {&init_task_union.task, nullptr};

struct tss_struct init_tss[MAX_CPU_NUM];

struct task_struct *user_current[MAX_CPU_NUM] = {nullptr};

extern "C" uint64_t switch_to_kernel_stack()
{
    uint64_t lapicid = lapic_id();
    uint64_t current_cpu_cpuid = get_cpuid_by_lapicid(lapicid);
    return (uint64_t)user_current[current_cpu_cpuid] + STACK_SIZE;
}

void cpu_halt()
{
    while (true)
        __asm__ __volatile__("hlt");
}

bool stack_range(uint64_t pml4_idx, uint64_t pdpt_idx, uint64_t pd_idx, uint64_t pt_idx, uint64_t user_stack_start, uint64_t user_stack_end)
{
    uint64_t addr = pml4_idx << 39 | pdpt_idx << 30 | pd_idx << 21 | pt_idx << 12;
    return user_stack_start <= addr && addr < user_stack_end;
}

// 抄limine的，这样会出问题吗？
// 我说的出问题是什么意思，你应该知道
// 如果会，那你就自己换个名字，换个写法，这样就可以了吧
#define PT_FLAG_VALID ((uint64_t)1 << 0)
#define PT_FLAG_WRITE ((uint64_t)1 << 1)
#define PT_FLAG_USER ((uint64_t)1 << 2)
#define PT_FLAG_LARGE ((uint64_t)1 << 7)
#define PT_FLAG_NX ((uint64_t)1 << 63)
#define PT_PADDR_MASK ((uint64_t)0xFFFFFFFFFFFFF000)

uint64_t clone_page_table(uint64_t cr3_old, uint64_t clone_flags, uint64_t user_stack_start, uint64_t user_stack_end)
{
    uint64_t cr3_new = (uint64_t)PAGE_ALLOC.alloc_page(); // 就不判断申请失败的情况了，不然有点麻烦，你自己加上吧
    if (cr3_new == 0)
    {
        write_serial_string("Cannot clone page table: no page can be allocated");
        return cr3_old;
    }
    // 4层嵌套for循环，简单粗暴
    // 你当然可以用算法优化一下的
    // 我就这样写了，就是给你提供个思路
    // 具体要怎么写，你自己看吧
    uint64_t *pml4_old = (uint64_t *)convert_physical_to_virtual(cr3_old & PT_PADDR_MASK);
    uint64_t *pml4_new = (uint64_t *)convert_physical_to_virtual(cr3_new & PT_PADDR_MASK);
    xmemset(pml4_new, 0, PAGE_SIZE);

    // 2048，半个页，后半个页，是内核空间，内核空间直接指针指过去就好，注意释放页表的时候不要给内核空间的页表也释放了
    xmemcpy(pml4_new + 256, pml4_old + 256, PAGE_SIZE / 2); // 我就在代码里面假设是4k的页了，如果你认为这不妥，那就自己给这些常量换成宏定义

    // for(size_t pml4_idx = 0; pml4_idx < 512; ++pml4_idx) {
    for (size_t pml4_idx = 0; pml4_idx < 256; ++pml4_idx)
    { // 256 没问题，这里只复制用户空间
        uint64_t pml4e_old = pml4_old[pml4_idx];
        bool pml4e_old_valid = pml4e_old & PT_FLAG_VALID;
        bool pml4e_old_write = pml4e_old & PT_FLAG_WRITE;
        bool pml4e_old_user = pml4e_old & PT_FLAG_USER;
        // bool pml4e_old_large = pml4e_old & PT_FLAG_LARGE;
        if (!pml4e_old_valid)
            continue; // 这里给你挖上一个坑，不知道你未来会不会掉进去，^_^
        // if(pml4e_old_large) cpu_halt();

        uint64_t pml4e_new = (uint64_t)PAGE_ALLOC.alloc_page();
        pml4e_new |= PT_FLAG_VALID;
        pml4e_new |= pml4e_old_write ? PT_FLAG_WRITE : 0;
        pml4e_new |= pml4e_old_user ? PT_FLAG_USER : 0;
        pml4_new[pml4_idx] = pml4e_new;

        uint64_t *pdpt_old = (uint64_t *)convert_physical_to_virtual(pml4e_old & PT_PADDR_MASK);
        uint64_t *pdpt_new = (uint64_t *)convert_physical_to_virtual(pml4e_new & PT_PADDR_MASK);
        xmemset(pdpt_new, 0, PAGE_SIZE);
        for (size_t pdpt_idx = 0; pdpt_idx < 512; ++pdpt_idx)
        {
            uint64_t pdpte_old = pdpt_old[pdpt_idx];
            bool pdpte_old_valid = pdpte_old & PT_FLAG_VALID;
            bool pdpte_old_write = pdpte_old & PT_FLAG_WRITE;
            bool pdpte_old_user = pdpte_old & PT_FLAG_USER;
            bool pdpte_old_large = pdpte_old & PT_FLAG_LARGE;
            if (!pdpte_old_valid)
                continue;

            // 1G的页，没见过，不会写代码
            // 虽然说我感觉是我感觉的那个样子
            // 这里就不写代码了，遇到1G的页就直接停机
            // 如果真的遇到了1G的页，我倒是还真想跟一下代码，研究研究

            // 嗯，现在大概是遇到了，至少是遇到2M的页了，但是要写这个代码吗？
            // 之后再说吧
            if (pdpte_old_large)
                cpu_halt();

            uint64_t pdpte_new = (uint64_t)PAGE_ALLOC.alloc_page();
            pdpte_new |= PT_FLAG_VALID;
            pdpte_new |= pdpte_old_write ? PT_FLAG_WRITE : 0;
            pdpte_new |= pdpte_old_user ? PT_FLAG_USER : 0;
            pdpt_new[pdpt_idx] = pdpte_new;

            uint64_t *pd_old = (uint64_t *)convert_physical_to_virtual(pdpte_old & PT_PADDR_MASK);
            uint64_t *pd_new = (uint64_t *)convert_physical_to_virtual(pdpte_new & PT_PADDR_MASK);
            xmemset(pd_new, 0, PAGE_SIZE);
            for (size_t pd_idx = 0; pd_idx < 512; ++pd_idx)
            {
                uint64_t pde_old = pd_old[pd_idx];
                bool pde_old_valid = pde_old & PT_FLAG_VALID;
                bool pde_old_write = pde_old & PT_FLAG_WRITE;
                bool pde_old_user = pde_old & PT_FLAG_USER;
                bool pde_old_large = pde_old & PT_FLAG_LARGE;
                if (!pde_old_valid)
                    continue;

                // 现在是用户空间了，用户空间有2M的页吗？
                // 不知道
                // 还是一样的，等遇到了再说吧，^_^
                if (pde_old_large)
                    cpu_halt(); // 2M page

                uint64_t pde_new = (uint64_t)PAGE_ALLOC.alloc_page();
                pde_new |= PT_FLAG_VALID;
                pde_new |= pde_old_write ? PT_FLAG_WRITE : 0;
                pde_new |= pde_old_user ? PT_FLAG_USER : 0;
                pd_new[pd_idx] = pde_new;

                uint64_t *pt_old = (uint64_t *)convert_physical_to_virtual(pde_old & PT_PADDR_MASK);
                uint64_t *pt_new = (uint64_t *)convert_physical_to_virtual(pde_new & PT_PADDR_MASK);
                xmemset(pt_new, 0, PAGE_SIZE);
                for (size_t pt_idx = 0; pt_idx < 512; ++pt_idx)
                {
                    uint64_t pte_old = pt_old[pt_idx];
                    bool pte_old_valid = pte_old & PT_FLAG_VALID;
                    bool pte_old_write = pte_old & PT_FLAG_WRITE;
                    bool pte_old_user = pte_old & PT_FLAG_USER;
                    bool pte_old_large = pte_old & PT_FLAG_LARGE;
                    if (!pte_old_valid)
                        continue;

                    uint64_t *page_old = (uint64_t *)convert_physical_to_virtual(pte_old & PT_PADDR_MASK);
                    if (clone_flags & CLONE_VM || stack_range(pml4_idx, pdpt_idx, pd_idx, pt_idx, user_stack_start, user_stack_end))
                    {
                        uint64_t pte_new = (uint64_t)PAGE_ALLOC.alloc_page();
                        pte_new |= PT_FLAG_VALID;
                        pte_new |= pte_old_write ? PT_FLAG_WRITE : 0;
                        pte_new |= pte_old_user ? PT_FLAG_USER : 0;
                        pt_new[pt_idx] = pte_new;

                        uint64_t *page_old = (uint64_t *)convert_physical_to_virtual(pte_old & PT_PADDR_MASK);
                        uint64_t *page_new = (uint64_t *)convert_physical_to_virtual(pte_new & PT_PADDR_MASK);
                        xmemcpy(page_new, page_old, PAGE_SIZE);
                    }
                    else
                        pt_new[pt_idx] = pte_old;
                }
            }
        }
    }
    return cr3_new;
}

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
        return (uint64_t)-ENOMEM;
    xmemset(buf, 0, file_size);

    pos = file_node->ops->lseek(file_node, 0, SEEK_SET);
    file_node->ops->read(file_node, buf, pos, file_size);

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
    mapper.map_range(stack_start_addr, 0, STACK_SIZE + STACK_SIZE, 0x7); // 用户栈可读可写可执行
    xmemset((void *)stack_start_addr, 0, STACK_SIZE);
    flush_tlb();

    return 0;
}

uint64_t push_slice(uint64_t ustack, uint8_t *slice, uint64_t len)
{
    uint64_t tmp_stack = ustack;
    tmp_stack -= len;
    tmp_stack -= tmp_stack % alignof(uint64_t);

    xmemcpy((void *)tmp_stack, slice, len);

    return tmp_stack;
}

uint64_t push_infos(struct task_struct *task, uint64_t current_stack, char *argv[], char *envp[])
{
    uint64_t i;
    uint64_t env_i = 0;
    uint64_t argv_i = 0;

    uint64_t tmp_stack = current_stack;
    tmp_stack = push_slice(tmp_stack, (uint8_t *)task->name, strlen(task->name) + 1);

    uint64_t *envps = (uint64_t *)malloc(512);
    xmemset(envps, 0, 512);
    uint64_t *argvps = (uint64_t *)malloc(512);
    xmemset(argvps, 0, 512);

    if (envp != nullptr)
    {
        // push envs
        for (env_i = 0; envp[env_i] != nullptr; env_i++)
        {
            tmp_stack = push_slice(tmp_stack, (uint8_t *)envp[env_i], strlen(envp[env_i]) + 1);
            envps[env_i] = tmp_stack;
        }
    }

    if (argv != nullptr)
    {
        // push argvs
        for (argv_i = 0; argv[argv_i] != nullptr; argv_i++)
        {
            tmp_stack = push_slice(tmp_stack, (uint8_t *)argv[argv_i], strlen(argv[argv_i]) + 1);
            argvps[argv_i] = tmp_stack;
        }
    }

    // push auxv
    uint8_t *tmp = (uint8_t *)malloc(2);
    xmemset(tmp, 0, 2);
    tmp_stack = push_slice(tmp_stack, tmp, 2);

    // push envp
    tmp_stack = push_slice(tmp_stack, tmp, 1);
    tmp_stack = push_slice(tmp_stack, (uint8_t *)envps, env_i * sizeof(uint64_t));

    // push argvp
    tmp_stack = push_slice(tmp_stack, tmp, 1);
    tmp_stack = push_slice(tmp_stack, (uint8_t *)argvps, argv_i * sizeof(uint64_t));

    uint64_t *args_len = (uint64_t *)malloc(sizeof(uint64_t));
    args_len[0] = argv_i;

    tmp_stack = push_slice(tmp_stack, (uint8_t *)args_len, sizeof(uint64_t));

    return tmp_stack;
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

    __asm__ __volatile__("movq  %0,     %%fs\n\t" ::"a"(current_task->thread->fs));
    __asm__ __volatile__("movq  %0,     %%gs\n\t" ::"a"(current_task->thread->gs));

    strcpy(current_task->name, name);

    stack_start_addr = push_infos(current_task, current_task->mm->start_stack, argv, envp);

    regs->rsp = stack_start_addr;
    regs->rbp = current_task->mm->start_stack;

    regs->rflags = 0x200246;

    regs->cs = SELECTOR_USER_CS;
    regs->ss = SELECTOR_USER_DS;

    regs->ds = SELECTOR_USER_DS;
    regs->es = SELECTOR_USER_DS;

    regs->rax = 1;

    __asm__ __volatile__(
        "movq %0, %%rsp\n\t"
        "jmp ret_from_intr\n\t" ::"r"((uint64_t)regs));

    return 0;
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

    init_device();
    init_pci(convert_physical_to_virtual((uint64_t)BC->MCFG));
    write_serial_string("PCI initialized.\n");

    init_xhci();
    write_serial_string("XHCI initialized.\n");
    keyboard_mouse_init_done = true;

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

    char *arg0 = "-shell";
    uint8_t **argv = (uint8_t **)malloc(sizeof(uint8_t *) * 2);
    xmemset(argv, 0, sizeof(uint8_t *) * 2);
    argv[0] = (uint8_t *)arg0;

    uint8_t **envp = (uint8_t **)malloc(sizeof(uint8_t *) * 1);
    xmemset(envp, 0, sizeof(uint8_t *) * 1);

    __asm__ __volatile__(
        "movq	%1,	%%rsp	\n\t"
        "pushq	%2		\n\t"
        "jmp   do_execve	\n\t"
        :
        : "D"(current_task->thread->rsp), "m"(current_task->thread->rsp), "m"(current_task->thread->rip), "S"("/usr/init.elf"), "d"(argv), "c"(envp)
        : "memory");

    return (uint64_t)-1;
}

extern spinlock_t fb_print_lock;

uint64_t desktop_flusher(uint64_t arg)
{
    disable_intr();

    // init_ttf(*fbc_addr);

    write_serial_string("desktop flusher is running, arg:");
    write_serial_dec(arg);
    write_serial_string(".\n");

    spin_init(&fb_print_lock);

    UINT16 desktop_sheet_id = 1145; // 如果SheetTest那行输出的是1145的话那么绝对有问题
    UINT16 dock_sheet_id = 1145;
    UINT16 mouse_sheet_id = 1145;
    SHEET_INFO shtinf;
    init_sheet(*fbc_addr, &shtinf);

    sht_img = &shtinf;

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

    init_ttf(&shtinf, &desktop_sheet_id);
    TTF_Print(&shtinf, &desktop_sheet_id, 64, 64, "114514", BLUE);

    draw_mouse(&shtinf, &mouse_sheet_id);
    XWM_INFO xwmi;
    UINT16 test_win;
    UINT16 test_win2;
    init_xwm(&xwmi);
    create_window(&xwmi, &shtinf, &test_win, "114514", 320, 180);
    create_window(&xwmi, &shtinf, &test_win2, "1919810", 320, 180);

    refresh_sheet(&shtinf); // 刷新

    while (!keyboard_mouse_init_done)
    {
        enable_intr();
        __asm__ __volatile__("pause");
    }

    UINT8 k_input;
    int tby;
    int tbx;

    // struct EFI_RUNTIME_SERVICES *RuntimeServices = (struct EFI_RUNTIME_SERVICES *)convert_physical_to_virtual((UINT64)(ST->RuntimeServices));
    // EFIAPI EFI_STATUS (*GetTime)(EFI_TIME *, EFI_TIME_CAPABILITIES *) = (EFIAPI EFI_STATUS(*)(EFI_TIME *, EFI_TIME_CAPABILITIES *))convert_physical_to_virtual((UINT64)RuntimeServices->GetTime);
    int scdx = fbc_addr->horizontal_resolution * 0.8125;
    struct time_t time;

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
        // time_read(&time);
        // draw_rect(&shtinf, &dock_sheet_id, scdx - 44, 0, scdx - 4, 24, DOCK_COL);
        // //12 小时制
        // PrintDec(&shtinf, &dock_sheet_id, scdx - 44, 4, (time.tm_hour + UTC) % 24, BLACK);
        // PrintString(&shtinf, &dock_sheet_id, scdx - 28, 4, ":", BLACK);
        // PrintDec(&shtinf, &dock_sheet_id, scdx - 20, 4, time.tm_min, BLACK);
        // refresh_part_sheet(&shtinf, scdx - 44, 0, scdx, 24);
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
    struct mm_struct *old_mm = tsk->mm;
    tsk->mm = (struct mm_struct *)malloc(sizeof(struct mm_struct));
    xmemcpy(tsk->mm, old_mm, sizeof(struct mm_struct));

    uint64_t old_page_table;
    __asm__ __volatile__("movq %%cr3, %0" : "=r"(old_page_table));

    uint64_t user_stack_start = tsk->mm->start_stack - STACK_SIZE;
    uint64_t user_stack_end = tsk->mm->start_stack;

    uint64_t new_page_table = clone_page_table(old_page_table, clone_flags, user_stack_start, user_stack_end);

    tsk->mm->pgd = (uint64_t *)new_page_table;

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
    thd->fsbase = current_task->thread->fsbase;
    thd->gsbase = current_task->thread->gsbase;

    thd->elf_mappings = tsk->thread->elf_mappings;

    if (tsk->flags & PF_KTHREAD)
        thd->rip = (uint64_t)kernel_thread_func;
    else
        thd->rip = (uint64_t)ret_from_fork;

    return 0;
}

void fork_exit_thread(struct task_struct *tsk) {}

uint64_t do_fork(struct X64_REGS *regs, uint64_t clone_flags, uint64_t stack_start, uint64_t stack_size)
{
    KERNEL_MAPPER mapper((uint64_t *)convert_physical_to_virtual((uint64_t)current_task->mm->pgd));

    // 获取fork锁，防止多个进程同时创建导致竞争
    spin_lock(&do_fork_lock);

    struct task_struct *tsk = NULL;   // 新任务结构体指针
    struct thread_struct *thd = NULL; // 新线程结构体指针
    void *p = (void *)NULL;           // 临时指针变量

    // 为新进程分配内核栈空间并转换为虚拟地址
    uint64_t p_phys = (uint64_t)PAGE_ALLOC.alloc_pages_align(STACK_SIZE / PAGE_SIZE, STACK_SIZE);
    p = (void *)convert_physical_to_virtual(p_phys);
    if (!p)
    {
        // 如果内存分配失败，释放锁并返回0
        spin_unlock(&do_fork_lock);
        return 0;
    }

    mapper.map_range((uint64_t)p, p_phys, STACK_SIZE, 0x3);
    flush_tlb();

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
    tsk->parent = current_task;             // 设置父进程
    tsk->cwd_node = current_task->cwd_node; // 设置 cwd
    // 初始化等待子进程退出的等待队列
    wait_queue_init(&tsk->wait_childexit, NULL);

    // 设置线程结构体
    thd = (struct thread_struct *)(tsk + 1); // 线程结构体紧跟在task_struct后面
    xmemset(thd, 0, sizeof(struct thread_struct));
    thd->elf_mappings = current_task->thread->elf_mappings; // 复制内存映射区域信息
    tsk->thread = thd;

    if (current_task->fpu)
    {
        tsk->fpu = (fpu_context *)convert_physical_to_virtual((uint64_t)PAGE_ALLOC.alloc_page());
        xmemcpy(tsk->fpu, current_task->fpu, sizeof(fpu_context));
    }

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

    write_serial_string("process ");
    write_serial_dec(current_task->pid);
    write_serial_string(" do_exit, code = ");
    write_serial_dec(code);
    write_serial_string("\n");

    if (current_task->parent != &init_task_union.task)
    {
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
    }

    while (1)
    {
        enable_intr();
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

void switch_mm(struct task_struct *prev, struct task_struct *next)
{
    __asm__ __volatile__("movq %0, %%cr3" : : "r"(next->mm->pgd) : "memory");
}

extern "C" void __switch_to(struct task_struct *prev, struct task_struct *next)
{
    init_tss[next->cpu_id].rsp0 = next->thread->rsp0;

    __asm__ __volatile__("fxsave (%0)" ::"r"(prev->fpu));

    __asm__ __volatile__("movq  %%fs,    %0\n\t" : "=a"(prev->thread->fs));
    __asm__ __volatile__("movq  %%gs,    %0\n\t" : "=a"(prev->thread->gs));

    prev->thread->fsbase = rdmsr(IA32_FS_BASE);
    prev->thread->gsbase = rdmsr(IA32_GS_BASE);

    __asm__ __volatile__("fxrstor (%0)" ::"r"(next->fpu));

    __asm__ __volatile__("movq  %0,     %%fs\n\t" ::"a"(next->thread->fs));
    __asm__ __volatile__("movq  %0,     %%gs\n\t" ::"a"(next->thread->gs));

    wrmsr(IA32_FS_BASE, next->thread->fsbase);
    wrmsr(IA32_GS_BASE, next->thread->gsbase);

    user_current[next->cpu_id] = next;
}

bool TASK_INITIALIZED = false;

void init_task_manager()
{
    spin_init(&do_fork_lock);

    uint64_t cr3;
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3));
    init_mm.pgd = (uint64_t *)(cr3 & ~(0xFFFUL));

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

    __asm__ __volatile__(
        "fnclex \n\t"
        "fninit \n\t");
    current_task->fpu = (fpu_context *)convert_physical_to_virtual((uint64_t)PAGE_ALLOC.alloc_page());
    xmemset(current_task->fpu, 0, sizeof(fpu_context));
    current_task->fpu->mxscr = 0x1f80;
    current_task->fpu->fcw = 0x037f;

    init_tss[current_task->cpu_id].rsp0 = init_thread.rsp0;

    list_init(&init_task_union.task.list);

    kernel_thread(init_kernel_thread, 10, CLONE_FS | CLONE_FILES | CLONE_SIGNAL);
    kernel_thread(desktop_flusher, 0, CLONE_FS | CLONE_FILES | CLONE_SIGNAL);

    init_task_union.task.preempt_count = 0;
    init_task_union.task.state = TASK_RUNNING;
    init_task_union.task.cpu_id = 0;

    TASK_INITIALIZED = true;
}
