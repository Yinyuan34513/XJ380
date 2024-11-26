#include "../include/include.h"
#include "./build_settings.h"

void do_wsod(const char *err_code, struct X64_REGS *regs) {
    int scrx = fbc_addr->horizontal_resolution;
    int scry = fbc_addr->vertical_resolution;
    rect(*fbc_addr, 0, 0, scrx, scry, {255, 255, 255});
    WriteString(*fbc_addr, 8, 8, "XJ380 has encountered a problem. Please visit https://xingjisoft.top/product/os/xj380/error.html.", {0, 0, 0});
    WriteString(*fbc_addr, 8, 28, "Error code: ", {0, 0, 0});   WriteString(*fbc_addr, 104, 28, err_code, {0, 0, 0});

    WriteString(*fbc_addr, 8, 64, "We've stored the breakpoint, and you can send a screenshot of this screen or error log file to a technician.", {0, 0, 0});
    
    WriteString(*fbc_addr, 8, 84, "Register Data:", {0, 0, 0});
    WriteString(*fbc_addr, 8, 104, "CS  = ", {0, 0, 0});    WriteString(*fbc_addr, 256, 104, "FUNC= ", {0, 0, 0});
    WriteString(*fbc_addr, 8, 124, "DS  = ", {0, 0, 0});    WriteString(*fbc_addr, 256, 124, "FLAG= ", {0, 0, 0});
    WriteString(*fbc_addr, 8, 144, "SS  = ", {0, 0, 0});    WriteString(*fbc_addr, 256, 144, "RIP = ", {0, 0, 0});
    WriteString(*fbc_addr, 8, 164, "ES  = ", {0, 0, 0});    WriteString(*fbc_addr, 256, 164, "ERRC= ", {0, 0, 0});
    WriteString(*fbc_addr, 8, 184, "RAX = ", {0, 0, 0});    WriteString(*fbc_addr, 256, 184, "R8  = ", {0, 0, 0});
    WriteString(*fbc_addr, 8, 204, "RBX = ", {0, 0, 0});    WriteString(*fbc_addr, 256, 204, "R9  = ", {0, 0, 0});
    WriteString(*fbc_addr, 8, 224, "RCX = ", {0, 0, 0});    WriteString(*fbc_addr, 256, 224, "R10 = ", {0, 0, 0});
    WriteString(*fbc_addr, 8, 244, "RDX = ", {0, 0, 0});    WriteString(*fbc_addr, 256, 244, "R11 = ", {0, 0, 0});
    WriteString(*fbc_addr, 8, 264, "R12 = ", {0, 0, 0});    WriteString(*fbc_addr, 256, 264, "R12 = ", {0, 0, 0});
    WriteString(*fbc_addr, 8, 284, "R14 = ", {0, 0, 0});    WriteString(*fbc_addr, 256, 284, "R15 = ", {0, 0, 0});

    WriteHex(*fbc_addr, 56, 104, regs->cs, {0, 0, 0});      WriteHex(*fbc_addr, 300, 104, regs->func, {0, 0, 0});
    WriteHex(*fbc_addr, 56, 124, regs->ds, {0, 0, 0});      WriteHex(*fbc_addr, 300, 124, regs->rflags, {0, 0, 0});
    WriteHex(*fbc_addr, 56, 144, regs->ss, {0, 0, 0});      WriteHex(*fbc_addr, 300, 144, regs->rip, {0, 0, 0});
    WriteHex(*fbc_addr, 56, 164, regs->es, {0, 0, 0});      WriteHex(*fbc_addr, 300, 164, regs->errcode, {0, 0, 0});
    WriteHex(*fbc_addr, 56, 184, regs->rax, {0, 0, 0});     WriteHex(*fbc_addr, 300, 184, regs->r8, {0, 0, 0});
    WriteHex(*fbc_addr, 56, 204, regs->rbx, {0, 0, 0});     WriteHex(*fbc_addr, 300, 204, regs->r9, {0, 0, 0});
    WriteHex(*fbc_addr, 56, 224, regs->rcx, {0, 0, 0});     WriteHex(*fbc_addr, 300, 224, regs->r10, {0, 0, 0});
    WriteHex(*fbc_addr, 56, 244, regs->rdx, {0, 0, 0});     WriteHex(*fbc_addr, 300, 244, regs->r11, {0, 0, 0});
    WriteHex(*fbc_addr, 56, 264, regs->r12, {0, 0, 0});     WriteHex(*fbc_addr, 300, 264, regs->r12, {0, 0, 0});
    WriteHex(*fbc_addr, 56, 284, regs->r14, {0, 0, 0});     WriteHex(*fbc_addr, 300, 284, regs->r15, {0, 0, 0});

    WriteString(*fbc_addr, 8, 320, "OS Version     = ", {0, 0, 0}); WriteString(*fbc_addr, 144, 320, OS_VERSION, {0, 0, 0});
    WriteString(*fbc_addr, 8, 360, "OS Edition     = ", {0, 0, 0}); WriteString(*fbc_addr, 144, 360, OS_EDITION, {0, 0, 0});
    WriteString(*fbc_addr, 8, 340, "Kernel Version = ", {0, 0, 0}); WriteString(*fbc_addr, 144, 340, KN_VERSION, {0, 0, 0});
    WriteString(*fbc_addr, scrx - 424, scry - 24, "XINGJI Interactive Software - XJ380 Operating System", {0, 0, 0});
    while (1) { 
        disable_intr();
        __asm__("hlt"); 
    };
} 

// 预期外的中断
void wsod_unexpect_intrrupt(struct X64_REGS *regs, UINT64 error_code) {
    do_wsod("UNEXPECT_INTRRUPT", regs);
}

// 不可屏蔽中断
void wsod_nmi(struct X64_REGS *regs, UINT64 error_code) {
    do_wsod("NMI", regs);
}

// 双重错误
void wsod_double_fault(struct X64_REGS *regs, UINT64 error_code) {
    do_wsod("DOUBLE_FAULT", regs);
}

// 内核错误
void wsod_system_kernel_error(struct X64_REGS *regs, UINT64 error_code) {
    do_wsod("SYSTEM_KERNEL_ERROR", regs);
}

// 一般保护性异常
void wsod_general_protection(struct X64_REGS *regs, UINT64 error_code) {
    do_wsod("GENERAL_PROTECTION", regs);
}

// 页错误
void wsod_page_fault(struct X64_REGS *regs, UINT64 error_code) {
    do_wsod("PAGE_FAULT", regs);
}

// DEBUG
void wsod_debug(struct X64_REGS *regs, UINT64 error_code) {
    do_wsod("DEBUG_BREAKPOINT", regs);
}
