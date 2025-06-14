#include "../../include/include.h"

void do_signal(UINT64 arg1, UINT64 arg2, UINT64 arg3, X64_REGS regs) {
    // 保存栈帧
    XJ380_SIGNAL_STBUFFER buffer;
    buffer.r15 = regs.r15;
    buffer.r14 = regs.r14;
    buffer.r13 = regs.r13;
    buffer.r12 = regs.r12;
    buffer.r11 = regs.r11;
    buffer.r10 = regs.r10;
    buffer.r9  = regs.r9;
    buffer.r8  = regs.r8;
    buffer.rbx = regs.rbx;
    buffer.rcx = regs.rcx;
    buffer.rdx = regs.rdx;
    buffer.rsi = regs.rsi;
    buffer.rdi = regs.rdi;
    buffer.rbp = regs.rbp;
    buffer.ds  = regs.ds;
    buffer.es  = regs.es;
    buffer.rax = regs.rax;
    buffer.rip = regs.rip;
    buffer.cs  = regs.cs;
    buffer.rsp = regs.rsp;
    buffer.ss  = regs.ss;
}
