#include "../../include/asm.h"
#include "../../include/global/kernel/apic/apic.h"
#include "../../include/cinc/cproto.h"

#ifndef __cplusplus
#  define bool  _Bool
#  define true  ((bool)1)
#  define false ((bool)0)
#endif

// 一碰就炸
IRQ_DESC_TYPE interrupt_desc[IRQ_NUM] = {0};
IRQ_DESC_TYPE local_apic_interrupt_desc[LOCAL_APIC_IRQ_NUM] = {0};
IRQ_DESC_TYPE SMP_IPI_desc[SMP_IRQ_NUM] = {0};

#define SAVE_ALL             \
    "cld;			\n\t"            \
    "pushq	%rax;		\n\t"      \
    "pushq	%rax;		\n\t"      \
    "movq	%es,	%rax;	\n\t"   \
    "pushq	%rax;		\n\t"      \
    "movq	%ds,	%rax;	\n\t"   \
    "pushq	%rax;		\n\t"      \
    "xorq	%rax,	%rax;	\n\t"  \
    "pushq	%rbp;		\n\t"      \
    "pushq	%rdi;		\n\t"      \
    "pushq	%rsi;		\n\t"      \
    "pushq	%rdx;		\n\t"      \
    "pushq	%rcx;		\n\t"      \
    "pushq	%rbx;		\n\t"      \
    "pushq	%r8;		\n\t"       \
    "pushq	%r9;		\n\t"       \
    "pushq	%r10;		\n\t"      \
    "pushq	%r11;		\n\t"      \
    "pushq	%r12;		\n\t"      \
    "pushq	%r13;		\n\t"      \
    "pushq	%r14;		\n\t"      \
    "pushq	%r15;		\n\t"      \
    "movq	$0x10,	%rdx;	\n\t" \
    "movq	%rdx,	%ds;	\n\t"   \
    "movq	%rdx,	%es;	\n\t"

#define IRQ_NAME2(nr) nr##interrupt(void)
#define IRQ_NAME(nr) IRQ_NAME2(IRQ##nr)

#define Build_IRQ(nr)                                                          \
    void IRQ_NAME(nr);                                                         \
    __asm__(SYMBOL_NAME_STR(IRQ) #nr "interrupt:		\n\t"                        \
                                     "pushq	$0x00				\n\t" SAVE_ALL \
                                     "movq	%rsp,	%rdi			\n\t"                  \
                                     "leaq	ret_from_intr(%rip),	%rax	\n\t"     \
                                     "pushq	%rax				\n\t"                      \
                                     "movq	$" #nr ",	%rsi			\n\t"             \
                                     "jmp	do_IRQ	\n\t");

// 构造中断入口
Build_IRQ(0x20);
Build_IRQ(0x21);
Build_IRQ(0x22);
Build_IRQ(0x23);
Build_IRQ(0x24);
Build_IRQ(0x25);
Build_IRQ(0x26);
Build_IRQ(0x27);
Build_IRQ(0x28);
Build_IRQ(0x29);
Build_IRQ(0x2a);
Build_IRQ(0x2b);
Build_IRQ(0x2c);
Build_IRQ(0x2d);
Build_IRQ(0x2e);
Build_IRQ(0x2f);
Build_IRQ(0x30);
Build_IRQ(0x31);
Build_IRQ(0x32);
Build_IRQ(0x33);
Build_IRQ(0x34);
Build_IRQ(0x35);
Build_IRQ(0x36);
Build_IRQ(0x37);

// 初始化中断数组
void (*interrupt_table[24])(void) =
{
    IRQ0x20interrupt,
    IRQ0x21interrupt,
    IRQ0x22interrupt,
    IRQ0x23interrupt,
    IRQ0x24interrupt,
    IRQ0x25interrupt,
    IRQ0x26interrupt,
    IRQ0x27interrupt,
    IRQ0x28interrupt,
    IRQ0x29interrupt,
    IRQ0x2ainterrupt,
    IRQ0x2binterrupt,
    IRQ0x2cinterrupt,
    IRQ0x2dinterrupt,
    IRQ0x2einterrupt,
    IRQ0x2finterrupt,
    IRQ0x30interrupt,
    IRQ0x31interrupt,
    IRQ0x32interrupt,
    IRQ0x33interrupt,
    IRQ0x34interrupt,
    IRQ0x35interrupt,
    IRQ0x36interrupt,
    IRQ0x37interrupt
};



/**
 * @brief 声明10个IPI消息处理程序，向量号从200(0xc8)开始
 *
 */

/*
 */
Build_IRQ(0xc8);
Build_IRQ(0xc9);
Build_IRQ(0xca);
Build_IRQ(0xcb);
Build_IRQ(0xcc);
Build_IRQ(0xcd);
Build_IRQ(0xce);
Build_IRQ(0xcf);
Build_IRQ(0xd0);
Build_IRQ(0xd1);

// 初始化IPI中断服务程序数组
void (*SMP_interrupt_table[SMP_IRQ_NUM])(void) =
    {
        IRQ0xc8interrupt,
        IRQ0xc9interrupt,
        IRQ0xcainterrupt,
        IRQ0xcbinterrupt,
        IRQ0xccinterrupt,
        IRQ0xcdinterrupt,
        IRQ0xceinterrupt,
        IRQ0xcfinterrupt,
        IRQ0xd0interrupt,
        IRQ0xd1interrupt,
};

// 初始化local apic中断服务程序数组
Build_IRQ(0x96);
Build_IRQ(0x97);
Build_IRQ(0x98);
Build_IRQ(0x99);
Build_IRQ(0x9a);
Build_IRQ(0x9b);
Build_IRQ(0x9c);
Build_IRQ(0x9d);
Build_IRQ(0x9e);
Build_IRQ(0x9f);

void (*local_apic_interrupt_table[LOCAL_APIC_IRQ_NUM])(void) =
    {
        IRQ0x96interrupt,
        IRQ0x97interrupt,
        IRQ0x98interrupt,
        IRQ0x99interrupt,
        IRQ0x9ainterrupt,
        IRQ0x9binterrupt,
        IRQ0x9cinterrupt,
        IRQ0x9dinterrupt,
        IRQ0x9einterrupt,
        IRQ0x9finterrupt,
};
