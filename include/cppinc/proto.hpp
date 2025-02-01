#ifndef _PROTO_HPP_
#define _PROTO_HPP_

#include "../include.h"
#include "../efi.h"
#include "../global/kernel/apic/apic.h"

/*
 *   graphics/*
 */

// graphics/draw.cpp
auto operator new(size_t size, void *ptr) -> void *;
auto operator new[](size_t size, void *ptr) -> void *;
auto operator delete(void *ptr, size_t size) -> void;
auto operator delete[](void *ptr, size_t size) -> void;
void rect(const FrameBufferConfig &fbc, int x1, int y1, int x2, int y2, const PixelColor &c);
void dot(const FrameBufferConfig &fbc, int x, int y, const PixelColor &c);
void line(const FrameBufferConfig &fbc, int x0, int y0, int x1, int y1, const PixelColor &c);
void circle(const FrameBufferConfig &fbc, int centerX, int centerY, int radius, int solid, const PixelColor &c);
void drawtime(EFI_SYSTEM_TABLE &table, const FrameBufferConfig &fbc, const int x, const int y, float UTC);

// graphics/draw_sheet.cpp
void draw_point(SHEET_INFO *sht, UINT16 *sheet_id, int x, int y, const SHEET_BUFFER &color);
void draw_rect(SHEET_INFO *sht, UINT16 *sheet_id, int x1, int y1, int x2, int y2, const SHEET_BUFFER &color);
void draw_line(SHEET_INFO *sht, UINT16 *sheet_id, int x0, int y0, int x1, int y1, const SHEET_BUFFER &color);

// graphics/desktop.cpp
void init_desktop(SHEET_INFO *sht, UINT16 *sheet_id, UINT16 *dock_sheet_id);
void init_dock(SHEET_INFO *sht, UINT16 *sheet_id);
void init_shortcut_dock(SHEET_INFO *sht, UINT16 *sheet_id);
void draw_logo(SHEET_INFO *sht, UINT16 *sheet_id, int xi, int yi);
void draw_studio_logo(SHEET_INFO *sht, UINT16 *sheet_id, int xi, int yi);

/*
 *   font/*
 */

// font/font.cpp
void WriteAscii(const FrameBufferConfig &fbc, int x, int y, char ch, const PixelColor &c);
void WriteDec(const FrameBufferConfig &fbc, int x, int y, unsigned long long dec, const PixelColor &c);
void WriteString(const FrameBufferConfig &fbc, int x, int y, const char *s, const PixelColor &c);
void WriteHex(const FrameBufferConfig &fbc, int x, int y, unsigned long long hex, const PixelColor &c);
void PrintFont(SHEET_INFO *sht, UINT16 *sheet_id, int x, int y, char ch, const SHEET_BUFFER &color);
void PrintString(SHEET_INFO *sht, UINT16 *sheet_id, int x, int y, const char *s, const SHEET_BUFFER &color);
void PrintHex(SHEET_INFO *sht, UINT16 *sheet_id, int x, int y, unsigned long long hex, const SHEET_BUFFER &color);
void PrintDec(SHEET_INFO *sht, UINT16 *sheet_id, int x, int y, unsigned long long dec, const SHEET_BUFFER &color);

/*
 *   lib/*
 */
// lib/string.c
char *strncpy(char *dest, const char *src, UINT64 length);
char *strcpy(char *dest, const char *src);
UINT64 strlen(const char *str);
UINT64 strcmp(char *from_str, char *cmp_str);
UINT64 part_strcmp(char *from_str, char *cmp_str, UINT64 size);
char *Dec2Char(unsigned long long dec);
char *Hex2Char(unsigned long long hex);

/*
 *   driver/*
 */
// driver/serial/serial_port.cpp
int init_serial();
int is_transmit_empty();
void write_serial(char a);
void write_serial_string(const char *str);
void write_serial_dec(unsigned long long dec);
void write_serial_hex(unsigned long long hex);

// driver/ps2/keyboard.cpp
extern struct keyboard_buf kb_fifo;
extern UINT8 keyboard_code[256];
extern UINT8 keyboard_code1[256];
void keyboard_init();
UINT8 get_keyboard_input();

// driver/ps2/mouse.cpp
void Mouse_init();
UINT8 get_Mouse_output();
bool mousedecode(UINT8 data);
int get_mouse_x();
int get_mouse_y();
void draw_mouse(SHEET_INFO *sht, UINT16 *sheet_id);
void move_mouse(SHEET_INFO *sht, UINT16 *sheet_id, int px, int py);

/*
 *   kernel/*
 */
// kernel/main.cpp
extern const FrameBufferConfig *fbc_addr;

// kernel/wsod.cpp
// 异常处理
extern "C" void wsod_unexpect_intrrupt(struct X64_REGS *regs, UINT64 error_code);
extern "C" void wsod_debug(struct X64_REGS *regs, UINT64 error_code);
extern "C" void wsod_nmi(struct X64_REGS *regs, UINT64 error_code);
extern "C" void wsod_double_fault(struct X64_REGS *regs, UINT64 error_code);
extern "C" void wsod_system_kernel_error(struct X64_REGS *regs, UINT64 error_code);
extern "C" void wsod_general_protection(struct X64_REGS *regs, UINT64 error_code);
extern "C" void wsod_page_fault(struct X64_REGS *regs, UINT64 error_code);

// APIC初始化错误处理
void wsod_apic_not_found();

// kernel/memorym.cpp
void init_MemoryManager(const FrameBufferConfig &fbc, BOOT_CONFIG *BootConfig);

// kernel/sheet.cpp
void init_sheet(const FrameBufferConfig &fbc, SHEET_INFO *sht);
void create_sheet(SHEET_INFO *sht, UINT32 bx, UINT32 by, UINT32 x, UINT32 y, UINT32 type, INT16 index, UINT16 *sheet_id);
UINT32 getBX(SHEET_INFO *sht, UINT16 *sheet_id);
UINT32 getBY(SHEET_INFO *sht, UINT16 *sheet_id);
UINT32 getXsize(SHEET_INFO *sht, UINT16 *sheet_id);
UINT32 getYsize(SHEET_INFO *sht, UINT16 *sheet_id);
void setBX(SHEET_INFO *sht, UINT16 *sheet_id, UINT32 num);
void setBY(SHEET_INFO *sht, UINT16 *sheet_id, UINT32 num);
void delete_sheet(SHEET_INFO *sht, UINT16 *sheet_id);
void refresh_sheet(SHEET_INFO *sht);
void refresh_part_sheet(SHEET_INFO *sht, int bex1, int bey1, int bex2, int bey2);
UINT32 get_hor();
UINT32 get_ver();

// kernel/intr/apic.cpp
void init_apic(UINT64 MADT);
void send_eoi();

// kernel/intr/hpet.cpp
void init_hpet(UINT64 hpet);
UINT64 nanoTime();

// kernel/cpu/msr.cpp
void GetMSR(UINT32 msr, UINT32 *lo, UINT32 *hi);
void SetMSR(UINT32 msr, UINT32 lo, UINT32 hi);

// kernel/cpu/cpuid.cpp
void asm_cpuid(UINT32 mop, UINT32 sop, UINT32 *eax, UINT32 *ebx, UINT32 *ecx, UINT32 *edx);
void get_cpu_name(char *c);

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

// kernel/intr/8259A.cpp
void init_8259A();
void disable_8259A();

// kernel/pctable/gdt.cpp
void init_gdt();

// kernel/pctable/idt.cpp
void init_idt();
void init_idt_desc(UINT8 number, UINT8 gate_type, int_func function, UINT8 privilege);

// lib/printf.cpp
int vsprintf(char *out, const char *fmt, __builtin_va_list ap);
int printf(const char *fmt, ...);

#endif