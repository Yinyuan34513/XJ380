#ifndef _INCLUDE_GDT_H_
#define _INCLUDE_GDT_H_

#include "../../stdint.h"

#define GDT_TYPE_ZERO      0
#define GDT_TYPE_KERNEL_CS 1
#define GDT_TYPE_KERNEL_DS 2
#define GDT_TYPE_USER_DS   3
#define GDT_TYPE_USER_CS   4
#define GDT_TYPE_TSS       6

#define SELECTOR_KERNEL_CS (GDT_TYPE_KERNEL_CS << 3)
#define SELECTOR_KERNEL_DS (GDT_TYPE_KERNEL_DS << 3)
#define SELECTOR_USER_CS   ((GDT_TYPE_USER_CS << 3) | RING3)
#define SELECTOR_USER_DS   ((GDT_TYPE_USER_DS << 3) | RING3)

// 全局描述符表长度
#define GDT_LENGTH GDT_TYPE_TSS + MAX_CPU_NUM * 2 + 1

// GDT 段描述符结构体 (64位模式)
typedef struct {
    UINT16 limit_low;   // 段界限 [0:15]
    UINT16 base_low;    // 基地址 [0:15]
    UINT8  base_middle; // 基地址 [16:23]
    UINT8  access;      // P(1) | DPL(2) | S(1) | Type(4)
    UINT8  granularity; // G(1) | D/B(1) | L(1) | AVL(1) | Limit[16:19](4)
    UINT8  base_high;   // 基地址 [24:31]
} __attribute__((packed)) GDT_ENTRY;

// 系统段描述符 (TSS等)
typedef struct {
    UINT16 limit_low;   // 段界限 [0:15]
    UINT16 base_low;    // 基地址 [0:15]
    UINT8  base_middle; // 基地址 [16:23]
    UINT8  access;      // P(1) | DPL(2) | S(1) | Type(4)
    UINT8  granularity; // G(1) | 0 | 0 | AVL(1) | Limit[16:19](4)
    UINT8  base_high;   // 基地址 [24:31]
    UINT32 base_upper;  // 基地址 [32:63]
    UINT32 reserved;    // 必须为0
} __attribute__((packed)) GDT_SYSTEM_ENTRY;
// TSS（64位）
typedef struct {
    UINT16 limit_low;    // 段界限			    15 - 0
    UINT16 base_low;     // 段基地址			    15 - 0
    UINT8  base_middle1; // 段基地址             	23 - 16
    UINT8  access;       // 段存在位、描述符特权级、描述符类型、描述符子类别
    UINT8  granularity;  // 其他标志和段界限       19 - 16
    UINT8  base_middle2; // 段基地址			    31 - 24
    UINT32 base_high;    // 段基地址			    63 - 32
    UINT32 zero;         // 保留
} __attribute__((packed)) TSS_DESC;

// GDTR
typedef struct {
    UINT16 limit; // 全局描述符表限长
    UINT64 base;  // 全局描述符表 32 位基地址
} __attribute__((packed)) GDT_PTR;

extern GDT_PTR gdt_ptr;

#define ltr(n)                                                                                     \
    do {                                                                                           \
        __asm__ __volatile__("ltr	%%ax" : : "a"(n << 3) : "memory");                             \
    } while (0)

void gdt_set_gate(INT64 num, UINT64 base, UINT64 limit, UINT8 access, UINT8 sign);
void gdt_set_system_gate(INT64 num, UINT64 base, UINT64 limit, UINT8 access, UINT8 sign);

#endif
