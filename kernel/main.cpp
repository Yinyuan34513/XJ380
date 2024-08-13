// main.cpp  
#include "../include/include.h"  
#include "../include/efi.h"


extern "C" void KernelMain(const FrameBufferConfig &fbc) {  
    init_desktop(fbc);
    WriteString(fbc, fbc.horizontal_resolution/2-(16*9), fbc.vertical_resolution/2, "Welcome to Xj380!!!", {255, 255, 255}); 
    while (1) asm("hlt");  
}