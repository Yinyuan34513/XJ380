#include "include.h"

PUBLIC void init_screen()   //绘制桌面
{
    draw_rect(0, 0, binfo->scrnx - 1, binfo->scrny - 1, 0x44cef6);
    draw_rect(0, binfo->scrny - 28, binfo->scrnx - 1, binfo->scrny - 1,  0xc6c6c6);
}