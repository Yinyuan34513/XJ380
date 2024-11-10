#include "../include/include.h"

/* 全局描述符表长度 */
#define GDT_LENGTH 5
#define kernel_data_selector 0x10

/* 全局描述符表定义 */
GDT_ENTRY gdt_entries[GDT_LENGTH];

/* GDTR */
GDT_PTR gdt_ptr;

static void gdt_set_gate(INT64 num, UINT64 base, UINT64 limit, UINT8 access, UINT8 gran);

/* 声明内核栈地址 */
extern "C" UINT64 stack;

/* 初始化全局描述符表 */
void init_gdt()
{
	// 全局描述符表界限 e.g. 从 0 开始，所以总长要 - 1
	gdt_ptr.limit	= sizeof(GDT_ENTRY) * GDT_LENGTH - 1;
	gdt_ptr.base	= (UINT64)&gdt_entries;

	// 采用 Intel 平坦模型
	gdt_set_gate(0, 0, 0, 0, 0);				// 按照 Intel 文档要求，第一个描述符必须全 0
	gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);	// 指令段
	gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);	// 数据段
	gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);	// 用户模式代码段
	gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);	// 用户模式数据段

	asm volatile (
		"lgdt [gdt_ptr]\n"
		:
		: [gdt_ptr] "*p" (&gdt_ptr)
	);
	asm volatile (
		"mov %[dsel], %%ds\n"
		"mov %[dsel], %%fs\n"
		"mov %[dsel], %%gs\n"
		"mov %[dsel], %%es\n"
		"mov %[dsel], %%ss\n"
		:
		: [dsel] "rm" (kernel_data_selector)
	);
}

// 全局描述符表构造函数，根据下标构造
// 参数 数组下标、基地址、限长、访问标志，其它访问标志
static void gdt_set_gate(INT64 num, UINT64 base, UINT64 limit, UINT8 access, UINT8 gran)
{
	gdt_entries[num].base_low		= (base & 0xFFFF);
	gdt_entries[num].base_middle	= (base >> 16) & 0xFF;
	gdt_entries[num].base_high		= (base >> 24) & 0xFF;

	gdt_entries[num].limit_low		= (limit & 0xFFFF);
	gdt_entries[num].granularity	= (limit >> 16) & 0x0F;

	gdt_entries[num].granularity	|= gran & 0xF0;
	gdt_entries[num].access			= access;
}