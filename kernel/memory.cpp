// 内存管理
extern "C" {
#include "../include/efi.h"
}
#include "../include/include.h"
int init_MemoryManager(const FrameBufferConfig &fbc, MEMORY_MAP *MemoryMap) {
    // 初始化
    EFI_MEMORY_DESCRIPTOR *EfiMemory = (EFI_MEMORY_DESCRIPTOR *)(MemoryMap->Buffer);
    int printx = 164;
    for (int i = 0; i < 4; i++) {
        WriteHex(fbc, 8, printx, EfiMemory->Type, WHITE);
        WriteHex(fbc, 96, printx, EfiMemory->PhysicalStart, WHITE);
        WriteHex(fbc, 248, printx, EfiMemory->VirtualStart, WHITE);
        WriteHex(fbc, 8, printx + 16, EfiMemory->NumberOfPages, WHITE);
        WriteHex(fbc, 160, printx + 16, EfiMemory->Attribute, WHITE);
        printx += 32;
        EfiMemory++;
    }
}
