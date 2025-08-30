#ifndef _BOOT_H_
#define _BOOT_H_
#include <efi/efi.h>
#include <mm/memory.h>
#include <smp/smp.h>

typedef struct
{
    MEMORY_MAP MemoryMap;

    uint64_t MADT;
    uint64_t HPET;
    uint64_t MCFG;

    // SMP用的，因为懒得写内存管理，属于是另辟蹊径了……
    uint64_t *saved_mtrrs;
    void     *temp_stack[MAX_CPU_NUM];
} BOOT_CONFIG;

#endif
