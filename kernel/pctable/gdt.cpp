#if defined (__x86_64__) || defined (__i386__)

#include <stdint.h>
#include <pctable/gdt.h>
#include <mm/memory.h>
#include <proto.h>

/* 全局描述符表定义 */
GDT_ENTRY gdt_entries[GDT_LENGTH] __attribute__((__aligned__(PAGE_SIZE)));

/* GDTR */
GDT_PTR gdt_ptr;

static struct GDT_ENTRY gdt_descs[] = {
    {0},

    {
        .limit       = 0xffff,
        .base_low    = 0x0000,
        .base_mid    = 0x00,
        .access      = 0b10011011,
        .granularity = 0b00000000,
        .base_hi     = 0x00
    },

    {
        .limit       = 0xffff,
        .base_low    = 0x0000,
        .base_mid    = 0x00,
        .access      = 0b10010011,
        .granularity = 0b00000000,
        .base_hi     = 0x00
    },

    {
        .limit       = 0xffff,
        .base_low    = 0x0000,
        .base_mid    = 0x00,
        .access      = 0b10011011,
        .granularity = 0b11001111,
        .base_hi     = 0x00
    },

    {
        .limit       = 0xffff,
        .base_low    = 0x0000,
        .base_mid    = 0x00,
        .access      = 0b10010011,
        .granularity = 0b11001111,
        .base_hi     = 0x00
    },

    {
        .limit       = 0x0000,
        .base_low    = 0x0000,
        .base_mid    = 0x00,
        .access      = 0b10011011,
        .granularity = 0b00100000,
        .base_hi     = 0x00
    },

    {
        .limit       = 0x0000,
        .base_low    = 0x0000,
        .base_mid    = 0x00,
        .access      = 0b10010011,
        .granularity = 0b00000000,
        .base_hi     = 0x00
    }
};

void init_gdt(void) {
    gdt_ptr.limit = sizeof(GDT_ENTRY) * GDT_LENGTH - 1;
	gdt_ptr.base = (UINT64)&gdt_entries;
    xmemcpy(gdt_entries, gdt_descs, sizeof(gdt_descs));

    __asm__ volatile("lgdt %0" : : "m"(gdt_ptr));
	__asm__ volatile(
		"movq %%rax, %%ds \n\t"
		"movq %%rax, %%es \n\t"
		"movq %%rax, %%fs \n\t"
		"movq %%rax, %%gs \n\t"
		"movq %%rax, %%ss \n\t"
		"pushq %[SELECTOR_CODE64] \n\t"
		"leaq .next(%%RIP),%%rax \n\t"
		"pushq %%rax \n\t"
		"lretq \n\r"
		".next: \n\t"
		:
		: [SELECTOR_CODE64] "i"(SELECTOR_KERNEL_CS),
		  [SELECTOR_DATA64] "a"(SELECTOR_KERNEL_DS)
		:);

	write_serial_string("GDT Initialize Success.\n");
}

#endif