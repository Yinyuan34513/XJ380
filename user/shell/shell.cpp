#include "x3api.h"
#include "krlibc.h"
#include "st_proto.h"
#include "gh_sign.h"
#include "convert.h"

int ab_x = 8;
int ab_y = 0;

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
    xapi_DrawRect(handle, 0, 0, 479, 269, 0xFFFFFFFF, true);
    printline("XINGJI SpaceTerminal for XJ380");
    printline("Copyright(C) XINGJI Interactive Software 2017-2025 All rights reserved.");
    paint_arrow_sign(2, 40);
    xapi_RefreshWindow(handle);

    while (1)
        __asm__ __volatile__("pause");
}

void printline(char *str)
{
    xapi_DrawText(handle, ab_x, ab_y, str, 10, 0x000000ff);
    ab_x = 8;
    ab_y += 16;
}

void paint_arrow_sign(int sx, int sy)
{
    UserInfo current_user;
    uint32_t user_name_width = 0;
    uint32_t user_type_width = 0;
    xapi_GetCurrentUser(&current_user);

    xapi_DrawTextl(handle, sx, sy, current_user.name, 10, 0xffffffff, &user_name_width);
    xapi_DrawTextl(handle, sx, sy, user_type_convert_table[current_user.user_type], 10, 0xffffffff, &user_type_width);

    xapi_DrawRect(handle, sx, sy, sx + user_name_width + 13, sy + 16, 0x00a2e8ff, true);
    xapi_DrawRect(handle, sx + user_name_width + 27, sy, 
                          sx + user_name_width + 27 + user_type_width + 13, sy + 16, 
                          0xfff200ff, true);

    for (int y = 0; y < 17; y++) {
        for (int x = 0; x < 10; x++) {
            if (arrow_head[y][x] == '#') { 
                xapi_DrawPoint(handle, x + sx + user_name_width + 19, y + sy, 0xfff200ff); 
            }
        }
    }

    for (int y = 0; y < 17; y++) {
        for (int x = 0; x < 10; x++) {
            if (arrow_head[y][x] == '.') { 
                xapi_DrawPoint(handle, x + sx + user_name_width + 27 + user_type_width + 14, y + sy, 0xfff200ff); 
            }
        }
    }

    for (int y = 0; y < 17; y++) {
        for (int x = 0; x < 10; x++) {
            if (arrow_head[y][x] == '.') { 
                xapi_DrawPoint(handle, x + sx + user_name_width + 14, y + sy, 0x00a2e8ff); 
            }
        }
    }

    xapi_DrawText(handle, sx + 8, sy - 3, current_user.name, 10, 0xffffffff);
    xapi_DrawText(handle, sx + user_name_width + 35, sy - 3, user_type_convert_table[current_user.user_type], 10, 0x000000ff);
}