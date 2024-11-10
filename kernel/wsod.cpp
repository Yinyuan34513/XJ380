#include "../include/include.h"

// 预期外的中断
void WSOD_UNEXPECT_INTRRUPT() {
    while (1) { 
        disable_intr();
        __asm__("hlt"); 
    };
}
/*
void WSOD_UNEXPECT_INTRRUPT(SHEET_INFO *sht) {
    int scrx = sht->scrx;
    int scry = sht->scry;
    draw_rect(sht, 255, 0, 0, scrx, scry, WHITE);
    PrintString(sht, 255, 8, 8, "XJ380 has encountered a problem. Please visit https://xingjisoft.top/product/os/xj380/error.html.", BLACK);
    PrintString(sht, 255, 24, "Error code: UNEXPECT_INTRRUPT", BLACK);
    PrintString(sht, 255, scrx - 424, scry - 24, "XINGJI Interactive Software - XJ380 Operating System", BLACK);
    while (1) { 
        disable_intr();
        __asm__("hlt"); 
    };
}
*/
