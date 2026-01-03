#include <x3api.h>
#include <krlibc.h>
#include "cm_proto.h"

int cindex = 1;

char wdaystr_table[8][20] = {
    "星期一",
    "星期二",
    "星期三",
    "星期四",
    "星期五",
    "星期六",
    "星期日",
    "星期八",
};

void draw_background()
{
    xapi_DrawRect(handle, 0, 0, win_width - 1, win_height - 1, 0x4d4d4dea, true);
    xapi_DrawText(handle, 30, 18, "控制中心", 40, 0xffffffff);
    xapi_DrawRect(handle, 0, 116, win_width - 1, 120, 0x00a2e8ff, true);

    if (cindex == 1) 
    {
        xapi_DrawRect(handle, 280, 60, 386, 120, 0x00a2e8ff, true);
        draw_mainpage();
    }
    else if (cindex == 2) 
    {
        xapi_DrawRect(handle, 386, 60, 544, 120, 0x00a2e8ff, true);
    }
    else if (cindex == 3)
    {
        xapi_DrawRect(handle, 544, 60, 670, 120, 0x00a2e8ff, true);
        draw_setting();
    }

    xapi_DrawText(handle, 312, 64, "主页", 16, 0xffffffff);
    xapi_DrawText(handle, 412, 64, "应用陈列柜", 16, 0xffffffff);
    xapi_DrawText(handle, 576, 64, "控制台", 16, 0xffffffff);

    char time_str_buffer[64];
    memset(time_str_buffer, 0, 64);

    TimeType tm;
    xapi_GetTimeX(&tm);

        xapi_OutputSerial(xcr_int2char(tm.tm_wday));
        xapi_OutputSerial("\n");

    sprintf(time_str_buffer, "%d 年 %d 月 %d 日 %s %02u:%02u", tm.tm_year, tm.tm_mon, tm.tm_mday, 
                    wdaystr_table[tm.tm_wday], tm.tm_hour, tm.tm_min);
    xapi_DrawText(handle, 940, 44, time_str_buffer, 16, 0xffffffff);

    xapi_RefreshWindow(handle);
}

void draw_mainpage()
{
    xapi_DrawRect(handle, 8, win_height - 38, win_width - 8, win_height - 8, 0x00a2e8ff, true);
    xapi_DrawText(handle, 16, win_height - 38, "→ 返回桌面", 16, 0xffffffff);
}

int setting_cindex = 1;

void draw_setting()
{
    xapi_DrawRect(handle, win_width / 10 * 3 + 8, 128, win_width / 10 * 3 + 12, win_height - 8, 0x00a2e8ff, true);

    xapi_DrawRect(handle, 8, 128, win_width / 10 * 3, 158, 0x00a2e8ff, true);
    xapi_DrawText(handle, 16, 128, "系统", 16, 0xffffffff);

    xapi_DrawRect(handle, 8, 166, win_width / 10 * 3, 196, 0x00a2e8ff, true);
    xapi_DrawText(handle, 16, 166, "图形", 16, 0xffffffff);

    xapi_DrawRect(handle, 8, 204, win_width / 10 * 3, 234, 0x00a2e8ff, true);
    xapi_DrawText(handle, 16, 204, "外观", 16, 0xffffffff);

    xapi_DrawRect(handle, 8, 242, win_width / 10 * 3, 272, 0x00a2e8ff, true);
    xapi_DrawText(handle, 16, 242, "应用", 16, 0xffffffff);

    xapi_DrawRect(handle, 8, 280, win_width / 10 * 3, 310, 0x00a2e8ff, true);
    xapi_DrawText(handle, 16, 280, "时间与位置", 16, 0xffffffff);

    if (setting_cindex == 1)
    {
        xapi_DrawRect(handle, win_width / 10 * 3, 128, win_width / 10 * 3 + 8, 158, 0x00a2e8ff, true);

        char system_version[64];
        xapi_GetSystemVersion(system_version);
        char cpu_model[64];
        xapi_GetCpuModel(cpu_model);
        char memory_size[64];
        strcat(memory_size, xcr_int2char(xapi_GetMemorySize() / 1024));
        strcat(memory_size, " GB");

        xapi_DrawPicture(handle, win_width / 10 * 3 + 8, 129, 302, 170, "/system/xj380_2.png");

        xapi_DrawText(handle, win_width / 10 * 3 + 302, 128, "XJ380 操作系统", 16, 0xffffffff);
        xapi_DrawText(handle, win_width / 10 * 3 + 302, 158, system_version, 11, 0xffffffff);

        xapi_DrawText(handle, win_width / 10 * 3 + 302, 218, "处理器", 11, 0xffffffff);
        xapi_DrawText(handle, win_width / 10 * 3 + 402, 218, cpu_model, 11, 0xffffffff);

        xapi_DrawText(handle, win_width / 10 * 3 + 302, 248, "运行内存", 11, 0xffffffff);
        xapi_DrawText(handle, win_width / 10 * 3 + 402, 248, memory_size, 11, 0xffffffff);
    }
    else if (setting_cindex == 2)
    {
        xapi_DrawRect(handle, win_width / 10 * 3, 166, win_width / 10 * 3 + 8, 196, 0x00a2e8ff, true);
    }
    else if (setting_cindex == 3)
    {
        xapi_DrawRect(handle, win_width / 10 * 3, 204, win_width / 10 * 3 + 8, 234, 0x00a2e8ff, true);
    }
    else if (setting_cindex == 4)
    {
        xapi_DrawRect(handle, win_width / 10 * 3, 242, win_width / 10 * 3 + 8, 272, 0x00a2e8ff, true);
    }
    else if (setting_cindex == 5)
    {
        xapi_DrawRect(handle, win_width / 10 * 3, 280, win_width / 10 * 3 + 8, 310, 0x00a2e8ff, true);
    }
}
