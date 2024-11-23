#ifndef _PROTECT_H_
#define _PROTECT_H_

#include "./stdint.h"

// 1和2没啥用，不放了
#define RING0       0
#define RING3       3

#include "./stdint.h"

struct X64_REGS
{
    UINT64 r15, r14, r13, r12, r11, r10, r9, r8;
    UINT64 rbx, rcx, rdx, rsi, rdi, rbp;
    UINT64 ds, es;
    UINT64 rax;
    UINT64 func, errcode;
    UINT64 rip;
    UINT64 cs;
    UINT64 rflags;
    UINT64 rsp;
    UINT64 ss;
};

#endif