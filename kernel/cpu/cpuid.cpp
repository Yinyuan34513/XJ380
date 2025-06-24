#include "../../include/include.h"

/**
 *
 * @brief CPUID
 *
 * @param mop 格式（0=CPU厂商 1=CPU参数）
 *
 */
void asm_cpuid(UINT32 mop, UINT32 sop, UINT32 *eax, UINT32 *ebx, UINT32 *ecx, UINT32 *edx)
{
	__asm__ volatile("cpuid \n\t" : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx) : "0"(mop), "2"(sop) : "memory");
}

/**
 * @brief 获取CPU型号
 *
 * @param c 用于存储型号的字符串
 */
void get_cpu_name(char *c)
{
	UINT32 *v = (UINT32 *)(c);
	asm_cpuid(0x80000002, 0, &v[0], &v[1], &v[2], &v[3]);
	asm_cpuid(0x80000003, 0, &v[4], &v[5], &v[6], &v[7]);
	asm_cpuid(0x80000004, 0, &v[8], &v[9], &v[10], &v[11]);
	c[47] = '\0';
}