#include "./alloc/page.h"
#include "../include/include.h"
#include "../include/efi.h"
#include "./build_settings.h"

#define NULL 0
const FrameBufferConfig *fbc_addr;
EFI_SYSTEM_TABLE *ST;
BOOT_CONFIG *BC;

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

void init_cpu()
{
  __asm__ __volatile__("movq %cr0, %rax\n\t"
                       "and $0xFFF3, %ax	\n\t" // clear coprocessor emulation CR0.EM and CR0.TS
                       "or $0x2, %ax\n\t"       // set coprocessor monitoring  CR0.MP
                       "movq %rax, %cr0\n\t"
                       "movq %cr4, %rax\n\t"
                       "or $(3 << 9), %ax\n\t" // set CR4.OSFXSR and CR4.OSXMMEXCPT at the same time
                       "movq %rax, %cr4\n\t");
}

extern "C" void KernelMain(const FrameBufferConfig &fbc, EFI_SYSTEM_TABLE &SystemTable, BOOT_CONFIG *BootConfig)
{
  fbc_addr = &fbc;
  ST = (EFI_SYSTEM_TABLE *)convert_physical_to_virtual((uint64_t)&SystemTable);
  BC = (BOOT_CONFIG *)convert_physical_to_virtual((UINT64)BootConfig);

  disable_intr();

  __asm__ __volatile__("movq %0, %%rbp" : : "r"((uint64_t)(init_task_union.stack + STACK_SIZE / sizeof(uint64_t))));
  __asm__ __volatile__("movq %0, %%rsp" : : "r"((uint64_t)(init_task_union.stack + STACK_SIZE / sizeof(uint64_t))));

  init_cpu();

  init_tss[current_task->cpu_id] = INIT_TSS;

  init_serial();

  init_gdt();
  init_idt();
  ltr(GDT_TYPE_TSS);

  init_MemoryManager(fbc, BC);

  uint8_t *ptr = (uint8_t *)convert_physical_to_virtual((uint64_t)PAGE_ALLOC.alloc_pages(STACK_SIZE / PAGE_SIZE)) + STACK_SIZE;
  xmemset(ptr, 0, STACK_SIZE);
  ((struct task_struct *)(ptr - STACK_SIZE))->cpu_id = 0;

  init_tss[current_task->cpu_id].ist1 = (uint64_t)ptr;
  init_tss[current_task->cpu_id].ist2 = (uint64_t)ptr;
  init_tss[current_task->cpu_id].ist3 = (uint64_t)ptr;
  init_tss[current_task->cpu_id].ist4 = (uint64_t)ptr;
  init_tss[current_task->cpu_id].ist5 = (uint64_t)ptr;
  init_tss[current_task->cpu_id].ist6 = (uint64_t)ptr;
  init_tss[current_task->cpu_id].ist7 = (uint64_t)ptr;

  init_hpet(convert_physical_to_virtual((uint64_t)BC->HPET));
  init_apic(convert_physical_to_virtual((uint64_t)BC->MADT));

  SMP_MANAGER.ap_startup(convert_physical_to_virtual((uint64_t)BC->MADT));

  schedule_init();
  init_syscall();
  init_task_manager();

  enable_intr();

  while (1)
  {
    __asm__ __volatile__("pause");
  }
}
