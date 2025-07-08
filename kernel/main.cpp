#include <efi/boot.h>
#include <efi/efi.h>
#include <efi/fbc.h>
#include <proto.h>

#define NULL 0
const FrameBufferConfig *fbc_addr;
EFI_SYSTEM_TABLE *ST;
BOOT_CONFIG *BC;
uint64_t *saved_mtrrs;
void *temp_stack[MAX_CPU_NUM];
void *smp_tpl[MAX_CPU_NUM];
uint32_t smp_trampoline_rs;

extern "C" void KernelMain(const FrameBufferConfig &fbc, EFI_SYSTEM_TABLE &SystemTable, BOOT_CONFIG *BootConfig)
{
     disable_intr();
     init_idt();
     init_gdt();
     init_apic(BootConfig->MADT);
     // 超绝偷懒解法，等有内存管理且在这之前初始化的时候叫我来改
     saved_mtrrs = (uint64_t *)(BootConfig->saved_mtrrs);
     for (int i = 0; i < MAX_CPU_NUM; ++i)
          temp_stack[i] = BootConfig->temp_stack[i];
     for (int i = 0; i < MAX_CPU_NUM; ++i)
          smp_tpl[i] = BootConfig->smp_trapl[i];
     smp_trampoline_rs = BootConfig->smp_trampoline_rs;

     enable_intr();

     while (true)
     {
          __asm__ __volatile__("pause");
     }
}
