#pragma once

#include "../../../include.h"

struct XJ380_SIGNAL_STBUFFER {
    UINT64   r15;
    UINT64   r14;
    UINT64   r13;
    UINT64   r12;
    UINT64   r11;
    UINT64   r10;
    UINT64   r9;
    UINT64   r8;
    UINT64   rbx;
    UINT64   rcx;
    UINT64   rdx;
    UINT64   rsi;
    UINT64   rdi;
    UINT64   rbp;
    UINT64   ds;
    UINT64   es;
    UINT64   rax;
    UINT64   func;
    UINT64   errcode;
    UINT64   rip;
    UINT64   cs;
    UINT64   rflags;
    UINT64   rsp;
    UINT64   ss;
    // 信号处理元数据
    MsgPrcor handler;
    UINT64   arg1;
    UINT64   arg2;
    UINT64   arg3;
    // 返回跳板地址
    void (*trampoline)(void);
};
