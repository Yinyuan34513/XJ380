#include "../../include/include.h"

IDT_GATE_TYPE idt_gate[IDT_SIZE] __attribute__((__aligned__(PAGE_SIZE)));
IDTR_TYPE idt_ptr;

// // Timer
// // Keyboard
// // Mouse
BUILD_IRQ(0x23);
BUILD_IRQ(0x24);
BUILD_IRQ(0x25);
BUILD_IRQ(0x26);
BUILD_IRQ(0x27);
BUILD_IRQ(0x28);
BUILD_IRQ(0x29);
BUILD_IRQ(0x2a);
BUILD_IRQ(0x2b);
BUILD_IRQ(0x2c);
BUILD_IRQ(0x2d);
BUILD_IRQ(0x2e);
BUILD_IRQ(0x2f);
BUILD_IRQ(0x30);
BUILD_IRQ(0x31);
BUILD_IRQ(0x32);
BUILD_IRQ(0x33);
BUILD_IRQ(0x34);
BUILD_IRQ(0x35);
BUILD_IRQ(0x36);
BUILD_IRQ(0x37);
BUILD_IRQ(0x38);
BUILD_IRQ(0x39);
BUILD_IRQ(0x40);
BUILD_IRQ(0x41);
BUILD_IRQ(0x42);
BUILD_IRQ(0x43);
BUILD_IRQ(0x44);
BUILD_IRQ(0x45);
BUILD_IRQ(0x46);
BUILD_IRQ(0x47);
BUILD_IRQ(0x48);
BUILD_IRQ(0x49);
BUILD_IRQ(0x50);

void (*interrupt[34])(void) =
    {
        IRQ0x23_interrupt,
        IRQ0x24_interrupt,
        IRQ0x25_interrupt,
        IRQ0x26_interrupt,
        IRQ0x27_interrupt,
        IRQ0x28_interrupt,
        IRQ0x29_interrupt,
        IRQ0x2a_interrupt,
        IRQ0x2b_interrupt,
        IRQ0x2c_interrupt,
        IRQ0x2d_interrupt,
        IRQ0x2e_interrupt,
        IRQ0x2f_interrupt,
        IRQ0x30_interrupt,
        IRQ0x31_interrupt,
        IRQ0x32_interrupt,
        IRQ0x33_interrupt,
        IRQ0x34_interrupt,
        IRQ0x35_interrupt,
        IRQ0x36_interrupt,
        IRQ0x37_interrupt,
        IRQ0x38_interrupt,
        IRQ0x39_interrupt,
        IRQ0x40_interrupt,
        IRQ0x41_interrupt,
        IRQ0x42_interrupt,
        IRQ0x43_interrupt,
        IRQ0x44_interrupt,
        IRQ0x45_interrupt,
        IRQ0x46_interrupt,
        IRQ0x47_interrupt,
        IRQ0x48_interrupt,
        IRQ0x49_interrupt,
        IRQ0x50_interrupt,
};

void init_8259A();

/**
 * @brief 初始化中断描述符表（IDT）
 *
 */
void init_idt()
{
    write_serial_string("Initializing IDT...\n");
    // 初始化IDTR寄存器和门描述符
    UINT16 *p_idt_limit = (UINT16 *)(&idt_ptr.size);
    UINT64 *p_idt_base = (UINT64 *)(&idt_ptr.base);

    *p_idt_limit = IDT_SIZE * sizeof(IDT_GATE_TYPE) - 1;
    *p_idt_base = (UINT64)&idt_gate;

    // 异常
    init_idt_desc(0, X86_64_INTR_GATE, divide_error, RING0);
    init_idt_desc(1, X86_64_INTR_GATE, debug, RING0);
    init_idt_desc(2, X86_64_INTR_GATE, nmi, RING0);
    init_idt_desc(3, X86_64_TRAP_GATE, int3, RING3);
    init_idt_desc(4, X86_64_TRAP_GATE, overflow, RING3);
    init_idt_desc(5, X86_64_INTR_GATE, bounds, RING0);
    init_idt_desc(6, X86_64_INTR_GATE, undefined_opcode, RING0);
    init_idt_desc(7, X86_64_INTR_GATE, dev_not_avaliable, RING0);
    init_idt_desc(8, X86_64_INTR_GATE, double_fault, RING0);
    init_idt_desc(9, X86_64_INTR_GATE, coprocessor_segment_overrun, RING0);
    init_idt_desc(10, X86_64_INTR_GATE, invalid_TSS, RING0);
    init_idt_desc(11, X86_64_INTR_GATE, segment_not_exists, RING0);
    init_idt_desc(12, X86_64_INTR_GATE, stack_segment_fault, RING0);
    init_idt_desc(13, X86_64_INTR_GATE, general_protection, RING0);
    init_idt_desc(14, X86_64_INTR_GATE, page_fault, RING0);
    init_idt_desc(16, X86_64_INTR_GATE, x87_FPU_error, RING0);
    init_idt_desc(17, X86_64_INTR_GATE, alignment_check, RING0);
    init_idt_desc(18, X86_64_INTR_GATE, machine_check, RING0);
    init_idt_desc(19, X86_64_INTR_GATE, SIMD_exception, RING0);
    init_idt_desc(20, X86_64_INTR_GATE, virtualization_exception, RING0);

    init_idt_desc(32, X86_64_INTR_GATE, timer_handler, RING0);
    init_idt_desc(33, X86_64_INTR_GATE, keyboard_handler, RING0);
    init_idt_desc(34, X86_64_INTR_GATE, mouse_handler, RING0);

    for (int i = 35; i < 35 + 34; i++)
    {
        init_idt_desc(i, X86_64_INTR_GATE, interrupt[i - 35], RING0);
    }

    __asm__ volatile("lidt %0" : : "m"(idt_ptr));
    write_serial_string("IDT Initialize Success.\n");
}

/**
 * @brief 初始化中断描述符表（IDT）中的一个描述符
 *
 * @param number 中断向量号
 * @param gate_type 门类型（如中断门、陷阱门等）
 * @param function 中断处理函数的指针
 * @param privilege 特权级别（如RING0、RING3等）
 */
void init_idt_desc(UINT8 number, UINT8 gate_type, int_func function, UINT8 privilege)
{
    IDT_GATE_TYPE *p_gate = &idt_gate[number];
    UINT64 f_base = (UINT64)function;

    p_gate->offset_1 = f_base & 0xFFFF;
    p_gate->selector = SELECTOR_KERNEL_CS;
    p_gate->ist = 0; // 不使用IST
    p_gate->type_attributes = gate_type | (privilege << 5);
    p_gate->offset_2 = (f_base >> 16) & 0xFFFF;
    p_gate->offset_3 = (f_base >> 32) & 0xFFFFFFFF;
    p_gate->zero = 0;
}
