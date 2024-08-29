#include "../include/efi.h"
#include "../include/include.h"
void init_MemoryManager(const FrameBufferConfig &fbc, MEMORY_MAP &MemoryMap) {
    // 初始化
    EFI_MEMORY_DESCRIPTOR *EfiMemory = (EFI_MEMORY_DESCRIPTOR *)(MemoryMap.Buffer);
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
