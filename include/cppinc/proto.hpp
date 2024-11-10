#ifndef _PROTO_HPP_
#define _PROTO_HPP_

#include "../include.h"

#include"../efi.h"



/*
*   graphics/*
*/

// graphics/draw.cpp
auto operator new(size_t size, void *ptr) -> void *;
auto operator new[](size_t size, void *ptr) -> void *;
auto operator delete(void *ptr, size_t size) -> void;
auto operator delete[](void *ptr, size_t size) -> void;
void rect(const FrameBufferConfig &fbc, int x1, int y1, int x2, int y2, const PixelColor& c);
void dot(const FrameBufferConfig &fbc, int x, int y, const PixelColor& c);
void line(const FrameBufferConfig &fbc, int x0, int y0, int x1, int y1, const PixelColor& c);
void circle(const FrameBufferConfig &fbc,int centerX, int centerY, int radius,int solid,const PixelColor& c);
void drawtime(EFI_SYSTEM_TABLE &table, const FrameBufferConfig &fbc,const int x,const int y,float UTC);

// graphics/draw_sheet.cpp
void draw_point(SHEET_INFO *sht, UINT16* sheet_id, int x, int y, const SHEET_BUFFER &color);
void draw_rect(SHEET_INFO *sht, UINT16* sheet_id, int x1, int y1, int x2, int y2, const SHEET_BUFFER &color);
void draw_line(SHEET_INFO *sht, UINT16* sheet_id, int x0, int y0, int x1, int y1, const SHEET_BUFFER &color);

// graphics/desktop.cpp
void init_desktop(SHEET_INFO *sht, UINT16* sheet_id, UINT16* dock_sheet_id);
void init_dock(SHEET_INFO *sht, UINT16* sheet_id);
void init_shortcut_dock(SHEET_INFO *sht, UINT16* sheet_id);
void draw_logo(SHEET_INFO *sht, UINT16* sheet_id, int xi, int yi);
void draw_studio_logo(SHEET_INFO *sht, UINT16* sheet_id, int xi, int yi);

// graphics/mouse.cpp
void draw_mouse(SHEET_INFO *sht, UINT16* sheet_id);
void move_mouse(SHEET_INFO *sht, UINT16* sheet_id, int px, int py);



/*
*   font/*
*/

// font/font.cpp
void WriteAscii(const FrameBufferConfig &fbc, int x, int y, char ch, const PixelColor &c);
void WriteDec(const FrameBufferConfig &fbc, int x, int y, unsigned long long dec, const PixelColor &c);
void WriteString(const FrameBufferConfig &fbc, int x, int y, const char *s, const PixelColor &c);
void WriteHex(const FrameBufferConfig &fbc, int x, int y, unsigned long long hex, const PixelColor &c);
void PrintFont(SHEET_INFO *sht, UINT16* sheet_id, int x, int y, char ch, const SHEET_BUFFER &color);
void PrintString(SHEET_INFO *sht, UINT16* sheet_id, int x, int y, const char *s, const SHEET_BUFFER &color);
void PrintHex(SHEET_INFO *sht, UINT16* sheet_id, int x, int y, unsigned long long hex, const SHEET_BUFFER &color);
void PrintDec(SHEET_INFO *sht, UINT16* sheet_id, int x, int y, unsigned long long dec, const SHEET_BUFFER &color);



/*
*   kernel/*
*/
// kernel/wsod.cpp
//void WSOD_UNEXPECT_INTRRUPT(SHEET_INFO *sht);
void WSOD_UNEXPECT_INTRRUPT();

// kernel/memorym.cpp
void init_MemoryManager(const FrameBufferConfig &fbc, BOOT_CONFIG *BootConfig);

// kernel/sheet.cpp
void init_sheet(const FrameBufferConfig &fbc, SHEET_INFO *sht);
void create_sheet(SHEET_INFO *sht, UINT32 bx, UINT32 by, UINT32 x, UINT32 y, UINT32 type, INT16 index, UINT16* sheet_id);
void delete_sheet(SHEET_INFO *sht, UINT16* sheet_id);
void refresh_sheet(SHEET_INFO *sht);
void refresh_part_sheet(SHEET_INFO *sht, int bex1, int bey1, int bex2, int bey2);

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

// kernel/pctable/gdt.cpp
void init_gdt();

// kernel/pctable/idt.cpp
void init_idt();
void init_idt_desc(UINT8 number, UINT8 gate_type, int_func function, UINT8 privilege);
void init_interruptions();

#endif