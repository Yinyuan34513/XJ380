#ifndef _PROTO_HPP_
#define _PROTO_HPP_

#include "../include.h"

#include"../efi.h"

// graphics/draw.cpp
auto operator new(size_t size, void *ptr) -> void *;
auto operator new[](size_t size, void *ptr) -> void *;
auto operator delete(void *ptr, size_t size) -> void;
auto operator delete[](void *ptr, size_t size) -> void;
void rect(const FrameBufferConfig &fbc, int x1, int y1, int x2, int y2, const PixelColor& c);
void dot(const FrameBufferConfig &fbc, int x, int y, const PixelColor& c);
void line(const FrameBufferConfig &fbc, int x0, int y0, int x1, int y1, const PixelColor& c);
void circle(const FrameBufferConfig &fbc,int centerX, int centerY, int radius,int solid,const PixelColor& c);

// graphics/draw_sheet.cpp
//void draw_point(struct SHEET_INFO &sht, UINT16* sheet_id, int x, int y, const SHEET_BUFFER& color);

// graphics/desktop.cpp
void init_desktop(const FrameBufferConfig &fbc);
void init_dock(const FrameBufferConfig &fbc);
void init_shortcut_dock(const FrameBufferConfig &fbc);
void draw_logo(const FrameBufferConfig &fbc, int xi, int yi);
void draw_studio_logo(const FrameBufferConfig &fbc, int xi, int yi);

// font/font.cpp
void WriteAscii(const FrameBufferConfig &fbc, int x, int y, char ch, const PixelColor &c);
void WriteDec(const FrameBufferConfig &fbc, int x, int y, unsigned long long dec, const PixelColor &c);
void WriteString(const FrameBufferConfig &fbc, int x, int y, const char *s, const PixelColor &c);
void WriteHex(const FrameBufferConfig &fbc, int x, int y, unsigned long long hex, const PixelColor &c);

// kernel/wsod.cpp
//void WSOD_Cannot_Alloc_MemoryMap(const FrameBufferConfig &fbc);

// kernel/memorym.cpp
void init_MemoryManager(const FrameBufferConfig &fbc, BOOT_CONFIG *BootConfig);

// kernel/sheet.cpp
void init_sheet(const FrameBufferConfig &fbc, SHEET_INFO *sht);
/*
void create_sheet(struct SHEET_INFO &sht, UINT32 bx, UINT32 by, UINT32 x, UINT32 y, UINT32 type, INT16 index, UINT16* sheet_id);
void delete_sheet(struct SHEET_INFO &sht, UINT16* sheet_id);
void refresh_sheet(struct SHEET_INFO &sht);
*/

#endif