#include <x3api.h>
#include <krlibc.h>
#include "fm_proto.h"

bool first_click = false;
bool double_click = false;
UINT64 clicktime_recorder = 0;

int click_x = 0;
int click_y = 0;

int choosing_index = -1;

bool register_lock = false;

bool need_paint = false;

void reg_lock()
{
    while (true)
    {
        if (!register_lock) break;
    }
    register_lock = true;
}

void reg_unlock()
{
    register_lock = false;
}

void register_click(int x, int y)
{
    reg_lock();

    click_x = x;
    click_y = y;
    if (first_click && xapi_GetTime() - clicktime_recorder < 3)
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
        need_paint = true;
    }
    else if (x >= 29 && x <= 47 && y >= 4 && y <= 22)
    {
        if (path_p != path_r)
        {
            if (path_p == 0) { path_p = 19; }
            else { path_p--; }
            strcpy(current_path, path_his[path_p]);

            need_paint = true;
        }
    }
    
    if (need_duopage)
    {
        if (x > FMR_X - 19)
        {
            if (y > 148 && y < 166 && file_count_base > 0)
            {
                file_count_base--;
                need_paint = true;
            }
            else if (y > 170 && y < 188 && file_count_base + 13 <= file_count)
            {
                file_count_base++;
                need_paint = true;
            }
        }
    }

    if (y >= 148)
    {
        int index = (y - 148) / 24;
        choosing_index = index;
        need_paint = true;
    }

    reg_unlock();
}
