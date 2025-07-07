#include <efi/boot.h>
#include <efi/efi.h>
#include <efi/fbc.h>

#define NULL 0
const FrameBufferConfig *fbc_addr;
EFI_SYSTEM_TABLE *ST;
BOOT_CONFIG *BC;

extern "C" void KernelMain(const FrameBufferConfig &fbc, EFI_SYSTEM_TABLE &SystemTable, BOOT_CONFIG *BootConfig)
{
  while (true)
  {
    __asm__ __volatile__("pause");
  }
}
