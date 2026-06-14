#include <x3api.h>
#include <krlibc.h>
#include "cm_proto.h"

void process_left_key(int x, int y)
{
    if (y > 60 && y < 120)
    {
        if (x > 280 && x < 386) { cindex = 1; }
        else if (x > 386 && x < 544) { cindex = 2; }
        else if (x > 544 && x < 670) { cindex = 3; }
        draw_background();
    }
    if (cindex == 1)
    {
        ctrlmenu_handle_mainpage_click(x, y);
    }
    else if (cindex == 2)
    {
        ctrlmenu_handle_app_showcase_click(x, y);
    }
    else if (cindex == 3)
    {
        if (x < (int)(win_width / 10 * 3))
        {
            if (y > 128 && y < 158) { setting_cindex = 1; }
            if (y > 158 && y < 196) { setting_cindex = 2; }
            if (y > 196 && y < 234) { setting_cindex = 3; }
            if (y > 242 && y < 272) { setting_cindex = 4; }
            if (y > 280 && y < 310) { setting_cindex = 5; }
            if (y > 318 && y < 348) { setting_cindex = 6; }
            draw_background_body();
        }
        else if (setting_cindex == 1)
        {
            int memory_x1 = win_width / 10 * 3 + 402;
            int memory_y1 = 248;
            int memory_x2 = memory_x1 + 120;
            int memory_y2 = memory_y1 + 20;

            if (x >= memory_x1 && x <= memory_x2 &&
                y >= memory_y1 && y <= memory_y2)
            {
                about_memory_show_mb = !about_memory_show_mb;
                draw_background_body();
            }
        }
    }
}
