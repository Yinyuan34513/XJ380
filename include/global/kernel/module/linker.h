#pragma once

#define PADDING_DOWN(size, to) ((size_t)(size) / (size_t)(to) * (size_t)(to))
#define PADDING_UP(size, to)   PADDING_DOWN((size_t)(size) + (size_t)(to) - (size_t)1, to)

#define PTE_PRESENT    (0x1 << 0)
#define PTE_WRITEABLE  (0x1 << 1)
#define PTE_USER       (0x1 << 2)
#define PTE_HUGE       (0x1 << 7)
#define PTE_NO_EXECUTE (((uint64_t)0x1) << 63)

#include "../../../include.h"

/**
 * @brief 加载动态链接库
 * @param file_addr 模块文件数据基址
 * @param mmap_offset 映射偏移地址(多模块时候必须处理该偏移, 不然容易覆写)
 * @return 返回加载后的地址(失败会返回NULL并在串口打印相关信息)
 */
void *dlinker_load(VOID *file_addr, UINT64 mmap_offset);


