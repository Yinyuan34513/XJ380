#include "krlibc.h"
#include "mm/frame.h"
#include "task/pcb.h"
#include <efi/boot.h>
#include <efi/efi.h>
#include <efi/fbc.h>
#include <proto.h>

#define NULL 0
const FrameBufferConfig *fbc_addr;
EFI_SYSTEM_TABLE        *ST;
BOOT_CONFIG             *BC;
uint64_t                *saved_mtrrs;
void                    *temp_stack[MAX_CPU_NUM];

void test_task()
{
    while (true)
    {
        write_serial_string("Hello from test_task!\n");
    }
}

extern "C" void KernelMain(const FrameBufferConfig &fbc, EFI_SYSTEM_TABLE &SystemTable, BOOT_CONFIG &BootConfig)
{
    (void)fbc;         // 暂时不用
    (void)SystemTable; // 暂时不用
    disable_intr();
    init_idt();
    init_gdt();
    init_hpet(BootConfig.HPET);
    init_apic(BootConfig.MADT);
    init_hhdm();
    init_frame(BootConfig.MemoryMap);
    page_setup();
    init_heap();
    // 超绝偷懒解法，等有内存管理且在这之前初始化的时候叫我来改
    saved_mtrrs = (uint64_t *)(BootConfig.saved_mtrrs);
    for (int i = 0; i < MAX_CPU_NUM; ++i)
        temp_stack[i] = BootConfig.temp_stack[i];

    init_smp(BootConfig.MADT);

    enable_intr();

    uint64_t phys = alloc_frames(2);
    write_serial_string("Alloc memory: ");
    write_serial_hex(phys);
    write_serial_string("\n");

    process_setup();

    char test_task_name[] = "test_task";
    create_kernel_thread((void *)test_task, NULL, test_task_name, NULL);

    open_interrupt;
    enable_scheduler();

    while (true)
    {
        __asm__ __volatile__("pause");
    }
}
