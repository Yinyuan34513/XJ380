#include "./alloc/page.h"
// #include "../include/include.h"
// #include "../include/efi.h"
// #include "./build_settings.h"

#define NULL 0
const FrameBufferConfig *fbc_addr;
EFI_SYSTEM_TABLE        *ST;
BOOT_CONFIG             *BC;
SHEET_INFO              *sht_img;
XWM_INFO                *xwmii;

#if BUILD_VERSION == DEBUG_VERSION
bool MemoryWasInit = false;
#endif

static inline __attribute__((always_inline)) char *u64tostrb16(char *buf, size_t len, UINT64 n) {
    char *s = buf + len;
    *--s    = '\0';
    if (n == 0) return (*--s = '0', s);
    for (; n; n >>= 4)
        *--s = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"[n & 15];
    return s;
}

void init_cpu() {
    __asm__ __volatile__(
        "movq %%cr0, %%rax\n\t"
        "and $0xFFF3, %%ax	\n\t" // clear coprocessor emulation CR0.EM and CR0.TS
        "or $0x2, %%ax\n\t"       // set coprocessor monitoring  CR0.MP
        "movq %%rax, %%cr0\n\t"
        "movq %%cr4, %%rax\n\t"
        "or $(3 << 9), %%ax\n\t" // set CR4.OSFXSR and CR4.OSXMMEXCPT at the same time
        "movq %%rax, %%cr4\n\t" ::
            : "rax");
}

extern "C" [[noreturn]] void KernelMain(const FrameBufferConfig &fbc, EFI_SYSTEM_TABLE &SystemTable,
                                        BOOT_CONFIG *BootConfig) {
    fbc_addr = &fbc;
    ST       = (EFI_SYSTEM_TABLE *)convert_physical_to_virtual((uint64_t)&SystemTable);
    BC       = (BOOT_CONFIG *)convert_physical_to_virtual((UINT64)BootConfig);

    WriteString(fbc, int(fbc.horizontal_resolution / 2 - 76),
                int(fbc.vertical_resolution * 0.75 - 40), "XJ380 is Loading...",
                {0xff, 0xff, 0xff});
    WriteString(fbc, int(fbc.horizontal_resolution / 2 - 76),
                int(fbc.vertical_resolution * 0.75 - 20), "Initializing CPU...",
                {0xff, 0xff, 0xff});
    rect(*fbc_addr, 48, int(fbc_addr->vertical_resolution - 68),
         int(fbc_addr->horizontal_resolution - 48), int(fbc_addr->vertical_resolution - 48),
         {0xe6, 0xe6, 0xe6});

    disable_intr();

    init_cpu();

    init_tss[current_task->cpu_id] = INIT_TSS;

    init_serial();

    init_gdt();
    init_idt();
    ltr(GDT_TYPE_TSS);

    rect(fbc, int(fbc.horizontal_resolution / 2 - 76), int(fbc.vertical_resolution * 0.75 - 20),
         int(fbc.horizontal_resolution / 2 + 76), int(fbc.vertical_resolution * 0.75 - 4),
         {0, 0, 0});
    rect(*fbc_addr, 48, int(fbc_addr->vertical_resolution - 68),
         int(fbc_addr->horizontal_resolution - 48) * 0.1, int(fbc_addr->vertical_resolution - 48),
         {0x23, 0xcf, 0x3e});
    WriteString(fbc, int(fbc.horizontal_resolution / 2 - 88),
                int(fbc.vertical_resolution * 0.75 - 20), "Initializing Memory...",
                {0xff, 0xff, 0xff});

    init_MemoryManager(*fbc_addr, BC);

    uint64_t ist_phys = (uint64_t)PAGE_ALLOC.alloc_pages(STACK_SIZE / PAGE_SIZE);
    uint64_t ist_virt = convert_physical_to_virtual(ist_phys);
    uint8_t *ptr      = (uint8_t *)(ist_virt + STACK_SIZE);
    xmemset(ptr, 0, STACK_SIZE);
    ((struct task_struct *)ist_virt)->cpu_id = 0;

    KERNEL_MAPPER mapper((uint64_t *)convert_physical_to_virtual((uint64_t)current_task->mm->pgd));
    mapper.map_range(ist_virt, ist_phys, STACK_SIZE, 0x3);

    init_tss[current_task->cpu_id].ist1 = (uint64_t)ptr;
    init_tss[current_task->cpu_id].ist2 = (uint64_t)ptr;
    init_tss[current_task->cpu_id].ist3 = (uint64_t)ptr;
    init_tss[current_task->cpu_id].ist4 = (uint64_t)ptr;
    init_tss[current_task->cpu_id].ist5 = (uint64_t)ptr;
    init_tss[current_task->cpu_id].ist6 = (uint64_t)ptr;
    init_tss[current_task->cpu_id].ist7 = (uint64_t)ptr;

    rect(fbc, int(fbc.horizontal_resolution / 2 - 88), int(fbc.vertical_resolution * 0.75 - 20),
         int(fbc.horizontal_resolution / 2 + 88), int(fbc.vertical_resolution * 0.75 - 4),
         {0, 0, 0});
    rect(*fbc_addr, 48, int(fbc_addr->vertical_resolution - 68),
         int(fbc_addr->horizontal_resolution - 48) * 0.4, int(fbc_addr->vertical_resolution - 48),
         {0x23, 0xcf, 0x3e});
    WriteString(fbc, int(fbc.horizontal_resolution / 2 - 80),
                int(fbc.vertical_resolution * 0.75 - 20), "Initializing APIC...",
                {0xff, 0xff, 0xff});

    init_hpet(convert_physical_to_virtual((uint64_t)BC->HPET));
    init_apic(convert_physical_to_virtual((uint64_t)BC->MADT));

    SMP_MANAGER.ap_startup(convert_physical_to_virtual((uint64_t)BC->MADT));

    rect(fbc, int(fbc.horizontal_resolution / 2 - 80), int(fbc.vertical_resolution * 0.75 - 20),
         int(fbc.horizontal_resolution / 2 + 80), int(fbc.vertical_resolution * 0.75 - 4),
         {0, 0, 0});
    rect(*fbc_addr, 48, int(fbc_addr->vertical_resolution - 68),
         int(fbc_addr->horizontal_resolution - 48) * 0.5, int(fbc_addr->vertical_resolution - 48),
         {0x23, 0xcf, 0x3e});
    WriteString(fbc, int(fbc.horizontal_resolution / 2 - 116),
                int(fbc.vertical_resolution * 0.75 - 20), "Initializing Multi Tasking...",
                {0xff, 0xff, 0xff});

    init_schedule();
    init_syscall();
    init_task_manager();

    enable_intr();

    while (true) {
        __asm__ __volatile__("pause");
    }
}
