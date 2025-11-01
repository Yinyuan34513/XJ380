#include "st_proto.h"
#include "convert.h"
#include "gh_sign.h"

#define CALC_FONT_WIDTH(str) ((strlen(str) - 1) * 10)

void paint_arrow_sign(int sx, int sy)
{
    front_line_is_arrow = true;
    UserInfo current_user;
    xapi_GetCurrentUser(&current_user);
    
    xapi_DrawRect(handle, sx, sy, sx + CALC_FONT_WIDTH(current_path) + 13 + 2, sy + 18, 0x00a2e8ff, true);
    xapi_DrawRect(handle, sx + CALC_FONT_WIDTH(current_path) + 27 + 2, sy,
                  sx + CALC_FONT_WIDTH(current_path) + 27 + 2 +
                      CALC_FONT_WIDTH(user_type_convert_table[current_user.user_type]) + 13,
                  sy + 18, 0xfff200ff, true);

    for (int y = 0; y < 19; y++)
    {
        for (int x = 0; x < 11; x++)
        {
            if (arrow_head[y][x] == '#')
            {
                xapi_DrawPoint(handle, x + sx + CALC_FONT_WIDTH(current_path) + 19 + 2, y + sy, 0xfff200ff);
            }
        }
    }

    for (int y = 0; y < 19; y++)
    {
        for (int x = 0; x < 11; x++)
        {
            if (arrow_head[y][x] == '.')
            {
                xapi_DrawPoint(handle,
                               x + sx + CALC_FONT_WIDTH(current_path) + 27 + 2 +
                                   CALC_FONT_WIDTH(user_type_convert_table[current_user.user_type]) + 14,
                               y + sy, 0xfff200ff);
            }
        }
    }

    for (int y = 0; y < 19; y++)
    {
        for (int x = 0; x < 11; x++)
        {
            if (arrow_head[y][x] == '.')
            {
                xapi_DrawPoint(handle, x + sx + CALC_FONT_WIDTH(current_path) + 14 + 2, y + sy, 0x00a2e8ff);
            }
        }
    }

    xapi_DrawSWText(handle, sx + 5, sy + 2, current_path, 0xffffffff);
    xapi_DrawSWText(handle, sx + CALC_FONT_WIDTH(current_path) + 34, sy + 2,
                    user_type_convert_table[current_user.user_type], 0x000000ff);

    cur_x = sx + CALC_FONT_WIDTH(current_path) + 27 + CALC_FONT_WIDTH(user_type_convert_table[current_user.user_type]) + 29;
    cur_y = sy + 4;

    if (cur_y >= TML_Y - 32) scroll();

    xapi_RefreshWindow(handle);
}

void clear_screen()
{
    xapi_DrawRect(handle, 0, 0, TML_X - 1, TML_Y - 1, BACKGROUND_COLOR, true);
    cur_x = 8;
    cur_y = 4;
    xapi_RefreshWindow(handle);
}

void print_to_console(char *str)
{
    xapi_DrawSWText(handle, *ab_x, *ab_y, str, TERMINAL_TEXT_COLOR);
    newline();
}

void print_to_console_zh(char *str)
{
    xapi_DrawText(handle, *ab_x, (*ab_y) - 4, str, 12, TERMINAL_TEXT_COLOR);
    *ab_y += 2;
    newline();
}

void putchar_at_cur(char *ch)
{
    if (p_chbuffer < 255)
    {
        char_buffer[p_chbuffer++] = *ch;
    }
    else
    {
        flush_buffer();
        if (p_chbuffer < 255) { char_buffer[p_chbuffer++] = *ch; }
    }
}

void newline()
{
    cur_x                = 8;
    cur_y               += front_line_is_arrow ? 20 : 16;
    front_line_is_arrow  = false;
    if (cur_y >= TML_Y - 32) scroll();
}

void backspace()
{
    if (p_inbuf == 0) return;
    if (cur_x != 8) { cur_x -= 9; }
    else
    {
        cur_x  = TML_X - 17;
        cur_y -= 16;
    }
    xapi_DrawRect(handle, cur_x, cur_y, cur_x + 9, cur_y + 16, BACKGROUND_COLOR, true);
    xapi_RefreshWindow(handle);
    p_inbuf--;
}

void flush_buffer()
{
    while (true)
    {
        if (!command_ck_lock) break;
    }

    if (p_chbuffer > 0)
    {
        char_buffer[p_chbuffer] = '\0';
        char temp[256];
        strcpy(temp, char_buffer);
        int temp_pchb = p_chbuffer;

        if (cur_x + temp_pchb * 9 >= TML_X - 8) { newline(); }

        p_chbuffer = 0;
        xapi_DrawSWText(handle, cur_x, cur_y, temp, TERMINAL_TEXT_COLOR);

        strcpy(&input_buf[p_inbuf], char_buffer);
        p_inbuf += strlen(char_buffer);
        cur_x += temp_pchb * 9;
        xapi_RefreshWindow(handle);
    }
}

void scroll()
{
    XCOLOR winbuf[TML_X * TML_Y];
    xapi_ReadBuffer(handle, 0, 0, TML_X, TML_Y, winbuf);
    xapi_DrawRect(handle, 0, 0, TML_X - 1, TML_Y - 1, BACKGROUND_COLOR, true);
    xapi_WriteBuffer(handle, 0, 0, TML_X, TML_Y - 16, winbuf + TML_X * 16);
    cur_y -= 16;
}