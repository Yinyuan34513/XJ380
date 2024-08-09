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
                pixel_writer->Write(x, y, {0,162,232});  // #00a2e8 - 星际蓝
            }  
        }  
    }  

    draw_rect(fbc, 200, 200, 400, 400, {255, 242, 0});  // #FFF200 - 星际黄
  
    while (1) asm("hlt");  
}