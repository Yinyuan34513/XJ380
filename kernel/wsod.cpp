#include "../include/include.h"

// 预期外的中断
void wsod_unexpect_intrrupt() {
    int scrx = shtinf.scrx;
    int scry = shtinf.scry;
    UINT16 temp_sheet = 255;
    draw_rect(&shtinf, &temp_sheet, 0, 0, scrx, scry, WHITE);
    PrintString(&shtinf, &temp_sheet, 8, 8, "XJ380 has encountered a problem. Please visit https://xingjisoft.top/product/os/xj380/error.html.", BLACK);
    PrintString(&shtinf, &temp_sheet, 24, 8, "Error code: UNEXPECT_INTRRUPT", BLACK);
    PrintString(&shtinf, &temp_sheet, scrx - 424, scry - 24, "XINGJI Interactive Software - XJ380 Operating System", BLACK);
    while (1) { 
        disable_intr();
        __asm__("hlt"); 
    };
}
