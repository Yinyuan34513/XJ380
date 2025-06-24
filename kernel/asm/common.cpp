#include "../../include/include.h"

// 端口写（8位）
void outb(UINT16 port, UINT8 value)
{
	asm volatile("outb %1, %0" : : "dN"(port), "a"(value));
}

// 端口读（8位）
UINT8 inb(UINT16 port)
{
	UINT8 ret;
	asm volatile("inb %1, %0" : "=a"(ret) : "dN"(port));
	return ret;
}

// 端口写（16位）
void outw(UINT16 port, UINT16 value)
{
	asm volatile("outw %1, %0" : : "dN"(port), "a"(value));
}

// 端口读（16位）
UINT16 inw(UINT16 port)
{
	UINT16 ret;
	asm volatile("inw %1, %0" : "=a"(ret) : "dN"(port));
	return ret;
}

// 端口写（32位）
void outl(UINT16 port, UINT32 value)
{
	asm volatile("outl %1, %0" : : "dN"(port), "a"(value));
}

// 端口读（32位）
UINT32 inl(UINT16 port)
{
	UINT32 ret;
	asm volatile("inl %1, %0" : "=a"(ret) : "dN"(port));
	return ret;
}

// 从I/O端口批量地读取数据到内存（16位）
void insw(UINT16 port, VOID *buf, unsigned long n)
{
	asm volatile("cld; rep; insw"
				 : "+D"(buf),
				   "+c"(n)
				 : "d"(port));
}

// 从内存批量地写入数据到I/O端口（16位）
void outsw(UINT16 port, const VOID *buf, unsigned long n)
{
	asm volatile("cld; rep; outsw"
				 : "+S"(buf),
				   "+c"(n)
				 : "d"(port));
}

// 从I/O端口批量地读取数据到内存（32位）
void insl(UINT32 port, VOID *addr, int cnt)
{
	asm volatile("cld;"
				 "repne; insl;"
				 : "=D"(addr), "=c"(cnt)
				 : "d"(port), "0"(addr), "1"(cnt)
				 : "memory", "cc");
}

// 从内存批量地写入数据到I/O端口（32位）
void outsl(UINT32 port, const VOID *addr, int cnt)
{
	asm volatile("cld;"
				 "repne; outsl;"
				 : "=S"(addr), "=c"(cnt)
				 : "d"(port), "0"(addr), "1"(cnt)
				 : "memory", "cc");
}

// 复活中断
void enable_intr()
{
	asm volatile("sti");
}

// 谋害中断
void disable_intr()
{
	asm volatile("cli" ::: "memory");
}

// MFENCE
void io_mfence()
{
	__asm__ volatile("mfence	\n\t" ::: "memory");
}
