#ifndef _DEFINE_CODE_HPP_
#define _DEFINE_CODE_HPP_

#include"efi.h"

// graphics/draw.cpp
void rect(const FrameBufferConfig &fbc, int x1, int y1, int x2, int y2, const PixelColor& c);
void dot(const FrameBufferConfig &fbc, int x, int y, const PixelColor& c);
void line(const FrameBufferConfig &fbc, int x0, int y0, int x1, int y1, const PixelColor& c);
void circle(const FrameBufferConfig &fbc,int centerX, int centerY, int radius,int solid,const PixelColor& c);
EFI_STATUS ShowBMP(struct BMP_PIXEL* pixels, struct BITMAPINFOHEADER* infoHeader, const FrameBufferConfig& fbc);
EFI_STATUS LoadBMPFile(CHAR16* filename, struct BITMAPINFOHEADER* infoHeader, struct BMP_PIXEL** pixels);
// graphics/desktop.cpp
void init_desktop(const FrameBufferConfig &fbc);
void init_dock(const FrameBufferConfig &fbc);
void init_shortcut_dock(const FrameBufferConfig &fbc);
void draw_logo(const FrameBufferConfig &fbc, int xi, int yi);
void draw_studio_logo(const FrameBufferConfig &fbc, int xi, int yi);

// font/font.cpp
void WriteAscii(const FrameBufferConfig &fbc, int x, int y, char ch, const PixelColor &c);
void WriteString(const FrameBufferConfig &fbc, int x, int y, const char *s, const PixelColor &c);

#endif