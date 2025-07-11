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
extern "C" void wsod_unexpect_intrrupt(struct X64_REGS *regs, uint64_t error_code);
extern "C" void c_device_not_avaliable(struct X64_REGS *regs, uint64_t error_code);
extern "C" void wsod_divide_error(struct X64_REGS *regs, uint64_t error_code);
extern "C" void wsod_undefined_opcode(struct X64_REGS *regs, uint64_t error_code);
extern "C" void wsod_debug(struct X64_REGS *regs, uint64_t error_code);
extern "C" void wsod_nmi(struct X64_REGS *regs, uint64_t error_code);
extern "C" void wsod_double_fault(struct X64_REGS *regs, uint64_t error_code);
extern "C" void wsod_system_kernel_error(struct X64_REGS *regs, uint64_t error_code);
extern "C" void wsod_general_protection(struct X64_REGS *regs, uint64_t error_code);
extern "C" void wsod_page_fault(struct X64_REGS *regs, uint64_t error_code);
extern "C" void default_isr(struct X64_REGS *regs, uint64_t error_code);

/*
 *
 *  kernel/cpu/
 * 
 */

// kernel/cpu/common.cpp
void outb(uint16_t port, uint8_t value);
uint8_t inb(uint16_t port);
void outw(uint16_t port, uint16_t value);
uint16_t inw(uint16_t port);
void outl(uint16_t port, uint32_t value);
uint32_t inl(uint16_t port);
void insw(uint16_t port, void *buf, unsigned long n);
void outsw(uint16_t port, const void *buf, unsigned long n);
void insl(uint32_t port, void *addr, int cnt);
void outsl(uint32_t port, const void *addr, int cnt);
void enable_intr();
void disable_intr();
void io_mfence();

// kernel/cpu/cpuid.cpp
void asm_cpuid(uint32_t mop, uint32_t sop, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx);
void get_cpu_name(char *c);
bool mtrr_supported();

// kernel/cpu/mtrr.cpp
void mtrr_save();
void mtrr_restore();

// kernel/cpu/delay.cpp
// 读取TSC
static inline uint64_t rdtsc();
void delay(uint32_t count);
uint64_t get_cpu_freq_mhz();
void delay_us(uint64_t us);
void delay_ms(uint64_t ms);
void delay_s(uint64_t s);

/*
 *
 *  kernel/intr/
 * 
 */

// kernel/intr/8259a.cpp
void init_8259A();
void disable_8259A();

// kernel/intr/apic.cpp
void lapic_write(uint32_t reg, uint64_t value);
uint32_t lapic_read(uint32_t reg);
void ioapic_mask_all();
void init_apic(uint64_t MADT0);

// kernel/intr/hpet.cpp
void init_hpet(uint64_t hpet_ptr);

/*
 *
 *  kernel/pctable/
 * 
 */

// kernel/pctable/idt.cpp
void init_idt();

// kernel/pctable/gdt.cpp
void init_gdt();

#include <mm/memory.h>

/*
 *
 *  kernel/memory/
 * 
 */

#include <smp/smp.h>

/*
 *
 *  kernel/smp/
 * 
 */

// kernel/smp/smp.cpp
void init_smp(uint64_t MADT0, XSK_SMP_INFO *xsi);

#endif