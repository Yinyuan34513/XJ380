// 内存管理
extern "C" {
#include "../include/efi.h"
}
#include "../include/include.h"
// 检查是否有足够的内存描述符
bool checkEnoughDescriptors(const MEMORY_MAP *MemoryMap, int requiredCount) {
    if (MemoryMap == NULL || MemoryMap->DescriptorSize == 0) {
        return false;
    }

    // 计算Buffer中能容纳的EFI_MEMORY_DESCRIPTOR数量
    size_t numDescriptors = MemoryMap->MapSize / MemoryMap->DescriptorSize;

    // 检查是否至少包含requiredCount个描述符
    return numDescriptors >= requiredCount;
}

int init_MemoryManager(const FrameBufferConfig &fbc, MEMORY_MAP *MemoryMap) {

    // 检查是否有足够的内存描述符
    if (!checkEnoughDescriptors(MemoryMap, 4)) {
        return -1;  // 不足4个描述符，返回错误
    }

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

    return 0;  // 成功初始化
}
