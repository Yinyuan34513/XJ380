#include "x3api.h"
#include "krlibc.h"
#include "st_proto.h"

HDLE handle;
void shell_MessagePrcor(UINT64 Type, UINT64 hData, UINT64 lData)
{
    return;
}

int main(int argc, char *argv[], char *envp[])
{
    (void)argc;
    (void)argv;
    (void)envp;

    XWINDOW Winfo;
    Winfo.title  = (char *)"终端";
    Winfo.width  = 480;
    Winfo.height = 270;
    xapi_CreateWindow(&handle, &Winfo);
    SetMsgPrcor(handle, shell_MessagePrcor);
    
    // 初始绘制
    xapi_DrawRect(handle, 0, 0, 480, 270, 0xFFFFFFFF, true);
    paint_arrow_sign(0, 0);
    xapi_RefreshWindow(handle);

    while (1)
        __asm__ __volatile__("pause");
}