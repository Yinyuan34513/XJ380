#ifndef _SMP_H_
#define _SMP_H_

#define MAX_CPU_NUM 256

#include <stdint.h>
#include "lock_queue.h"

struct PROCESSOR_INFO {
    uint64_t processor_id;
    uint64_t lapic_id;           // 字面意思。
    uint64_t gdt_entries_t[7];   // GDT条目
    lock_queue *scheduler_queue; // 该核心的调度队列
};

struct XSK_SMP_INFO {
    size_t cpu_count;           // CPU 核心总数
    uint32_t bsp_lapic_id;      // BSP 的lapic id
    struct PROCESSOR_INFO pcr_inf[MAX_CPU_NUM];     // 各个核心的数据
};

#endif