#include <x3api.h>
#include <krlibc.h>
#include "cm_proto.h"

HDLE handle;

UINT64 win_width;
UINT64 win_height;

char wdaystr_table[7][20] = {
    "星期一",
    "星期二",
    "星期三",
    "星期四",
    "星期五",
    "星期六",
    "星期日",
};

void ctrlmenu_MessagePrcor(UINT64 Type, UINT64 hData, UINT64 lData)
{
    switch (Type)
    {
    case MSG_LBUTTON:
        process_left_key(hData, lData);
        break;
    }
    return;
}

int main(int argc, char *argv[], char *envp[])
{
    (void)argc;
    (void)argv;
    (void)envp;

    XWINDOW Winfo;
    Winfo.title = (char *)"控制中心";
    Winfo.sets  = XWIN_FULL_SCR;  
    xapi_CreateWindow(&handle, &Winfo);
    xapi_GetWindowSize(handle, &win_width, &win_height);
    SetMsgPrcor(handle, ctrlmenu_MessagePrcor);

    draw_background();

    xapi_RefreshWindow(handle);

    int old_min = 60;

    while (1)
    {
        __asm__ __volatile__("pause");

        char time_str_buffer[64];
        memset(time_str_buffer, 0, 64);

        TimeType tm;
        xapi_GetTimeX(&tm);

        if (tm.tm_min != old_min)
        {
            draw_background();

            sprintf(time_str_buffer, "%d 年 %d 月 %d 日 %s %02u:%02u", tm.tm_year, tm.tm_mon, tm.tm_mday, 
                    wdaystr_table[tm.tm_wday], tm.tm_hour, tm.tm_min);

            xapi_DrawText(handle, 940, 44, time_str_buffer, 16, 0xffffffff);

            xapi_RefreshWindow(handle);
            old_min = tm.tm_min;
        }
        
        xapi_Sleep(1000);
    }
}


