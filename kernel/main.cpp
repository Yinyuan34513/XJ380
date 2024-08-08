// 版权所有©XINGJI Studios 2017-2024 保留所有权利。
// XJ380内核主程序（\kernel.elf）
#include "../include/include.h"

struct PixelColor {
    uint8_t r, g, b;
};

int WritePixel(const FrameBufferConfig &config, int x, int y, const PixelColor &c)
{
    const int pixel_position = config.pixels_per_scan_line * y + x;
    if (config.pixel_format == kRGBR) {
        uint8_t *p = &config.frame_buffer[4 * pixel_position]; // RGBR/BGRR模式下，一个像素占4个字节
        p[0] = c.r;
        p[1] = c.g;
        p[2] = c.b; // 按顺序写入，因为是RGBR
    } else if (config.pixel_format == kBGRR) {
        uint8_t *p = &config.frame_buffer[4 * pixel_position]; // RGBR/BGRR模式下，一个像素占4个字节
        p[0] = c.b;
        p[1] = c.g;
        p[2] = c.r; // 同样，按顺序写入
    } else {
        return -1; // 不应该出现
    }
    return 0; // 正常
}

extern "C" void KernelMain(const FrameBufferConfig& fbc) {
    for (int x = 0; x < fbc.horizontal_resolution; x++) {
        for (int y = 0; y < fbc.vertical_resolution; y++) {
            WritePixel(fbc, x, y, {0, 162, 232});
        }
    }
    while (1) asm("hlt");
}
