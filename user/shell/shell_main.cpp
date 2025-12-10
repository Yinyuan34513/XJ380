#include "../xapi/include/krlibc.h"
#include "../xapi/include/x3api.h"
#include "convert.h"
#include "gh_sign.h"
#include "st_proto.h"

// 全局变量定义
int  cur_x  = 0;
int  cur_y  = 0;
int *ab_x   = &cur_x;
int *ab_y   = &cur_y;

char char_buffer[64];
int  p_chbuffer = 0;

char input_buf[1024];
int  p_inbuf = 0;

char current_path[512];

bool front_line_is_arrow = false;
bool command_ck_lock     = false;
bool putchar_lock        = false;
bool paint_cursor_lock   = false;

bool server_mode = false;

HDLE handle;

int blkc = 0, cf = 0;

void shell_MessagePrcor(UINT64 Type, UINT64 hData, UINT64 lData)
{
    switch (Type)
    {
    case MSG_CHAR: putchar_at_cur((char *)&lData); break;
    case MSG_SPCHAR:
        if (lData == '\n') { check_command(); }
        if (lData == '\b') { backspace(); }
        break;
    }
    return;
}

int main(int argc, char *argv[], char *envp[])
{
    XWINDOW Winfo;
    Winfo.title  = (char *)"终端";
    Winfo.width  = TML_X;
    Winfo.height = TML_Y;
    Winfo.sets   = XWIN_NORMAL;
    xapi_CreateWindow(&handle, &Winfo);
    SetMsgPrcor(handle, shell_MessagePrcor);

    mark_terminal();

    // 初始绘制
    *ab_x = 8;
    *ab_y = 4;
    xapi_DrawRect(handle, 0, 0, TML_X - 1, TML_Y - 1, BACKGROUND_COLOR, true);

    if (argv != NULL && argv[0] != NULL)
    {
        if (strcmp(argv[0], "-xj380sys-terminal-app-mode") == 0)
        {
            // 这代表是给别的命令行程序用的
            server_mode = true;
            xapi_RefreshWindow(handle);
            tam_main_loop();
            return 0;
        }
    }

    strcpy(current_path, "/");
    print_to_console("XINGJI SpaceTerminal for XJ380");
    print_to_console("Copyright(C) XINGJI Interactive Software 2017-2025 All rights reserved.");
    paint_arrow_sign(2, 40);
    xapi_RefreshWindow(handle);

    while (1)
    {
        // 主循环中检查并刷新缓冲区
        if (p_chbuffer) { flush_buffer(); }
        __asm__ __volatile__("pause");
    }
}