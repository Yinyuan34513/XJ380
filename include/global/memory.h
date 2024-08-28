// 我也不知道该放哪，应该不会有什么冲突罢，那就先放全局了
#ifndef _MEMORY_H_
#define _MEMORY_H_
#include "../efi.h"
#include "./stdint.h"
typedef struct
{
    __UINT64_TYPE__ MapSize;
    void *Buffer;
    __UINT64_TYPE__ MapKey;
    __UINT64_TYPE__ DescriptorSize;
    __UINT32_TYPE__ DescriptorVersion;
} MEMORY_MAP;
#endif
