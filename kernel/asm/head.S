#include "../build_settings.h"

.section .text

.global _start
_start:
    movq %rdi, fbc
    movq %rsi, bc
    movq %rdx, st

    movq head_stack_start(%rip), %rbp
    movq head_stack_start(%rip), %rsp

    movq fbc, %rdi
    movq bc, %rsi
    movq st, %rdx

    jmp KernelMain

head_stack_start:
    .quad init_task_union + STACK_SIZE

fbc:
    .quad 0

bc:
    .quad 0

st:
    .quad 0
