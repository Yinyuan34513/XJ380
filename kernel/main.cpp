// main.cpp  
#include "../include/include.h"  
auto operator new(size_t size, void *ptr) -> void * {
  return ptr;
}

auto operator new[](size_t size, void *ptr) -> void * {
  return ptr;
}

auto operator delete(void *ptr, size_t size) -> void { 
}

auto operator delete[](void *ptr, size_t size) -> void { 
}

extern "C" void KernelMain(const FrameBufferConfig &fbc) {  
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
        for (int x = 0; x < fbc.horizontal_resolution; x++) {  
            for (int y = 0; y < fbc.vertical_resolution; y++) {  
                pixel_writer->Write(x, y, {0,162,232});  
            }  
        }  
  
        // 在屏幕中央绘制一个绿色方块  
        for (int x = 0; x < 200; x++) {  
            for (int y = 0; y < 100; y++) {  
                pixel_writer->Write(fbc.horizontal_resolution / 2 - 100 + x, fbc.vertical_resolution / 2 - 50 + y, {0, 255, 0});  
            }  
        }  
    }  
  
    while (1) asm("hlt");  
}