#include "../../include/include.h"

IDT_GATE_TYPE idt_gate[IDT_SIZE];
IDTR_TYPE idt_ptr;

// 初始化idt
void init_idt() {
    // 初始化IDTR寄存器和门描述符
    UINT16 *p_idt_limit = (UINT16 *)(&idt_ptr.size);
    UINT64 *p_idt_base  = (UINT64 *)(&idt_ptr.base);

    *p_idt_limit = IDT_SIZE * sizeof(IDT_GATE_TYPE) - 1;
    *p_idt_base  = (UINT64) &idt_gate;

    init_interruptions();

    asm volatile (
		"lidt [idt_ptr]\n"
		:
		: [idt_ptr] "*p" (&idt_ptr)
	);
}

void init_interruptions() {
    init_idt_desc(INT_VECTOR_DIVIDE         , X86_64_INTR_GATE, WSOD_UNEXPECT_INTRRUPT, RING0);
    init_idt_desc(INT_VECTOR_DEBUG          , X86_64_INTR_GATE, WSOD_UNEXPECT_INTRRUPT, RING0);
    init_idt_desc(INT_VECTOR_NMI            , X86_64_INTR_GATE, WSOD_UNEXPECT_INTRRUPT, RING0);
    init_idt_desc(INT_VECTOR_BREAKPOINT     , X86_64_TRAP_GATE, WSOD_UNEXPECT_INTRRUPT, RING0);
    init_idt_desc(INT_VECTOR_OVERFLOW       , X86_64_TRAP_GATE, WSOD_UNEXPECT_INTRRUPT, RING0);
    init_idt_desc(INT_VECTOR_BOUND          , X86_64_INTR_GATE, WSOD_UNEXPECT_INTRRUPT, RING0);
    init_idt_desc(INT_VECTOR_INVAL_OP       , X86_64_INTR_GATE, WSOD_UNEXPECT_INTRRUPT, RING0);
    init_idt_desc(INT_VECTOR_NO_MATH        , X86_64_INTR_GATE, WSOD_UNEXPECT_INTRRUPT, RING0);
    init_idt_desc(INT_VECTOR_DOUBLEF        , X86_64_INTR_GATE, WSOD_UNEXPECT_INTRRUPT, RING0);
    init_idt_desc(INT_VECTOR_CSO            , X86_64_INTR_GATE, WSOD_UNEXPECT_INTRRUPT, RING0);
    init_idt_desc(INT_VECTOR_INVAL_TSS      , X86_64_INTR_GATE, WSOD_UNEXPECT_INTRRUPT, RING0);
    init_idt_desc(INT_VECTOR_SEGMENT        , X86_64_INTR_GATE, WSOD_UNEXPECT_INTRRUPT, RING0);
    init_idt_desc(INT_VECTOR_STACK_SEG      , X86_64_INTR_GATE, WSOD_UNEXPECT_INTRRUPT, RING0);
    init_idt_desc(INT_VECTOR_GENERAL        , X86_64_INTR_GATE, WSOD_UNEXPECT_INTRRUPT, RING0);
    init_idt_desc(INT_VECTOR_PAGE_FAULT     , X86_64_INTR_GATE, WSOD_UNEXPECT_INTRRUPT, RING0);
    init_idt_desc(INT_VECTOR_FPU            , X86_64_INTR_GATE, WSOD_UNEXPECT_INTRRUPT, RING0);
    init_idt_desc(INT_VECTOR_ALIGN_CHECK    , X86_64_INTR_GATE, WSOD_UNEXPECT_INTRRUPT, RING0);
    init_idt_desc(INT_VECTOR_MACHINE_CHECK  , X86_64_INTR_GATE, WSOD_UNEXPECT_INTRRUPT, RING0);
    init_idt_desc(INT_VECTOR_SIMD           , X86_64_INTR_GATE, WSOD_UNEXPECT_INTRRUPT, RING0);
    init_idt_desc(INT_VECTOR_VIRTUAL_EX     , X86_64_INTR_GATE, WSOD_UNEXPECT_INTRRUPT, RING0);
    init_idt_desc(INT_VECTOR_CTRL_PROTECT   , X86_64_INTR_GATE, WSOD_UNEXPECT_INTRRUPT, RING0);
}

void init_idt_desc(UINT8 number, UINT8 gate_type, int_func function, UINT8 privilege) {
    IDT_GATE_TYPE  *p_gate = &idt_gate[number];
    UINT64          f_base = (UINT64)function;

    p_gate->offset_1            = f_base & 0xFFFF;
    p_gate->selector            = SELECTOR_KERNEL_CS;
    p_gate->ist                 = 0;           // 不使用IST
    p_gate->type_attributes     = gate_type | (privilege << 5);
    p_gate->offset_2            = (f_base >> 16) & 0xFFFF;
    p_gate->offset_3            = (f_base >> 32) & 0xFFFFFFFF;
}
