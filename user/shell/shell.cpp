#include "x3api.h"
#include "krlibc.h"
#include "st_proto.h"
#include "gh_sign.h"
#include "convert.h"

int cur_x = 0;
int cur_y = 0;
int ab_hsf = 0;
int *ab_x = &ab_hsf;
int *ab_y = &cur_y;

char char_buffer[256];
int p_chbuffer = 0;  // 改为int类型，作为缓冲区索引
int buffer_dirty = 0; // 新增：标记缓冲区是否有数据需要刷新

HDLE handle;

void flush_buffer(); // 新增：声明缓冲区刷新函数

void shell_MessagePrcor(UINT64 Type, UINT64 hData, UINT64 lData)
{
    switch (Type)
    {
    case MSG_CHAR:
        putchar_at_cur((char *)&lData);
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
    Winfo.title  = (char *)"终端";
    Winfo.width  = 720;
    Winfo.height = 405;
    xapi_CreateWindow(&handle, &Winfo);
    SetMsgPrcor(handle, shell_MessagePrcor);
    
    // 初始绘制
    *ab_x = 8;
    *ab_y = 4;
    xapi_DrawRect(handle, 0, 0, 719, 404, 0xFFFFFFFF, true);
    print_to_console("XINGJI SpaceTerminal for XJ380");
    print_to_console("Copyright(C) XINGJI Interactive Software 2017-2025 All rights reserved.");
    paint_arrow_sign(2, 46);
    xapi_RefreshWindow(handle);

    while (1) {
        // 主循环中检查并刷新缓冲区
        if (buffer_dirty) {
            flush_buffer();
            buffer_dirty = 0;
        }
        __asm__ __volatile__("pause");
    }
}

void print_to_console(char *str)
{
    xapi_DrawSWText(handle, *ab_x, *ab_y, str, 10, 0x000000ff);
    *ab_x = 8;
    *ab_y += 16;
}

void putchar_at_cur(char *ch)
{
    // 将字符存入缓冲区而不是直接显示
    if (p_chbuffer < 255) { // 确保不溢出
        char_buffer[p_chbuffer++] = *ch;
        buffer_dirty = 1; // 标记缓冲区有数据需要刷新
        
        // 如果是换行符，立即刷新缓冲区并重置光标
        if (*ch == '\n' || *ch == '\r') {
            flush_buffer();
            cur_x = 8;
            cur_y += 16;
            p_chbuffer = 0; // 重置缓冲区索引
            buffer_dirty = 0;
        }
    } else {
        // 缓冲区满，强制刷新
        flush_buffer();
        // 将当前字符放入刷新后的缓冲区
        if (p_chbuffer < 255) {
            char_buffer[p_chbuffer++] = *ch;
            buffer_dirty = 1;
        }
    }
}

// 新增：刷新缓冲区函数
void flush_buffer()
{
    if (p_chbuffer > 0) {
        // 在缓冲区末尾添加字符串结束符
        char_buffer[p_chbuffer] = '\0';
        
        // 绘制缓冲区内容
        xapi_DrawSWText(handle, cur_x, cur_y, char_buffer, 10, 0x000000ff);
        
        // 更新光标位置（基于字符数量）
        cur_x += p_chbuffer * 10;
        
        // 重置缓冲区
        p_chbuffer = 0;
        
        // 刷新窗口显示
        xapi_RefreshWindow(handle);
    }
}

#define CALC_FONT_WIDTH(str) ((strlen(str) - 1) * 10) 

void paint_arrow_sign(int sx, int sy)
{
    UserInfo current_user;
    xapi_GetCurrentUser(&current_user);

    strcpy(current_user.name, "/apps");

    xapi_DrawRect(handle, sx, sy, sx + CALC_FONT_WIDTH(current_user.name) + 13, sy + 18, 0x00a2e8ff, true);
    xapi_DrawRect(handle, sx + CALC_FONT_WIDTH(current_user.name) + 27, sy, 
                          sx + CALC_FONT_WIDTH(current_user.name) + 27 + 
                          CALC_FONT_WIDTH(user_type_convert_table[current_user.user_type]) + 13, sy + 18, 
                          0xfff200ff, true);

    for (int y = 0; y < 19; y++) {
        for (int x = 0; x < 11; x++) {
            if (arrow_head[y][x] == '#') { 
                xapi_DrawPoint(handle, x + sx + CALC_FONT_WIDTH(current_user.name) + 19, y + sy, 0xfff200ff); 
            }
        }
    }

    for (int y = 0; y < 19; y++) {
        for (int x = 0; x < 11; x++) {
            if (arrow_head[y][x] == '.') { 
                xapi_DrawPoint(handle, x + sx + CALC_FONT_WIDTH(current_user.name) + 27 + 
                               CALC_FONT_WIDTH(user_type_convert_table[current_user.user_type]) + 14, 
                               y + sy, 0xfff200ff); 
            }
        }
    }

    for (int y = 0; y < 19; y++) {
        for (int x = 0; x < 11; x++) {
            if (arrow_head[y][x] == '.') { 
                xapi_DrawPoint(handle, x + sx + CALC_FONT_WIDTH(current_user.name) + 14, y + sy, 0x00a2e8ff); 
            }
        }
    }

    xapi_DrawSWText(handle, sx + 8, sy - 1, current_user.name, 10, 0xffffffff);
    xapi_DrawSWText(handle, sx + CALC_FONT_WIDTH(current_user.name) + 34, sy - 1, 
                  user_type_convert_table[current_user.user_type], 10, 0x000000ff);

    cur_x = sx + CALC_FONT_WIDTH(current_user.name) + 27 + 
                 CALC_FONT_WIDTH(user_type_convert_table[current_user.user_type]) + 29;
    cur_y = sy;
}