#include "../../include/include.h"

__attribute__((aligned(0x10))) 
IDT_GATE_TYPE idt_gate[IDT_SIZE];
IDTR_TYPE idt_ptr;

void init_8259A();

// 初始化idt
void init_idt() {
    write_serial_string("Initializing IDT...\n");
    // 初始化IDTR寄存器和门描述符
    UINT16 *p_idt_limit = (UINT16 *)(&idt_ptr.size);
    UINT64 *p_idt_base  = (UINT64 *)(&idt_ptr.base);

    *p_idt_limit = IDT_SIZE * sizeof(IDT_GATE_TYPE) - 1;
    *p_idt_base  = (UINT64) &idt_gate;

    // 异常
    init_idt_desc(0 , X86_64_INTR_GATE, divide_error                , RING0);
    init_idt_desc(1 , X86_64_INTR_GATE, debug                       , RING0);
    init_idt_desc(2 , X86_64_INTR_GATE, nmi                         , RING0);
    init_idt_desc(3 , X86_64_TRAP_GATE, int3                        , RING3);
    init_idt_desc(4 , X86_64_TRAP_GATE, overflow                    , RING3);
    init_idt_desc(5 , X86_64_INTR_GATE, bounds                      , RING0);
    init_idt_desc(6 , X86_64_INTR_GATE, undefined_opcode            , RING0);
    init_idt_desc(7 , X86_64_INTR_GATE, dev_not_avaliable           , RING0);
    init_idt_desc(8 , X86_64_INTR_GATE, double_fault                , RING0);
    init_idt_desc(9 , X86_64_INTR_GATE, coprocessor_segment_overrun , RING0);
    init_idt_desc(10, X86_64_INTR_GATE, invalid_TSS                 , RING0);
    init_idt_desc(11, X86_64_INTR_GATE, segment_not_exists          , RING0);
    init_idt_desc(12, X86_64_INTR_GATE, stack_segment_fault         , RING0);
    init_idt_desc(13, X86_64_INTR_GATE, general_protection          , RING0);
    init_idt_desc(14, X86_64_INTR_GATE, page_fault                  , RING0);
    init_idt_desc(16, X86_64_INTR_GATE, x87_FPU_error               , RING0);
    init_idt_desc(17, X86_64_INTR_GATE, alignment_check             , RING0);
    init_idt_desc(18, X86_64_INTR_GATE, machine_check               , RING0);
    init_idt_desc(19, X86_64_INTR_GATE, SIMD_exception              , RING0);
    init_idt_desc(20, X86_64_INTR_GATE, virtualization_exception    , RING0);

    init_idt_desc(32, X86_64_INTR_GATE, timer_handler, RING0);

    __asm__ volatile ("lidt %0" : : "m"(idt_ptr));
    write_serial_string("IDT Initialize Success.\n");
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
    p_gate->zero                = 0;
}
