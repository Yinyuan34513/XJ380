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
bool                     no_interrupt = false; // 为true时代表严禁开启中断

void test_task()
{
    uint64_t time = nanoTime();
    while (true)
    {
        time = nanoTime();
        write_serial_fmt("Hello from `test_task` in %llu\n", time);
    }
}

void test_task1()
{
    uint64_t time = nanoTime();
    while (true)
    {
        time = nanoTime();
        write_serial_fmt("Hello from `test_task1` in %llu\n", time);
    }
}

extern "C" void KernelMain(const FrameBufferConfig &fbc, EFI_SYSTEM_TABLE &SystemTable, BOOT_CONFIG &BootConfig)
{
    (void)fbc;         // 暂时不用
    (void)SystemTable; // 暂时不用
    no_interrupt = true;
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
    no_interrupt = false;

    uint64_t phys = alloc_frames(2);
    write_serial_string("Alloc memory: ");
    write_serial_hex(phys);
    write_serial_string("\n");

    process_setup();

    char test_task_name[]  = "test_task";
    char test_task1_name[] = "test_task1";
    create_kernel_thread((void *)test_task, NULL, test_task_name, NULL);
    create_kernel_thread((void *)test_task1, NULL, test_task1_name, NULL);
    

    open_interrupt;
    enable_scheduler();

    while (true)
    {
        __asm__ __volatile__("pause");
    }
}
