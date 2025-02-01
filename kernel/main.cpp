#include "../include/include.h"
#include "../include/efi.h"
#include "./build_settings.h"
#define NULL 0
const FrameBufferConfig *fbc_addr;
EFI_SYSTEM_TABLE *ST;
BOOT_CONFIG *BC;

extern struct keyboard_buf p_mouse;
static inline __attribute__((always_inline)) char *u64tostrb16(char *buf, size_t len, UINT64 n)
{
  char *s = buf + len;
  *--s = '\0';
  if (n == 0)
    return (*--s = '0', s);
  for (; n; n >>= 4)
    *--s = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"[n & 15];
  return s;
}

extern "C" void KernelMain(const FrameBufferConfig &fbc, EFI_SYSTEM_TABLE &SystemTable, BOOT_CONFIG *BootConfig)
{
  __asm__ __volatile__("movq %0, %%rsp" : : "r"((uint64_t)(init_task_union.stack + STACK_SIZE / sizeof(uint64_t))));
  disable_intr();

  fbc_addr = &fbc;
  ST = &SystemTable;
  BC = BootConfig;

  init_gdt();
  init_idt();
  ltr(GDT_TYPE_TSS);
  init_hpet(BootConfig->HPET);
  init_apic(BootConfig->MADT);

  init_MemoryManager(fbc, BootConfig);

  SMP_MANAGER.ap_startup(BootConfig->MADT);

  schedule_init();
  init_task_manager();

  enable_intr();

  while (1)
  {
    __asm__ __volatile__("pause");
  }
}
