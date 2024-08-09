// main.cpp  
#include "../include/include.h"  

extern "C" void KernelMain(const FrameBufferConfig &fbc) {  
    init_desktop(fbc);
    while (1) asm("hlt");  
}