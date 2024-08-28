#include "../include/include.h"

void WSOD_Cannot_Alloc_MemoryMap(const FrameBufferConfig &fbc) {
    int scrx = fbc.horizontal_resolution;
    int scry = fbc.vertical_resolution;
    rect(fbc, 0, 0, scrx, scry, WHITE);
    WriteString(fbc, 8, 8, "XJ380 has encountered a problem. Please visit https://xingjisoft.top/product/os/xj380/error.html.", BLACK);
    WriteString(fbc, 8, 24, "Error code: CANNOT_ALLOC_MEMORYMAP", BLACK);
    WriteString(fbc, scrx - 424, scry - 24, "XINGJI Interactive Software - XJ380 Operating System", BLACK);
}


void WSOD_Cannot_Alloc_MemoryMap_2(const FrameBufferConfig &fbc) {
    int scrx = fbc.horizontal_resolution;
    int scry = fbc.vertical_resolution;
    rect(fbc, 0, 0, scrx, scry, WHITE);
    WriteString(fbc, 8, 8, "XJ380 has encountered a problem. Please visit https://xingjisoft.top/product/os/xj380/error.html.", BLACK);
    WriteString(fbc, 8, 24, "Error code: CANNOT_ALLOC_MEMORYMAP_2", BLACK);
    WriteString(fbc, scrx - 424, scry - 24, "XINGJI Interactive Software - XJ380 Operating System", BLACK);
}