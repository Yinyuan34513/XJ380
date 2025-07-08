#ifndef _PROTO_H_
#define _PROTO_H_

#include <stdint.h>

/*
 *
 *  driver/serial/
 * 
 */

// driver/serial/serial_port.cpp
int init_serial();
void write_serial(char a);
void write_serial_string(const char *str);
void write_serial_dec(unsigned long long dec);
void write_serial_hex(unsigned long long hex);

/*
 *
 *  kernel/
 * 
 */

// kernel/wsod.cpp
extern "C" void wsod_unexpect_intrrupt(struct X64_REGS *regs, UINT64 error_code);
extern "C" void c_device_not_avaliable(struct X64_REGS *regs, UINT64 error_code);
extern "C" void wsod_divide_error(struct X64_REGS *regs, UINT64 error_code);
extern "C" void wsod_undefined_opcode(struct X64_REGS *regs, UINT64 error_code);
extern "C" void wsod_debug(struct X64_REGS *regs, UINT64 error_code);
extern "C" void wsod_nmi(struct X64_REGS *regs, UINT64 error_code);
extern "C" void wsod_double_fault(struct X64_REGS *regs, UINT64 error_code);
extern "C" void wsod_system_kernel_error(struct X64_REGS *regs, UINT64 error_code);
extern "C" void wsod_general_protection(struct X64_REGS *regs, UINT64 error_code);
extern "C" void wsod_page_fault(struct X64_REGS *regs, UINT64 error_code);
extern "C" void default_isr(struct X64_REGS *regs, UINT64 error_code);

/*
 *
 *  kernel/cpu/
 * 
 */

// kernel/cpu/common.cpp
void outb(UINT16 port, UINT8 value);
UINT8 inb(UINT16 port);
void outw(UINT16 port, UINT16 value);
UINT16 inw(UINT16 port);
void outl(UINT16 port, UINT32 value);
UINT32 inl(UINT16 port);
void insw(UINT16 port, VOID *buf, unsigned long n);
void outsw(UINT16 port, const VOID *buf, unsigned long n);
void insl(UINT32 port, VOID *addr, int cnt);
void outsl(UINT32 port, const VOID *addr, int cnt);
void enable_intr();
void disable_intr();
void io_mfence();

// kernel/cpu/cpuid.cpp
void asm_cpuid(UINT32 mop, UINT32 sop, UINT32 *eax, UINT32 *ebx, UINT32 *ecx, UINT32 *edx);
void get_cpu_name(char *c);
bool mtrr_supported();

// kernel/cpu/mtrr.cpp
void mtrr_save();
void mtrr_restore();

/*
 *
 *  kernel/intr/
 * 
 */

// kernel/intr/8259a.cpp
void init_8259A();
void disable_8259A();

// kernel/intr/apic.cpp
void lapic_write(UINT32 reg, UINT64 value);
UINT32 lapic_read(UINT32 reg);
void ioapic_mask_all();
void init_apic(UINT64 MADT0);

/*
 *
 *  kernel/pctable/
 * 
 */

// kernel/pctable/idt.cpp
void init_idt();

// kernel/pctable/gdt.cpp
void init_gdt();

#endif