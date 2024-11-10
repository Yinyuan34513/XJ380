#ifndef _IDT_H_
#define _IDT_H_

#define IDT_SIZE                    256
#define X86_64_INTR_GATE            0xE
#define X86_64_TRAP_GATE            0xF

// 中断向量号
#define INT_VECTOR_DIVIDE           0
#define INT_VECTOR_DEBUG            1
#define INT_VECTOR_NMI              2
#define INT_VECTOR_BREAKPOINT       3
#define INT_VECTOR_OVERFLOW         4
#define INT_VECTOR_BOUND            5
#define INT_VECTOR_INVAL_OP         6
#define INT_VECTOR_NO_MATH          7
#define INT_VECTOR_DOUBLEF          8
#define INT_VECTOR_CSO              9
#define INT_VECTOR_INVAL_TSS        10
#define INT_VECTOR_SEGMENT          11
#define INT_VECTOR_STACK_SEG        12
#define INT_VECTOR_GENERAL          13
#define INT_VECTOR_PAGE_FAULT       14
#define INT_VECTOR_ZERO             15
#define INT_VECTOR_FPU              16
#define INT_VECTOR_ALIGN_CHECK      17
#define INT_VECTOR_MACHINE_CHECK    18
#define INT_VECTOR_SIMD             19
#define INT_VECTOR_VIRTUAL_EX       20
#define INT_VECTOR_CTRL_PROTECT     21
/* 22-31 保留 */
/* 32-255 自己定义 */


#include "./stdint.h"

typedef struct
{
    UINT16 size;        // 大小，单位字节
    UINT64 base;        // IDT的地址（不是物理地址，但是XJ380是1：1页表好像也没啥区别）
} __attribute__((packed)) IDTR_TYPE;

typedef struct
{
    UINT16 offset_1;        // 地址偏移量的第0~15位，BaseAddress + Offset = 函数地址
    UINT16 selector;        // 指向GDT的一个代码段
    UINT8  ist;             // Interrupt Stack Table的偏移量，存储在Task State Segment中。全部为零则不使用IST。
    UINT8  type_attributes; // 门类型，DPL和Present位。详见https://wiki.osdev.org/Interrupt_Descriptor_Table#Structure_on_x86-64。
    UINT16 offset_2;        // 地址偏移量的第16~31位
    UINT32 offset_3;        // 地址偏移量的第32~63位
    UINT32 zero;            // 保留
} __attribute__((packed)) IDT_GATE_TYPE;



#endif
