// main.cpp  
#include "../include/include.h"  

extern "C" void KernelMain(const FrameBufferConfig &fbc) {  
    init_desktop(fbc);

    BMP_PIXEL* pixels = nullptr;
    BITMAPINFOHEADER infoHeader;
    CHAR16 filename[] = L"\\EFI\\BOOT\\test.bmp";
    LoadBMPFile(filename, &infoHeader, &pixels);
    // 计算屏幕中心的位置
    UINT32 offsetX = (fbc.Width - infoHeader.biWidth) / 2;
    UINT32 offsetY = (fbc.Height - infoHeader.biHeight) / 2;
    // 调整显示位置
    struct BITMAPINFOHEADER adjustedInfoHeader = infoHeader;
    adjustedInfoHeader.biWidth += offsetX;
    adjustedInfoHeader.biHeight += offsetY;
    ShowBMP(pixels, &adjustedInfoHeader, fbc);

    WriteString(fbc, fbc.horizontal_resolution/2-(16*9), fbc.vertical_resolution/2, "Welcome to Xj380!!!", {255, 255, 255}); 
    while (1) asm("hlt");  
}