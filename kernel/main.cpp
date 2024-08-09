// main.cpp  
#include "../include/include.h"  

extern "C" void KernelMain(const FrameBufferConfig &fbc) {  
    rect(fbc, 0, 0, fbc.horizontal_resolution, fbc.vertical_resolution, {0,162,232});  // #FFF200 - 星际黄
    rect(fbc, 200, 200, 400, 400, {255, 242, 0});  // #FFF200 - 星际黄
    line(fbc,0,0,fbc.horizontal_resolution, fbc.vertical_resolution,{0,255,0});
    circle(fbc,fbc.horizontal_resolution/2+100,fbc.vertical_resolution/2,100,1,{255,0,0});
    while (1) asm("hlt");  
}