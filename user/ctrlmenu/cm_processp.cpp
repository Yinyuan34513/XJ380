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
        if (x > 8 && y > win_height - 38 && 
            x < win_width - 8 && y < win_height - 8)
        {
            xapi_Exit(0);
        }
    }
}
