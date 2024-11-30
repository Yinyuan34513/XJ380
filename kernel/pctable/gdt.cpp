#include "../../include/include.h"

/* 全局描述符表定义 */
GDT_ENTRY gdt_entries[GDT_LENGTH];

/* GDTR */
GDT_PTR gdt_ptr;

static void gdt_set_gate(INT64 num, UINT64 base, UINT64 limit, UINT8 access, UINT8 sign);

/* 声明内核栈地址 */
extern "C" UINT64 stack;

/* 初始化全局描述符表 */
void init_gdt()
{
	gdt_ptr.limit	= sizeof(GDT_ENTRY) * GDT_LENGTH - 1;
	gdt_ptr.base	= (UINT64)&gdt_entries;

	// 采用 Intel 平坦模型
	// 0xAF = 10101111
	gdt_set_gate(GDT_TYPE_ZERO, 0, 0, 0, 0);						// 按照 Intel 文档要求，第一个描述符必须全 0
	gdt_set_gate(GDT_TYPE_KERNEL_CS, 0, 0xFFFFF, 0x9A, 0xAF);	// 代码段 0x9A = 10011010
	gdt_set_gate(GDT_TYPE_KERNEL_DS, 0, 0xFFFFF, 0x92, 0xAF);	// 数据段 0x92 = 10010010
	gdt_set_gate(GDT_TYPE_USER_CS, 0, 0xFFFFF, 0xFA, 0xAF);		// 用户模式代码段 0xFA = 11111010
	gdt_set_gate(GDT_TYPE_USER_DS, 0, 0xFFFFF, 0xF2, 0xAF);		// 用户模式数据段 0xF2 = 11110010

	__asm__ volatile ("lgdt %0" : : "m"(gdt_ptr));
	asm volatile (
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
		: [SELECTOR_CODE64]"i"( SELECTOR_KERNEL_CS ),
		  [SELECTOR_DATA64]"a"( SELECTOR_KERNEL_DS )
	: );
}

// 热知识：64位下，基地址和限长填了也没用
static void gdt_set_gate(INT64 num, UINT64 base, UINT64 limit, UINT8 access, UINT8 sign)
{
	gdt_entries[num].base_low		= (base & 0xFFFF);
	gdt_entries[num].base_middle	= (base >> 16) & 0xFF;
	gdt_entries[num].base_high		= (base >> 24) & 0xFF;

	gdt_entries[num].limit_low		= (limit & 0xFFFF);
	gdt_entries[num].granularity	= (limit >> 16) & 0x0F;

	gdt_entries[num].granularity	|= sign & 0xF0;
	gdt_entries[num].access			= access;
}