#include <efi/boot.h>
#include <efi/efi.h>
#include <efi/fbc.h>
#include <proto.h>

#define NULL 0
const FrameBufferConfig *fbc_addr;
EFI_SYSTEM_TABLE *ST;
BOOT_CONFIG *BC;

extern "C" void KernelMain(const FrameBufferConfig &fbc, EFI_SYSTEM_TABLE &SystemTable, BOOT_CONFIG *BootConfig)
{
     disable_intr();
     init_idt();
     init_gdt();
     init_apic(BootConfig->MADT);

     enable_intr();

     while (true)
     {
          __asm__ __volatile__("pause");
     }
}
