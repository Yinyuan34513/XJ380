#include "include.h"

PUBLIC void init_screen()
{
    draw_rect(0, 0, binfo->scrnx - 1, binfo->scrny - 1, 0x44cef6);
    draw_rect(0, binfo->scrny - 28, binfo->scrnx - 1, binfo->scrny - 28, 0xc6c6c6);
    draw_rect(0, binfo->scrny - 27, binfo->scrnx - 1, binfo->scrny - 27, 0xffffff);
    draw_rect(0, binfo->scrny - 26, binfo->scrnx - 1, binfo->scrny - 1,  0xc6c6c6);
}