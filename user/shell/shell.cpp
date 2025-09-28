#include "x3api.h"
#include "krlibc.h"
#include "st_proto.h"
#include "gh_sign.h"

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

void paint_arrow_sign(int sx, int sy)
{
    UserInfo current_user;
    uint32_t user_name_width = 0;
    xapi_GetCurrentUser(&current_user);
    xapi_DrawTextl(handle, sx, sy, current_user.name, 10, 0xffffffff, &user_name_width);
    xapi_DrawRect(handle, sx, sy, sx + user_name_width + 4, sy + 16, 0x00a2e8ff, true);
    // xapi_DrawTextl(handle, sx + 12, sy, current_user.name, 10, 0xffffffff, &user_name_width);

    // for (int y = 0; y < 10; y++) {
    //     for (int x = 0; x < 20; x++) {
    //         if (arrow_head[y][x] == '@') xapi_DrawPoint(handle, sx + x, sy + y, 0x00a2e8ff);
    //     }
    // }
}