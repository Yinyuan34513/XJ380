// 我也不知道该放哪，应该不会有什么冲突罢，那就先放全局了
#ifndef _BOOT_H_
#define _BOOT_H_
#include "../efi.h"
#include "./kernel/apic/madt.h"
#include "./kernel/memory.h"
#include "./stdint.h"

typedef struct {
    MEMORY_MAP MemoryMap;

    UINT64 MADT;
    UINT64 HPET;
    UINT64 MCFG;
} BOOT_CONFIG;

#endif
