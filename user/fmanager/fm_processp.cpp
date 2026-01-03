#include <x3api.h>
#include <krlibc.h>
#include "fm_proto.h"

bool first_click = false;
bool double_click = false;
UINT64 clicktime_recorder = 0;

int click_x = 0;
int click_y = 0;

void register_click(int x, int y)
{
    click_x = x;
    click_y = y;
    if (first_click && xapi_GetTime() - clicktime_recorder < 2)
    {
        double_click = true;
        first_click = false;
    }
    else
    {
        first_click = true;
        clicktime_recorder = xapi_GetTime();
    }

    if (x >= 5 && x <= 23 && y >= 4 && y <= 22)
    {
        revert_path(current_path);
        paint_dir(current_path);
    }
    else if (x >= 29 && x <= 47 && y >= 4 && y <= 22)
    {
        if (path_p != path_r)
        {
            if (path_p == 0) { path_p = 19; }
            else { path_p--; }
            strcpy(current_path, path_his[path_p]);

            paint_dir(current_path);
        }
    }
    
    if (need_duopage)
    {
        if (x > FMR_X - 19)
        {
            if (y > 148 && y < 166 && file_count_base > 0)
            {
                file_count_base--;
                paint_fm_back();
                paint_dir(current_path);
            }
            else if (y > 170 && y < 188 && file_count_base + 13 <= file_count)
            {
                file_count_base++;
                paint_fm_back();
                paint_dir(current_path);
            }
        }
    }
}
