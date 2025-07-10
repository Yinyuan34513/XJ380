#include <efi/boot.h>
#include <efi/efi.h>
#include <efi/fbc.h>
#include <proto.h>
#include "mm/frame.h"

#define NULL 0
const FrameBufferConfig *fbc_addr;
EFI_SYSTEM_TABLE *ST;
BOOT_CONFIG *BC;
uint64_t *saved_mtrrs;
void *temp_stack[MAX_CPU_NUM];

extern "C" void KernelMain(const FrameBufferConfig &fbc, EFI_SYSTEM_TABLE &SystemTable, BOOT_CONFIG *BootConfig)
{
    disable_intr();
    init_idt();
    init_gdt();
    init_apic(BootConfig->MADT);
    init_frame(BootConfig->MemoryMap);
    // 超绝偷懒解法，等有内存管理且在这之前初始化的时候叫我来改
    saved_mtrrs = (uint64_t *) (BootConfig->saved_mtrrs);
    for (int i = 0; i < MAX_CPU_NUM; ++i)
        temp_stack[i] = BootConfig->temp_stack[i];

    XSK_SMP_INFO xsi;
    init_smp(BootConfig->MADT, &xsi);

    enable_intr();

    uint64_t phys = alloc_frames(2);
    write_serial_string("Alloc memory: ");
    write_serial_hex(phys);
    write_serial_string("\n");

    while (true)
    {
        __asm__ __volatile__("pause");
    }
}
