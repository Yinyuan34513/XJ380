// 我也不知道该放哪，应该不会有什么冲突罢，那就先放全局了
#ifndef _BOOT_H_
#define _BOOT_H_
#include <efi.h>
#include <memory.h>

#define MAX_CPU_NUM 256

typedef struct
{
    MEMORY_MAP MemoryMap;

    UINT64 FADT;
    UINT64 MADT;
    UINT64 HPET;
    UINT64 MCFG;

    // SMP用的，因为懒得写内存管理，属于是另辟蹊径了……
    UINT64 *saved_mtrrs;
    void   *temp_stack[MAX_CPU_NUM];

    int is_qemu;
} BOOT_CONFIG;

#endif
