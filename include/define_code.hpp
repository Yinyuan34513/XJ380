#ifndef _DEFINE_CODE_HPP_
#define _DEFINE_CODE_HPP_

// graphics/draw.cpp
void rect(const FrameBufferConfig &fbc, int x1, int y1, int x2, int y2, const PixelColor& c);
void dot(const FrameBufferConfig &fbc, int x, int y, const PixelColor& c);
void line(const FrameBufferConfig &fbc, int x0, int y0, int x1, int y1, const PixelColor& c);
void circle(const FrameBufferConfig &fbc,int centerX, int centerY, int radius,int solid,const PixelColor& c);

// graphics/desktop.cpp
void init_desktop(const FrameBufferConfig &fbc);
void init_dock(const FrameBufferConfig &fbc);

#endif