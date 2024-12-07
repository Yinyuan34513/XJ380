// 我也不知道该放哪，应该不会有什么冲突罢，那就先放全局了
#ifndef _BOOT_H_
#define _BOOT_H_
#include "../efi.h"
#include "./stdint.h"
#include "./kernel/memory.h"
#include "./kernel/apic/madt.h"
typedef struct 
{
    MADT_FLAG   MadtFlag;
    UINT64      TableAddr;
} MADT_CONFIG;
typedef struct
{
    MADT_CONFIG MADT;
    MEMORY_MAP  MemoryMap;
} BOOT_CONFIG;
#endif
