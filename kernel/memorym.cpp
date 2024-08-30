#include "../include/efi.h"
#include "../include/include.h"
typedef struct {
  UINT32 Type;
  EFI_PHYSICAL_ADDRESS PhysicalStart;
  EFI_VIRTUAL_ADDRESS VirtualStart;
  UINT64 NumberOfPages;
  UINT64 Attribute;
} __attribute__((__aligned__(16))) EfiMemoryDesc;
void init_MemoryManager(const FrameBufferConfig &fbc, BOOT_CONFIG *BootConfig) {
    // 初始化
    EfiMemoryDesc *EfiMemory = (EfiMemoryDesc *)(BootConfig->MemoryMap.Buffer);
    int printy = 164;
    for (int i = 0; i < 10; i++) {
        WriteHex(fbc, 8, printy, EfiMemory->Type, WHITE);
        WriteHex(fbc, 96, printy, EfiMemory->PhysicalStart, WHITE);
        WriteHex(fbc, 248, printy, EfiMemory->VirtualStart, WHITE);
        WriteHex(fbc, 8, printy + 16, EfiMemory->NumberOfPages, WHITE);
        WriteHex(fbc, 160, printy + 16, EfiMemory->Attribute, WHITE);
        printy += 32;
        EfiMemory++;
    }
}
