#include "build_settings.h"

.section .text

.global _start
_start:
    movq %rdi, fbc
    movq %rsi, st
    movq %rdx, bc

    movq %rcx, %rbp
    movq %rcx, %rsp

    movq fbc, %rdi
    movq st, %rsi
    movq bc, %rdx

    jmp KernelMain

fbc:
    .quad 0

st:
    .quad 0

bc:
    .quad 0
