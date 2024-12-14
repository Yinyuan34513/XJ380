#ifndef _APIC_H_
#define _APIC_H_

#pragma pack(1)
#ifdef __cplusplus
extern "C" {
#endif
#include "../../../efi.h"
#ifdef __cplusplus
}
#endif

#include "../../stdint.h"
#include "../longm.h"

// CPUID命令
#define CPUID_ECX_x2APIC            1 << 21
#define CPUID_EDX_APIC              1 << 9

// MSR(x2APIC)
#define IA32_APIC_BASE_MSR          0x1B                    // IA32_APIC_BASE寄存器的MSR编号
#define IA32_APIC_SVR_MSR           0x80f                   // IA32_APIC_SVR寄存器的MSR编号
#define IA32_APIC_EOI_MSR           0x80b                   // EOI寄存器的MSR编号
#define IA32_APIC_LVT_TIMER         0x832                   // LVT计时器寄存器的MSR编号
#define IA32_APIC_LVT_THERMAL       0x833                   // LVT温度传感器寄存器的MSR编号
#define IA32_APIC_LVT_PERFOR_MON    0x834                   // LVT性能监控器寄存器的MSR编号
#define IA32_APIC_LVT_LINT0         0x835                   // LVT LINT0寄存器的MSR编号
#define IA32_APIC_LVT_LINT1         0x836                   // LVT LINT1寄存器的MSR编号
#define IA32_APIC_LVT_ERROR         0x837                   // LVT错误寄存器的MSR编号

#define IA32_APIC_BASE_MSR_ENABLE   0x800                   // IA32_APIC_BASE启用APIC
#define IA32_APIC_BASE_MSR_X2       0x400                   // IA32_APIC_BASE启用x2APIC
#define IA32_APIC_SVR_MSR_LAPIC     0x100                   // IA32_APIC_SVR启用LocalAPIC
#define IA32_APIC_SVR_MSR_DISEOI    0x1000                  // IA32_APIC_SVR禁用EOI广播



// Local APIC寄存器地址偏移量
// 0x00~0x10 保留
#define LOCAL_APIC_OFFSET_Local_APIC_ID 0x20
#define LOCAL_APIC_OFFSET_Local_APIC_Version 0x30
// 0x40~0x70 保留
#define LOCAL_APIC_OFFSET_Local_APIC_TPR 0x80
#define LOCAL_APIC_OFFSET_Local_APIC_APR 0x90
#define LOCAL_APIC_OFFSET_Local_APIC_PPR 0xa0
#define LOCAL_APIC_OFFSET_Local_APIC_EOI 0xb0
#define LOCAL_APIC_OFFSET_Local_APIC_RRD 0xc0
#define LOCAL_APIC_OFFSET_Local_APIC_LDR 0xd0
#define LOCAL_APIC_OFFSET_Local_APIC_DFR 0xe0
#define LOCAL_APIC_OFFSET_Local_APIC_SVR 0xf0

#define LOCAL_APIC_OFFSET_Local_APIC_ISR_31_0 0x100
#define LOCAL_APIC_OFFSET_Local_APIC_ISR_63_32 0x110
#define LOCAL_APIC_OFFSET_Local_APIC_ISR_95_64 0x120
#define LOCAL_APIC_OFFSET_Local_APIC_ISR_127_96 0x130
#define LOCAL_APIC_OFFSET_Local_APIC_ISR_159_128 0x140
#define LOCAL_APIC_OFFSET_Local_APIC_ISR_191_160 0x150
#define LOCAL_APIC_OFFSET_Local_APIC_ISR_223_192 0x160
#define LOCAL_APIC_OFFSET_Local_APIC_ISR_255_224 0x170

#define LOCAL_APIC_OFFSET_Local_APIC_TMR_31_0 0x180
#define LOCAL_APIC_OFFSET_Local_APIC_TMR_63_32 0x190
#define LOCAL_APIC_OFFSET_Local_APIC_TMR_95_64 0x1a0
#define LOCAL_APIC_OFFSET_Local_APIC_TMR_127_96 0x1b0
#define LOCAL_APIC_OFFSET_Local_APIC_TMR_159_128 0x1c0
#define LOCAL_APIC_OFFSET_Local_APIC_TMR_191_160 0x1d0
#define LOCAL_APIC_OFFSET_Local_APIC_TMR_223_192 0x1e0
#define LOCAL_APIC_OFFSET_Local_APIC_TMR_255_224 0x1f0

#define LOCAL_APIC_OFFSET_Local_APIC_IRR_31_0 0x200
#define LOCAL_APIC_OFFSET_Local_APIC_IRR_63_32 0x210
#define LOCAL_APIC_OFFSET_Local_APIC_IRR_95_64 0x220
#define LOCAL_APIC_OFFSET_Local_APIC_IRR_127_96 0x230
#define LOCAL_APIC_OFFSET_Local_APIC_IRR_159_128 0x240
#define LOCAL_APIC_OFFSET_Local_APIC_IRR_191_160 0x250
#define LOCAL_APIC_OFFSET_Local_APIC_IRR_223_192 0x260
#define LOCAL_APIC_OFFSET_Local_APIC_IRR_255_224 0x270

#define LOCAL_APIC_OFFSET_Local_APIC_ESR 0x280

// 0x290~0x2e0 保留

#define LOCAL_APIC_OFFSET_Local_APIC_LVT_CMCI 0x2f0
#define LOCAL_APIC_OFFSET_Local_APIC_ICR_31_0 0x300
#define LOCAL_APIC_OFFSET_Local_APIC_ICR_63_32 0x310
#define LOCAL_APIC_OFFSET_Local_APIC_LVT_TIMER 0x320
#define LOCAL_APIC_OFFSET_Local_APIC_LVT_THERMAL 0x330
#define LOCAL_APIC_OFFSET_Local_APIC_LVT_PERFORMANCE_MONITOR 0x340
#define LOCAL_APIC_OFFSET_Local_APIC_LVT_LINT0 0x350
#define LOCAL_APIC_OFFSET_Local_APIC_LVT_LINT1 0x360
#define LOCAL_APIC_OFFSET_Local_APIC_LVT_ERROR 0x370
// 初始计数寄存器（定时器专用）
#define LOCAL_APIC_OFFSET_Local_APIC_INITIAL_COUNT_REG 0x380
// 当前计数寄存器（定时器专用）
#define LOCAL_APIC_OFFSET_Local_APIC_CURRENT_COUNT_REG 0x390
// 0x3A0~0x3D0 Reserved.
// 分频配置寄存器（定时器专用）
#define LOCAL_APIC_OFFSET_Local_APIC_CLKDIV 0x3e0

#define IRQ_NUM 24
#define SMP_IRQ_NUM 10
#define LOCAL_APIC_IRQ_NUM 50

extern void (*interrupt_table[24])(void);
extern void (*SMP_interrupt_table[SMP_IRQ_NUM])(void);
extern void (*local_apic_interrupt_table[LOCAL_APIC_IRQ_NUM])(void);

/*
    ICR
*/

struct INT_CMD_REG
{
    unsigned int vector : 8, // 0~7
        deliver_mode : 3,    // 8~10
        dest_mode : 1,       // 11
        deliver_status : 1,  // 12
        res_1 : 1,           // 13
        level : 1,           // 14
        trigger : 1,         // 15
        res_2 : 2,           // 16~17
        dest_shorthand : 2,  // 18~19
        res_3 : 12;          // 20~31

    union
    {
        struct
        {
            unsigned int res_4 : 24, // 32~55
                dest_field : 8;      // 56~63
        } apic_destination;

        unsigned int x2apic_destination; // 32~63
    } destination;

} __attribute__((packed));

typedef struct
{
    // 使能中断操作接口
    void (*enable)(UINT64 irq_num);
    // 禁止中断操作接口
    void (*disable)(UINT64 irq_num);
    // 安装中断操作接口
    UINT64 (*install)(UINT64 irq_num, void *arg);
    // 卸载中断操作接口
    void (*uninstall)(UINT64 irq_num);
    // 应答中断操作接口
    void (*ack)(UINT64 irq_num);
} HARDWARE_INTR_CTRL;

// 中断描述结构体
typedef struct
{
    HARDWARE_INTR_CTRL *controller;
    char *irq_name;                             // 中断名
    UINT64 parameter;                           // 中断处理函数的参数
    void (*handler)(UINT64 irq_num, UINT64 parameter, struct X64_REGS *regs);    // 中断处理函数
    UINT64 flags;                               // 自定义的标志位
} IRQ_DESC_TYPE;

// 一碰就炸
extern IRQ_DESC_TYPE interrupt_desc[IRQ_NUM];
extern IRQ_DESC_TYPE local_apic_interrupt_desc[LOCAL_APIC_IRQ_NUM];
extern IRQ_DESC_TYPE SMP_IPI_desc[SMP_IRQ_NUM];

#pragma pack()

#endif