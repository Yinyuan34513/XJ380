#ifndef _INCLUDE_GDT_H_
#define _INCLUDE_GDT_H_

#include "../../stdint.h"

#define GDT_TYPE_ZERO 0
#define GDT_TYPE_KERNEL_CS 1
#define GDT_TYPE_KERNEL_DS 2
#define GDT_TYPE_USER_CS 3
#define GDT_TYPE_USER_DS 4

#define SELECTOR_KERNEL_CS GDT_TYPE_KERNEL_CS << 3
#define SELECTOR_KERNEL_DS GDT_TYPE_KERNEL_DS << 3
#define SELECTOR_USER_CS GDT_TYPE_USER_CS << 3
#define SELECTOR_USER_DS GDT_TYPE_USER_DS << 3

// 全局描述符表长度
#define GDT_LENGTH 5

// 全局描述符类型
typedef struct
{
	UINT16 limit_low;  // 段界限			    	15 - 0
	UINT16 base_low;   // 段基地址			    15 - 0
	UINT8 base_middle; // 段基地址            	23 - 16
	UINT8 access;	   // 段存在位、描述符特权级、描述符类型、描述符子类别
	UINT8 granularity; // 其他标志和段界限        19 - 16
	UINT8 base_high;   // 段基地址			    31 - 24
} __attribute__((packed)) GDT_ENTRY;

// TSS（64位）
typedef struct
{
	UINT16 limit_low;	// 段界限			    15 - 0
	UINT16 base_low;	// 段基地址			    15 - 0
	UINT8 base_middle1; // 段基地址             	23 - 16
	UINT8 access;		// 段存在位、描述符特权级、描述符类型、描述符子类别
	UINT8 granularity;	// 其他标志和段界限       19 - 16
	UINT8 base_middle2; // 段基地址			    31 - 24
	UINT32 base_high;	// 段基地址			    63 - 32
	UINT32 zero;		// 保留
} __attribute__((packed)) TSS_DESC;

// GDTR
typedef struct
{
	UINT16 limit; // 全局描述符表限长
	UINT64 base;  // 全局描述符表 32 位基地址
} __attribute__((packed)) GDT_PTR;

#endif