#ifndef _IDT_H_
#define _IDT_H_

#define IDT_SIZE         256
#define X86_64_INTR_GATE 0x8E
#define X86_64_TRAP_GATE 0x8F

// 除法错误
extern "C" void divide_error();
// 调试
extern "C" void debug();
// 不可屏蔽中断
extern "C" void nmi();
//
extern "C" void int3();
// 溢出
extern "C" void overflow();
// 边界问题
extern "C" void bounds();
// 未定义的操作数
extern "C" void undefined_opcode();
// 设备不可用
extern "C" void dev_not_avaliable();
extern "C" void double_fault();
extern "C" void coprocessor_segment_overrun();
extern "C" void invalid_TSS();
extern "C" void segment_not_exists();
extern "C" void stack_segment_fault();
extern "C" void general_protection();
// 缺页异常
extern "C" void page_fault();
extern "C" void x87_FPU_error();
extern "C" void alignment_check();
extern "C" void machine_check();
extern "C" void SIMD_exception();
extern "C" void virtualization_exception();

// 默认处理程序
extern "C" void undefined_interrupt();

// 中断入口
typedef void (*int_func)();

#include <stdint.h>

typedef struct
{
    uint16_t size; // 大小，单位字节
    uint64_t base; // IDT的地址（不是物理地址，但是XJ380是1：1页表好像也没啥区别）
} __attribute__((packed)) IDTR_TYPE;

typedef struct
{
    uint16_t offset_1; // 地址偏移量的第0~15位，BaseAddress + Offset = 函数地址
    uint16_t selector; // 指向GDT的一个代码段
    uint8_t  ist;      // Interrupt Stack Table的偏移量，存储在Task State Segment中。全部为零则不使用IST。
    uint8_t
        type_attributes; // 门类型，DPL和Present位。详见https://wiki.osdev.org/Interrupt_Descriptor_Table#Structure_on_x86-64。
    uint16_t offset_2; // 地址偏移量的第16~31位
    uint32_t offset_3; // 地址偏移量的第32~63位
    uint32_t zero;     // 保留
} __attribute__((packed)) IDT_GATE_TYPE;

#endif