#include "../include/include.h"


void draw_rect(const FrameBufferConfig &fbc, int x1, int y1, int x2, int y2, const PixelColor& c)
{
    static char pixel_writer_buf[sizeof(RGBRWriter) > sizeof(BGRRWriter) ? sizeof(RGBRWriter) : sizeof(BGRRWriter)];  
    static PixelWriter* pixel_writer = nullptr;  
  
    switch (fbc.pixel_format) {  
        case PixelFormat::kRGBR:  
            pixel_writer = new(pixel_writer_buf) RGBRWriter(fbc); // 使用 placement new  
            break;  
        case PixelFormat::kBGRR:  
            pixel_writer = new(pixel_writer_buf) BGRRWriter(fbc);  
            break;  
        // ... 处理其他像素格式 ...  
        default:  
            // 错误处理  
            break;  
    }  
  
    if (pixel_writer != nullptr) {  
        for (int x = x1; x < x2; x++) {  
            for (int y = y1; y < y2; y++) {  
                pixel_writer->Write(x, y, c);
            }  
        }  
    }  
    return;
}
