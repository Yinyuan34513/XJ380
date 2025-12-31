#include <x3api.h>
#include <krlibc.h>
#include "cm_proto.h"

int cindex = 3;

void draw_background()
{
    xapi_DrawRect(handle, 0, 0, win_width - 1, win_height - 1, 0x4d4d4dea, true);
    xapi_DrawText(handle, 30, 18, "控制中心", 40, 0xffffffff);
    xapi_DrawRect(handle, 0, 116, win_width - 1, 120, 0x00a2e8ff, true);

    if (cindex == 1) xapi_DrawRect(handle, 280, 60, 386, 120, 0x00a2e8ff, true);
    else if (cindex == 2) xapi_DrawRect(handle, 386, 60, 544, 120, 0x00a2e8ff, true);
    else if (cindex == 3) xapi_DrawRect(handle, 544, 60, 670, 120, 0x00a2e8ff, true);

    xapi_DrawText(handle, 312, 64, "主页", 16, 0xffffffff);
    xapi_DrawText(handle, 412, 64, "应用陈列柜", 16, 0xffffffff);
    xapi_DrawText(handle, 576, 64, "控制台", 16, 0xffffffff);
}
