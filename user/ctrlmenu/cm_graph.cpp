#include <x3api.h>
#include <krlibc.h>
#include "cm_proto.h"
#include "../../kernel/build_settings.h"

int cindex = 1;

bool dont_refresh = false;

char wdaystr_table[7][20] = {
    "星期日",
    "星期一",
    "星期二",
    "星期三",
    "星期四",
    "星期五",
    "星期六",
};

void draw_background()
{
    if (dont_refresh) return;

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

    sprintf(time_str_buffer, "%d 年 %d 月 %d 日 %s %02u:%02u", tm.tm_year, tm.tm_mon, tm.tm_mday, 
                    wdaystr_table[tm.tm_wday - 1], tm.tm_hour, tm.tm_min);
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
        char fenbianlv[64];
        strcat(fenbianlv, xcr_int2char(win_width));
        strcat(fenbianlv, "x");
        strcat(fenbianlv, xcr_int2char(win_height));

        xapi_DrawPicture(handle, win_width / 10 * 3 + 8, 129, 302, 170, "/system/xj380_2.png");

        xapi_DrawText(handle, win_width / 10 * 3 + 302, 128, "XJ380 操作系统", 16, 0xffffffff);
        xapi_DrawText(handle, win_width / 10 * 3 + 302, 158, OS_EDITION, 11, 0xffffffff);

        xapi_DrawText(handle, win_width / 10 * 3 + 302, 218, "处理器", 11, 0xffffffff);
        xapi_DrawText(handle, win_width / 10 * 3 + 402, 218, cpu_model, 11, 0xffffffff);

        xapi_DrawText(handle, win_width / 10 * 3 + 302, 248, "运行内存", 11, 0xffffffff);
        xapi_DrawText(handle, win_width / 10 * 3 + 402, 248, memory_size, 11, 0xffffffff);

        xapi_DrawText(handle, win_width / 10 * 3 + 302, 278, "内核版本", 11, 0xffffffff);
        xapi_DrawText(handle, win_width / 10 * 3 + 402, 278, KN_VERSION, 11, 0xffffffff);

        xapi_DrawText(handle, win_width / 10 * 3 + 302, 308, "系统版本", 11, 0xffffffff);
        xapi_DrawText(handle, win_width / 10 * 3 + 402, 308, system_version, 11, 0xffffffff);

        xapi_DrawText(handle, win_width / 10 * 3 + 302, 338, "系统类型", 11, 0xffffffff);
        xapi_DrawText(handle, win_width / 10 * 3 + 402, 338, "x64 操作系统，基于 x86_64 处理器", 11, 0xffffffff);

        xapi_DrawText(handle, win_width / 10 * 3 + 302, 368, "分辨率", 11, 0xffffffff);
        xapi_DrawText(handle, win_width / 10 * 3 + 402, 368, fenbianlv, 11, 0xffffffff);
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
        draw_setting_apps();
    }
    else if (setting_cindex == 5)
    {
        xapi_DrawRect(handle, win_width / 10 * 3, 280, win_width / 10 * 3 + 8, 310, 0x00a2e8ff, true);
    }
}

void draw_setting_apps()
{
    UserInfo usif;
    xapi_GetCurrentUser(&usif);
    char runfile_path[256];
    memset((void *)runfile_path, 0, 256);
    strcat(runfile_path, "/Users/");
    strcat(runfile_path, usif.name);
    strcat(runfile_path, "/runfile.dat");
    XFILE *f = xapi_OpenFile(runfile_path);
    if (!f) return;
    RunfileSettings_Format *rff = (RunfileSettings_Format *)f->buffer;
    xapi_DrawText(handle, win_width / 10 * 3 + 24, 128, "默认应用", 16, 0xffffffff);

    for (int i = 0; rff->items[i].exname[0] != '\0'; i++)
    {
        char temp_exname[11] = ".";
        strcat(temp_exname, rff->items[i].exname);
        xapi_DrawText(handle, win_width / 10 * 3 + 24 , 168 + i * 32, temp_exname, 11, 0xffffffff);
        xapi_DrawText(handle, win_width / 10 * 3 + 124, 168 + i * 32, rff->items[i].describe, 11, 0xffffffff);
        xapi_DrawText(handle, win_width / 10 * 3 + 274, 168 + i * 32, rff->items[i].runpath, 11, 0xffffffff);
        xapi_Button(handle, i, win_width - 100, 168 + i * 32, "更改");
    }
}

void create_input_box(char *buf)
{
    dont_refresh = true;
    xapi_DrawRect(handle, win_width / 2 - 300, win_height / 2 - 50, win_width / 2 + 300, win_height / 2 + 50, 0x007accff, true);
    xapi_DrawText(handle, win_width / 2 - 300 + 8, win_height / 2 - 50 + 4, "输入新的值：", 11, 0xffffffff);
    xapi_DrawRect(handle, win_width / 2 - 300 + 8, win_height / 2 - 10, win_width / 2 + 300 - 8, win_height / 2 + 10, 0xfffffffff, true);
    xapi_Button(handle, 114514, win_width / 2 + 300 - 58, win_height / 2 + 50 - 32, "取消");
    xapi_Button(handle, 114515, win_width / 2 + 300 - 116, win_height / 2 + 50 - 32, "确定");
    xapi_RefreshPartWindow(handle, win_width / 2 - 300, win_height / 2 - 50, win_width / 2 + 300, win_height / 2 + 50);
}
