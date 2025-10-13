// file_manager.cpp
#include "krlibc.h"
#include "fm_proto.h"
#include "x3api.h"

// 文件管理器窗口尺寸
#define FMR_X 800
#define FMR_Y 450

HDLE handle;

int main(int argc, char *argv[], char *envp[])
{
    (void)argc;
    (void)argv;
    (void)envp;

    XWINDOW Winfo;
    Winfo.title  = (char *)"文件管理器";
    Winfo.width  = FMR_X;
    Winfo.height = FMR_Y;
    xapi_CreateWindow(&handle, &Winfo);

    paint_fm_back();

    xapi_RefreshWindow(handle);

    while (1)
    {
        __asm__ __volatile__("pause");
    }
}

void paint_fm_back()
{
    xapi_DrawRect(handle, 0, 0 , FMR_X - 1, 21, 0xeaeaeaff, true);
    xapi_DrawPNG(handle, 0, 22, FMR_X, 90, "/system/image/fm_bg.png");
}
