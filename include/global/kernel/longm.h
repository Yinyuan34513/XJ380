#ifndef _PROTECT_H_
#define _PROTECT_H_

#include "../stdint.h"

#define INT_M_CTL     0x20
#define INT_M_CTLMASK 0x21
#define INT_S_CTL     0xA0
#define INT_S_CTLMASK 0xA1

#define INT_VECTOR_IRQ0 0x20
#define INT_VECTOR_IRQ8 0x28

#define RING0 0
#define RING3 3

struct X64_REGS {
    UINT64 r15;
    UINT64 r14;
    UINT64 r13;
    UINT64 r12;
    UINT64 r11;
    UINT64 r10;
    UINT64 r9;
    UINT64 r8;
    UINT64 rbx;
    UINT64 rcx;
    UINT64 rdx;
    UINT64 rsi;
    UINT64 rdi;
    UINT64 rbp;
    UINT64 ds;
    UINT64 es;
    UINT64 rax;
    UINT64 func;
    UINT64 errcode;
    UINT64 rip;
    UINT64 cs;
    UINT64 rflags;
    UINT64 rsp;
    UINT64 ss;
};

#endif