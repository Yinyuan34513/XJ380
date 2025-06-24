#ifndef _CPROTO_HPP_
#define _CPROTO_HPP_

/*
 *   lib
 */
// lib/string.c
char *strncpy(char *dest, const char *src, UINT64 length);
char *strcpy(char *dest, const char *src);
UINT64 strlen(const char *str);
UINT64 strcmp(char *from_str, char *cmp_str);
UINT64 part_strcmp(char *from_str, char *cmp_str, UINT64 size);

/*
 *   kernel
 */
// kernel/intr/apic.cpp
void init_apic(UINT64 MADT);

/*
 *   driver
 */

// kernel/cpu/msr.cpp
void GetMSR(UINT32 msr, UINT32 *lo, UINT32 *hi);
void SetMSR(UINT32 msr, UINT32 lo, UINT32 hi);

// kernel/asm/common.cpp
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

// lib/printf.cpp
int vsprintf(char *out, const char *fmt, __builtin_va_list ap);
int printf(const char *fmt, ...);

#endif
