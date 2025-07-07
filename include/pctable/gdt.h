#ifndef _INCLUDE_GDT_H_
#define _INCLUDE_GDT_H_

#include <stdint.h>

#define GDT_TYPE_ZERO 0
#define GDT_TYPE_KERNEL_CS 1
#define GDT_TYPE_KERNEL_DS 2
#define GDT_TYPE_USER_DS 3
#define GDT_TYPE_USER_CS 4
#define GDT_TYPE_TSS 6

#define SELECTOR_KERNEL_CS (GDT_TYPE_KERNEL_CS << 3)
#define SELECTOR_KERNEL_DS (GDT_TYPE_KERNEL_DS << 3)
#define SELECTOR_USER_CS ((GDT_TYPE_USER_CS << 3) | RING3)
#define SELECTOR_USER_DS ((GDT_TYPE_USER_DS << 3) | RING3)

// 全局描述符表长度
#define GDT_LENGTH GDT_TYPE_TSS + MAX_CPU_NUM * 2 + 1

// GDT 段描述符结构体 (64位模式)
struct GDT_ENTRY
{
	UINT16 limit;  // 段界限 [0:15]
	UINT16 base_low;   // 基地址 [0:15]
	UINT8 base_mid; // 基地址 [16:23]
	UINT8 access;	   // P(1) | DPL(2) | S(1) | Type(4)
	UINT8 granularity; // G(1) | D/B(1) | L(1) | AVL(1) | Limit[16:19](4)
	UINT8 base_hi;   // 基地址 [24:31]
} __attribute__((packed));

// GDTR
typedef struct
{
	UINT16 limit; // 全局描述符表限长
	UINT64 base;  // 全局描述符表 32 位基地址
} __attribute__((packed)) GDT_PTR;

extern GDT_PTR gdt_ptr;

#endif