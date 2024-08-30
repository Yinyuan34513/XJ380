extern "C" {
#include "../include/efi.h"
}
#include "../include/include.h"

// 定义内存类型的常量
#define EFI_CONVENTIONAL_MEMORY 0x01
#define EFI_RESERVED_MEMORY_TYPE 0x02
#define EFI_UNUSABLE_MEMORY 0x03
#define EFI_LOADER_CODE 0x04
#define EFI_LOADER_DATA 0x05
#define EFI_BOOT_SERVICES_CODE 0x06
#define EFI_BOOT_SERVICES_DATA 0x07
#define EFI_RUNTIME_SERVICES_CODE 0x08
#define EFI_RUNTIME_SERVICES_DATA 0x09
#define EFI_CONSUMED_BY_LEGACY 0x0A
#define EFI_ACPI_RECLAIM_MEMORY 0x0C
#define EFI_ACPI_NON_VOLATILE 0x0D
#define EFI_MEMORY_MAPPED_IO 0x0E
#define EFI_MEMORY_MAPPED_IO_PORT_SPACE 0x0F
#define EFI_PAL_CODE 0x10
#define EFI_PERSISTENT_MEMORY 0x11
#define EFI_MAX_MEMORY_TYPE 0x12

typedef unsigned long int uintptr_t;

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

// 获取内存类型的字符串表示
const char* GetMemoryTypeName(uint32_t type) {
    switch (type) {
        case EFI_CONVENTIONAL_MEMORY: return "ConventionalMemory";
        case EFI_RESERVED_MEMORY_TYPE: return "ReservedMemoryType";
        case EFI_UNUSABLE_MEMORY: return "UnusableMemory";
        case EFI_LOADER_CODE: return "LoaderCode";
        case EFI_LOADER_DATA: return "LoaderData";
        case EFI_BOOT_SERVICES_CODE: return "BootServicesCode";
        case EFI_BOOT_SERVICES_DATA: return "BootServicesData";
        case EFI_RUNTIME_SERVICES_CODE: return "RuntimeServicesCode";
        case EFI_RUNTIME_SERVICES_DATA: return "RuntimeServicesData";
        case EFI_CONSUMED_BY_LEGACY: return "ConsumedByLegacy";
        case EFI_ACPI_RECLAIM_MEMORY: return "ACPIReclaimMemory";
        case EFI_ACPI_NON_VOLATILE: return "ACPINonVolatile";
        case EFI_MEMORY_MAPPED_IO: return "MemoryMappedIO";
        case EFI_MEMORY_MAPPED_IO_PORT_SPACE: return "MemoryMappedIOPortSpace";
        case EFI_PAL_CODE: return "PALCode";
        case EFI_PERSISTENT_MEMORY: return "PersistentMemory";
        default: return "Unknown";
    }
}

// 初始化内存管理器
int init_MemoryManager(const FrameBufferConfig &fbc, MEMORY_MAP *MemoryMap) {
    // 检查是否有足够的内存描述符
    if (!checkEnoughDescriptors(MemoryMap, 4)) {
        WriteString(fbc, 28, 24, "Not enough descriptors", WHITE);
        WSOD_Cannot_Alloc_MemoryMap_2(fbc);
        while (1) { __asm__("hlt"); } // 无限循环
    }

    // 检查内存对齐
    if (((uintptr_t)MemoryMap->Buffer & 0x7) != 0) {
        // 地址不对齐，处理错误
        WriteString(fbc, 28, 24, "Memory not aligned", WHITE);
        WSOD_Cannot_Alloc_MemoryMap_2(fbc);
        while (1) { __asm__("hlt"); } // 无限循环
    }

    // 检查内存大小
    if (MemoryMap->MapSize < sizeof(EFI_MEMORY_DESCRIPTOR)) {
        WriteString(fbc, 28, 24, "Insufficient memory size", WHITE);
        WSOD_Cannot_Alloc_MemoryMap_2(fbc);
        while (1) { __asm__("hlt"); } // 无限循环
    }

    // 输出内存映射基本信息
    WriteString(fbc, 428, 40, "Map Size:", WHITE);
    WriteHex(fbc, 428, 56, MemoryMap->MapSize, WHITE);
    WriteString(fbc, 428, 72, "Map Key:", WHITE);
    WriteHex(fbc, 428, 88, MemoryMap->MapKey, WHITE);
    WriteString(fbc, 428, 104, "Descriptor Size:", WHITE);
    WriteHex(fbc, 428, 120, MemoryMap->DescriptorSize, WHITE);
    WriteString(fbc, 428, 136, "Descriptor Version:", WHITE);
    WriteHex(fbc, 428, 152, MemoryMap->DescriptorVersion, WHITE);

    // 转换为EFI_MEMORY_DESCRIPTOR指针
    EFI_MEMORY_DESCRIPTOR *EfiMemory = (EFI_MEMORY_DESCRIPTOR *)MemoryMap->Buffer;
    //WriteHex(fbc, 560, 150, (uint64_t)MemoryMap->Buffer, WHITE);
    int printx = 200;

    for (int i = 0; i < 4; i++) {
        // 输出类型
        const char* typeName = GetMemoryTypeName(EfiMemory->Type);

        //WriteHex(fbc, 560, printx, (uint64_t)EfiMemory, WHITE);

        //WriteString(fbc, 8, printx, typeName, WHITE); // 输出内存类型字符串
        WriteString(fbc, 8, printx, "type:", WHITE);
        WriteHex(fbc, 48, printx, EfiMemory->Type, WHITE); // 输出内存类型十六进制值

        // 输出起始地址
        WriteString(fbc, 128, printx, "start address:", WHITE);
        WriteHex(fbc, 256, printx, EfiMemory->PhysicalStart, WHITE);

        // 计算并输出结束地址
        uint64_t endAddress = EfiMemory->PhysicalStart + (EfiMemory->NumberOfPages * 0x1000) - 1;
        WriteString(fbc, 344, printx, "END address:", WHITE);
        WriteHex(fbc, 456, printx, endAddress, WHITE);

        // 输出页数
        WriteString(fbc, 8, printx+16, "Paginal number:", WHITE);
        WriteHex(fbc, 136, printx + 16, EfiMemory->NumberOfPages, WHITE);

        // 输出属性
        WriteString(fbc, 217, printx+16, "property:", WHITE);
        WriteHex(fbc, 297, printx + 16, EfiMemory->Attribute, WHITE);

        printx += 32;
        EfiMemory = (EFI_MEMORY_DESCRIPTOR *)((uintptr_t)EfiMemory + MemoryMap->DescriptorSize);
    }

    return 0;  // 成功初始化
}