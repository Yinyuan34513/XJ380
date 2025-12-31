#include <x3api.h>
#include <krlibc.h>
#include "cm_proto.h"

HDLE handle;

UINT64 win_width;
UINT64 win_height;

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

    draw_background();

    xapi_RefreshWindow(handle);

    while (1)
    {
        __asm__ __volatile__("pause");
    }
}


