#include <x3api.h>
#include <krlibc.h>
#include "cm_proto.h"

HDLE handle;

UINT64 win_width;
UINT64 win_height;

bool exit_cm = false;

void ctrlmenu_MessagePrcor(UINT64 Type, UINT64 hData, UINT64 lData)
{
    switch (Type)
    {
    case MSG_LBUTTON:
        process_left_key(hData, lData);
        break;
    case MSG_CRL:
        if (lData < 2000) { create_input_box((char *)lData); } // cm_graph.cpp:145
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
        if (exit_cm)
            xapi_Exit(0);
            
        __asm__ __volatile__("pause");

        TimeType tm;
        xapi_GetTimeX(&tm);

        if (tm.tm_min != old_min)
        {
            draw_background();
            old_min = tm.tm_min;
        }
        
        xapi_Sleep(1000);
    }
}


