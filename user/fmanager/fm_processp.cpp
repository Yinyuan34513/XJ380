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
        if (path_p > 0) {
            path_p--;  // 后退到上一个位置
            strcpy(current_path, path_his[path_p]);
        }
        paint_dir(current_path);
    }

    if(x>=29 && x<=47 && y>= 4 && y<= 22)
    {
        if (path_p < path_r) {
            path_p++;  // 前进到下一个位置
            strcpy(current_path, path_his[path_p]);
        }
        paint_dir(current_path);
    }
}
