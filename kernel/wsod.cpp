#include "../include/include.h"
#include "../include/backtrace.h"
#include "./build_settings.h"

void do_wsod(const char *err_code, struct X64_REGS *regs)
{
    disable_intr();

    uint64_t lapicid = lapic_id();
    uint64_t cpu_id = get_cpuid_by_lapicid(lapicid);

    write_serial_string("CPU ID = ");
    write_serial_dec(cpu_id);
    write_serial_string("\n");

    write_serial_string("RSP = ");
    write_serial_hex(regs->rsp);
    write_serial_string("\n");

    write_serial_string("RIP = ");
    write_serial_hex(regs->rip);
    write_serial_string("\n");

    uint64_t fsbase = rdmsr(IA32_FS_BASE);
    write_serial_string("FSBASE = ");
    write_serial_hex(fsbase);
    write_serial_string("\n");

    uint64_t gsbase = rdmsr(IA32_GS_BASE);
    write_serial_string("GSBASE = ");
    write_serial_hex(gsbase);
    write_serial_string("\n");

    int scrx = fbc_addr->horizontal_resolution;
    int scry = fbc_addr->vertical_resolution;
    rect(*fbc_addr, 0, 0, scrx, scry, {255, 255, 255});
    WriteString(*fbc_addr, 8, 8, "XJ380 has encountered a problem. Please visit https://xingjisoft.top/product/os/xj380/error.html.", {0, 0, 0});
    WriteString(*fbc_addr, 8, 28, "Error code: ", {0, 0, 0});
    WriteString(*fbc_addr, 104, 28, err_code, {0, 0, 0});

    WriteString(*fbc_addr, 8, 64, "We've stored the breakpoint, and you can send a screenshot of this screen or error log file to a technician.", {0, 0, 0});

    WriteString(*fbc_addr, 8, 84, "Register Data:", {0, 0, 0});
    WriteString(*fbc_addr, 8, 104, "CS  = ", {0, 0, 0});
    WriteString(*fbc_addr, 256, 104, "FUNC= ", {0, 0, 0});
    WriteString(*fbc_addr, 8, 124, "DS  = ", {0, 0, 0});
    WriteString(*fbc_addr, 256, 124, "FLAG= ", {0, 0, 0});
    WriteString(*fbc_addr, 8, 144, "SS  = ", {0, 0, 0});
    WriteString(*fbc_addr, 256, 144, "RIP = ", {0, 0, 0});
    WriteString(*fbc_addr, 8, 164, "ES  = ", {0, 0, 0});
    WriteString(*fbc_addr, 256, 164, "ERRC= ", {0, 0, 0});
    WriteString(*fbc_addr, 8, 184, "RAX = ", {0, 0, 0});
    WriteString(*fbc_addr, 256, 184, "R8  = ", {0, 0, 0});
    WriteString(*fbc_addr, 8, 204, "RBX = ", {0, 0, 0});
    WriteString(*fbc_addr, 256, 204, "R9  = ", {0, 0, 0});
    WriteString(*fbc_addr, 8, 224, "RCX = ", {0, 0, 0});
    WriteString(*fbc_addr, 256, 224, "R10 = ", {0, 0, 0});
    WriteString(*fbc_addr, 8, 244, "RDX = ", {0, 0, 0});
    WriteString(*fbc_addr, 256, 244, "R11 = ", {0, 0, 0});
    WriteString(*fbc_addr, 8, 264, "R12 = ", {0, 0, 0});
    WriteString(*fbc_addr, 256, 264, "R12 = ", {0, 0, 0});
    WriteString(*fbc_addr, 8, 284, "R14 = ", {0, 0, 0});
    WriteString(*fbc_addr, 256, 284, "R15 = ", {0, 0, 0});

    WriteHex(*fbc_addr, 56, 104, regs->cs, {0, 0, 0});
    WriteHex(*fbc_addr, 300, 104, regs->func, {0, 0, 0});
    WriteHex(*fbc_addr, 56, 124, regs->ds, {0, 0, 0});
    WriteHex(*fbc_addr, 300, 124, regs->rflags, {0, 0, 0});
    WriteHex(*fbc_addr, 56, 144, regs->ss, {0, 0, 0});
    WriteHex(*fbc_addr, 300, 144, regs->rip, {0, 0, 0});
    WriteHex(*fbc_addr, 56, 164, regs->es, {0, 0, 0});
    WriteHex(*fbc_addr, 300, 164, regs->errcode, {0, 0, 0});
    WriteHex(*fbc_addr, 56, 184, regs->rax, {0, 0, 0});
    WriteHex(*fbc_addr, 300, 184, regs->r8, {0, 0, 0});
    WriteHex(*fbc_addr, 56, 204, regs->rbx, {0, 0, 0});
    WriteHex(*fbc_addr, 300, 204, regs->r9, {0, 0, 0});
    WriteHex(*fbc_addr, 56, 224, regs->rcx, {0, 0, 0});
    WriteHex(*fbc_addr, 300, 224, regs->r10, {0, 0, 0});
    WriteHex(*fbc_addr, 56, 244, regs->rdx, {0, 0, 0});
    WriteHex(*fbc_addr, 300, 244, regs->r11, {0, 0, 0});
    WriteHex(*fbc_addr, 56, 264, regs->r12, {0, 0, 0});
    WriteHex(*fbc_addr, 300, 264, regs->r12, {0, 0, 0});
    WriteHex(*fbc_addr, 56, 284, regs->r14, {0, 0, 0});
    WriteHex(*fbc_addr, 300, 284, regs->r15, {0, 0, 0});

    WriteString(*fbc_addr, 8, 320, "OS Version     = ", {0, 0, 0});
    WriteString(*fbc_addr, 144, 320, OS_VERSION, {0, 0, 0});
    WriteString(*fbc_addr, 8, 360, "OS Edition     = ", {0, 0, 0});
    WriteString(*fbc_addr, 144, 360, OS_EDITION, {0, 0, 0});
    WriteString(*fbc_addr, 8, 340, "Kernel Version = ", {0, 0, 0});
    WriteString(*fbc_addr, 144, 340, KN_VERSION, {0, 0, 0});
    WriteString(*fbc_addr, scrx - 424, scry - 24, "XINGJI Interactive Software - XJ380 Operating System", {0, 0, 0});
    write_serial_string("WSOD: ");
    write_serial_string(err_code);
    write_serial_string("\n");

    backtrace(regs);

    while (1)
    {
        __asm__("pause");
    };
}

void no_regs_wsod(const char *err_code)
{
    int scrx = fbc_addr->horizontal_resolution;
    int scry = fbc_addr->vertical_resolution;
    rect(*fbc_addr, 0, 0, scrx, scry, {255, 255, 255});
    WriteString(*fbc_addr, 8, 8, "XJ380 has encountered a problem. Please visit https://xingjisoft.top/product/os/xj380/error.html.", {0, 0, 0});
    WriteString(*fbc_addr, 8, 28, "Error code: ", {0, 0, 0});
    WriteString(*fbc_addr, 104, 28, err_code, {0, 0, 0});

    WriteString(*fbc_addr, 8, 64, "We've stored the breakpoint, and you can send a screenshot of this screen or error log file to a technician.", {0, 0, 0});

    WriteString(*fbc_addr, 8, 84, "Register Data:", {0, 0, 0});
    WriteString(*fbc_addr, 8, 104, "CS  = N/A", {0, 0, 0});
    WriteString(*fbc_addr, 256, 104, "FUNC= N/A", {0, 0, 0});
    WriteString(*fbc_addr, 8, 124, "DS  = N/A", {0, 0, 0});
    WriteString(*fbc_addr, 256, 124, "FLAG= N/A", {0, 0, 0});
    WriteString(*fbc_addr, 8, 144, "SS  = N/A", {0, 0, 0});
    WriteString(*fbc_addr, 256, 144, "RIP = N/A", {0, 0, 0});
    WriteString(*fbc_addr, 8, 164, "ES  = N/A", {0, 0, 0});
    WriteString(*fbc_addr, 256, 164, "ERRC= N/A", {0, 0, 0});
    WriteString(*fbc_addr, 8, 184, "RAX = N/A", {0, 0, 0});
    WriteString(*fbc_addr, 256, 184, "R8  = N/A", {0, 0, 0});
    WriteString(*fbc_addr, 8, 204, "RBX = N/A", {0, 0, 0});
    WriteString(*fbc_addr, 256, 204, "R9  = N/A", {0, 0, 0});
    WriteString(*fbc_addr, 8, 224, "RCX = N/A", {0, 0, 0});
    WriteString(*fbc_addr, 256, 224, "R10 = N/A", {0, 0, 0});
    WriteString(*fbc_addr, 8, 244, "RDX = N/A", {0, 0, 0});
    WriteString(*fbc_addr, 256, 244, "R11 = N/A", {0, 0, 0});
    WriteString(*fbc_addr, 8, 264, "R12 = N/A", {0, 0, 0});
    WriteString(*fbc_addr, 256, 264, "R12 = N/A", {0, 0, 0});
    WriteString(*fbc_addr, 8, 284, "R14 = N/A", {0, 0, 0});
    WriteString(*fbc_addr, 256, 284, "R15 = N/A", {0, 0, 0});

    WriteString(*fbc_addr, 8, 320, "OS Version     = ", {0, 0, 0});
    WriteString(*fbc_addr, 144, 320, OS_VERSION, {0, 0, 0});
    WriteString(*fbc_addr, 8, 360, "OS Edition     = ", {0, 0, 0});
    WriteString(*fbc_addr, 144, 360, OS_EDITION, {0, 0, 0});
    WriteString(*fbc_addr, 8, 340, "Kernel Version = ", {0, 0, 0});
    WriteString(*fbc_addr, 144, 340, KN_VERSION, {0, 0, 0});
    WriteString(*fbc_addr, scrx - 424, scry - 24, "XINGJI Interactive Software - XJ380 Operating System", {0, 0, 0});
    write_serial_string("WSOD: ");
    write_serial_string(err_code);

    disable_intr();
    while (1)
    {
        __asm__ __volatile__("pause");
    };
}

/**
 *
 *      异常处理函数
 *
 */

// 预期外的中断
void wsod_unexpect_intrrupt(struct X64_REGS *regs, UINT64 error_code)
{
    do_wsod("UNEXPECT_INTRRUPT", regs);
}

void c_device_not_avaliable(struct X64_REGS *regs, UINT64 error_code)
{
    __asm__ __volatile__(
        "movq %cr0, %rax\n\t"
        "and $0xFFF3, %ax	\n\t" // clear coprocessor emulation CR0.EM and CR0.TS
        "or $0x2, %ax\n\t"        // set coprocessor monitoring  CR0.MP
        "movq %rax, %cr0\n\t");
}

void wsod_divide_error(struct X64_REGS *regs, UINT64 error_code)
{
    do_wsod("DIVIDE_ERROR", regs);
}

void wsod_undefined_opcode(struct X64_REGS *regs, UINT64 error_code)
{
    do_wsod("UNDEFINED_OPCODE", regs);
}

// 不可屏蔽中断
void wsod_nmi(struct X64_REGS *regs, UINT64 error_code)
{
    do_wsod("NMI", regs);
}

// 双重错误
void wsod_double_fault(struct X64_REGS *regs, UINT64 error_code)
{
    do_wsod("DOUBLE_FAULT", regs);
}

// 内核错误
void wsod_system_kernel_error(struct X64_REGS *regs, UINT64 error_code)
{
    do_wsod("SYSTEM_KERNEL_ERROR", regs);
}

// 一般保护性异常
void wsod_general_protection(struct X64_REGS *regs, UINT64 error_code)
{
    if (error_code & 0x01)
        write_serial_string(
            "The exception occurred during delivery of an event external to the program,such as an interrupt "
            "or an earlier exception.\n");

    if (error_code & 0x02)
        write_serial_string("Refers to a gate descriptor in the IDT;\n");
    else
        write_serial_string("Refers to a descriptor in the GDT or the current LDT;\n");

    if ((error_code & 0x02) == 0)
        if (error_code & 0x04)
            write_serial_string("Refers to a segment or gate descriptor in the LDT;\n");
        else
            write_serial_string("Refers to a descriptor in the current GDT;\n");

    do_wsod("GENERAL_PROTECTION", regs);
}

// 页错误
void wsod_page_fault(struct X64_REGS *regs, UINT64 error_code)
{
    if (!(error_code & 0x01))
        write_serial_string("Page Not-Present,\t");

    if (error_code & 0x02)
        write_serial_string("Write Cause Fault,\t");
    else
        write_serial_string("Read Cause Fault,\t");

    if (error_code & 0x04)
        write_serial_string("Fault in user(3)\t");
    else
        write_serial_string("Fault in supervisor(0,1,2)\t");

    if (error_code & 0x08)
        write_serial_string(",Reserved Bit Cause Fault\t");

    if (error_code & 0x10)
        write_serial_string(",Instruction fetch Cause Fault");

    write_serial_string("\n");

    uint64_t cr2;
    __asm__("mov %%cr2, %0" : "=r"(cr2));
    write_serial_string("cr2 = ");
    write_serial_hex(cr2);
    write_serial_string("\n");

    do_wsod("PAGE_FAULT", regs);
}

// DEBUG
void wsod_debug(struct X64_REGS *regs, UINT64 error_code)
{
    do_wsod("DEBUG_BREAKPOINT", regs);
}

/**
 *
 *      APIC初始化错误处理函数
 *
 */

// 未找到APIC
void wsod_apic_not_found()
{
    no_regs_wsod("APIC_NOT_FOUND");
}

/**
 *
 *      较次要功能初始化错误处理函数
 *
 */

// 未找到APIC
void wsod_cannot_load_ttf()
{
    no_regs_wsod("CANNOT_LOAD_TTF");
}