#ifndef _PROTO_H_
#define _PROTO_H_

#define KERNEL_STACK_SIZE 32768 // 任务内核栈地址

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
#ifdef __cplusplus
extern "C" {
#endif
void wsod_unexpect_intrrupt(struct X64_REGS *regs, uint64_t error_code);
void c_device_not_avaliable(struct X64_REGS *regs, uint64_t error_code);
void wsod_divide_error(struct X64_REGS *regs, uint64_t error_code);
void wsod_undefined_opcode(struct X64_REGS *regs, uint64_t error_code);
void wsod_debug(struct X64_REGS *regs, uint64_t error_code);
void wsod_nmi(struct X64_REGS *regs, uint64_t error_code);
void wsod_double_fault(struct X64_REGS *regs, uint64_t error_code);
void wsod_system_kernel_error(struct X64_REGS *regs, uint64_t error_code);
void wsod_general_protection(struct X64_REGS *regs, uint64_t error_code);
void wsod_page_fault(struct X64_REGS *regs, uint64_t error_code);
void default_isr(struct X64_REGS *regs, uint64_t error_code);
#ifdef __cplusplus
}
#endif

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
void init_lApic();
void init_apic(uint64_t MADT0);
uint64_t lapic_id();

// kernel/intr/hpet.cpp
uint64_t nanoTime();
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

/*
 *
 *  kernel/memory/
 * 
 */

// kernel/memory/heap/heap.cpp
#include <mm/alloc/alloc.h>
void init_heap();

#include <mm/bitmap.h>
#include <mm/memory.h>
#include <mm/page.h>

// kernel/memory/bitmap.cpp
void bitmap_init(Bitmap *bitmap, uint8_t *buffer, size_t size);
bool bitmap_get(const Bitmap *bitmap, size_t index);
void bitmap_set(Bitmap *bitmap, size_t index, bool value);
void bitmap_set_range(Bitmap *bitmap, size_t start, size_t end, bool value);
size_t bitmap_find_range(const Bitmap *bitmap, size_t length, bool value);

// kernel/memory/frame.cpp
void     init_frame(MEMORY_MAP map);
void     free_frame(uint64_t addr);
uint64_t alloc_frames(size_t count);

// kernel/memory/hhdm.cpp
void init_hhdm();
uint64_t get_physical_memory_offset();
void *phys_to_virt(uint64_t phys_addr);
void *virt_to_phys(uint64_t virt_addr);
void *driver_phys_to_virt(uint64_t phys_addr);
void *driver_virt_to_phys(uint64_t virt_addr);
uint64_t page_virt_to_phys(uint64_t va);

// kernel/memory/page.cpp
void handle_page_fault(struct X64_REGS *frame, uint64_t error_code);
void page_table_clear(page_table_t *table);
bool page_table_get_flags(page_directory_t *directory, uint64_t addr, uint64_t *out_flags);
bool page_table_update_flags(page_directory_t *directory, uint64_t addr, uint64_t new_flags);
page_table_t *page_table_create(page_table_entry_t *entry);
page_directory_t *get_kernel_pagedir();
static page_table_t *copy_page_table_recursive(page_table_t *source_table, int level, bool all_copy,
                                               bool kernel_space);
static void free_page_table_recursive(page_table_t *table, int level);
void unmap_page(page_directory_t *directory, uint64_t vaddr);
void unmap_page_range(page_directory_t *directory, uint64_t vaddr, uint64_t size);
void page_map_to(page_directory_t *directory, uint64_t addr, uint64_t frame, uint64_t flags);
uint64_t page_alloc_random(page_directory_t *directory, uint64_t length, uint64_t flags);
void page_map_range_to_random(page_directory_t *directory, uint64_t addr, uint64_t length,
                              uint64_t flags);
void page_map_range_to(page_directory_t *directory, uint64_t frame, uint64_t length,
                       uint64_t flags);
void page_map_range(page_directory_t *directory, uint64_t addr, uint64_t frame, uint64_t length,
                    uint64_t flags);
void page_setup();

/*
 *
 *  kernel/smp/
 * 
 */

#include <smp/smp.h>

// kernel/smp/smp.cpp
void init_smp(uint64_t MADT0);

#endif