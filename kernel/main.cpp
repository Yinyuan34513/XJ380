// main.cpp  
#include "../include/include.h"  

extern "C" void KernelMain(const FrameBufferConfig &fbc) {  
    init_desktop(fbc);
    WriteString(fbc, fbc.horizontal_resolution/2-(16*9), fbc.vertical_resolution/2, "Welcome to Xj380!!!", {255, 255, 255}); // 在(0,66)的位置，用蓝色打印Hello World
    while (1) asm("hlt");  
}