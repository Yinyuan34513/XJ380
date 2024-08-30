// 我也不知道该放哪，应该不会有什么冲突罢，那就先放全局了
#ifndef _MEMORY_H_
#define _MEMORY_H_
#include "../efi.h"
#include "./stdint.h"
typedef struct __packed
{
    UINTN MapSize;
    VOID *Buffer;
    UINTN MapKey;
    UINTN DescriptorSize;
    UINT32 DescriptorVersion;
} MEMORY_MAP;
#endif
