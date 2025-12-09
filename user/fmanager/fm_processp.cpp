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
    if (first_click && xapi_GetTime() - clicktime_recorder <= 500)
    {
        double_click = true;
        first_click = false;
    }
    else
    {
        first_click = true;
        clicktime_recorder = xapi_GetTime();
    }
}
