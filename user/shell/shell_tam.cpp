#include "../xapi/include/krlibc.h"
#include "../xapi/include/x3api.h"
#include "convert.h"
#include "st_proto.h"

extern int  cur_x;
extern int  cur_y;
extern int *ab_x;
extern int *ab_y;
extern char char_buffer[64];
extern int  p_chbuffer;
extern bool command_ck_lock;

char app_input_buffer[1024];
int  p_input_buffer = 0;
bool input_is_enter = false;

void terminal_child_putchar(char ch)
{
    if (p_input_buffer >= (int)sizeof(app_input_buffer) - 2) return;

    char text[2] = {ch, '\0'};
    app_input_buffer[p_input_buffer++] = ch;
    app_input_buffer[p_input_buffer] = '\0';
    ptt_console_sp(text);
    reset_terminal_cursor_blink();
    show_terminal_cursor(false);
    xapi_RefreshWindow(handle);
}

void terminal_child_backspace()
{
    if (p_input_buffer <= 0) return;

    p_input_buffer--;
    app_input_buffer[p_input_buffer] = '\0';

    hide_terminal_cursor(false);
    if (cur_x != 8) { cur_x -= 9; }
    else
    {
        cur_x  = TML_X - 17;
        cur_y -= 16;
    }
    xapi_DrawRect(handle, cur_x, cur_y, cur_x + 9, cur_y + 16, BACKGROUND_COLOR, true);
    reset_terminal_cursor_blink();
    show_terminal_cursor(false);
    xapi_RefreshWindow(handle);
}

void terminal_child_submit_line()
{
    if (p_input_buffer < (int)sizeof(app_input_buffer) - 1)
    {
        app_input_buffer[p_input_buffer++] = '\n';
        app_input_buffer[p_input_buffer] = '\0';
    }
    newline();
    input_is_enter = true;
    reset_terminal_cursor_blink();
    show_terminal_cursor(true);
    xapi_RefreshWindow(handle);
}

void terminal_child_flush_input_if_requested()
{
    if (!check_read_xttp_buffer() || !input_is_enter) return;

    input_is_enter = false;
    write_xttp_buffer(app_input_buffer);
    app_input_buffer[0] = '\0';
    p_input_buffer      = 0;
    reset_terminal_cursor_blink();
    show_terminal_cursor(true);
}

void tam_flush_buffer()
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

        hide_terminal_cursor(false);
        if (cur_x + temp_pchb * 9 >= TML_X - 8) { newline(); }

        p_chbuffer = 0;
        xapi_DrawSWText(handle, cur_x, cur_y, temp, TERMINAL_TEXT_COLOR);

        strcpy(&app_input_buffer[p_input_buffer], char_buffer);
        p_input_buffer += strlen(char_buffer);
        cur_x += temp_pchb * 9;
        reset_terminal_cursor_blink();
        show_terminal_cursor(false);
        xapi_RefreshWindow(handle);
    }
}

#include <stdint.h>

// UTF-8辅助函数
static int utf8_char_len(unsigned char c) {
    if ((c & 0x80) == 0x00) return 1;      // ASCII
    if ((c & 0xE0) == 0xC0) return 2;      // 2字节
    if ((c & 0xF0) == 0xE0) return 3;      // 3字节（大多数中文字符）
    if ((c & 0xF8) == 0xF0) return 4;      // 4字节
    return 1;  // 无效，按单字节处理
}

static UINT32 ansi_color_table[8] = {
    0x000000ff, // black
    0xaa0000ff, // red
    0x00aa00ff, // green
    0xaa5500ff, // yellow
    0x0000aaff, // blue
    0xaa00aaff, // magenta
    0x00aaaaff, // cyan
    0xaaaaaaff, // white
};

static UINT32 ansi_bright_color_table[8] = {
    0x555555ff, // bright black
    0xff5555ff, // bright red
    0x55ff55ff, // bright green
    0xffff55ff, // bright yellow
    0x5555ffff, // bright blue
    0xff55ffff, // bright magenta
    0x55ffffff, // bright cyan
    0xffffffff, // bright white
};

static UINT32 terminal_current_text_color = TERMINAL_TEXT_COLOR;
static bool terminal_bold_text = false;

static void apply_ansi_sgr_param(int param)
{
    if (param == 0)
    {
        terminal_current_text_color = TERMINAL_TEXT_COLOR;
        terminal_bold_text = false;
    }
    else if (param == 1)
    {
        terminal_bold_text = true;
    }
    else if (param == 22)
    {
        terminal_bold_text = false;
    }
    else if (param == 39)
    {
        terminal_current_text_color = TERMINAL_TEXT_COLOR;
    }
    else if (param >= 30 && param <= 37)
    {
        int idx = param - 30;
        terminal_current_text_color = terminal_bold_text ? ansi_bright_color_table[idx] : ansi_color_table[idx];
    }
    else if (param >= 90 && param <= 97)
    {
        terminal_current_text_color = ansi_bright_color_table[param - 90];
    }
}

static void apply_ansi_sgr_sequence(const char *params, int len)
{
    if (len <= 0)
    {
        apply_ansi_sgr_param(0);
        return;
    }

    int value = 0;
    bool have_value = false;
    for (int i = 0; i <= len; i++)
    {
        char c = i < len ? params[i] : ';';
        if (c >= '0' && c <= '9')
        {
            value = value * 10 + (c - '0');
            have_value = true;
        }
        else if (c == ';' || c == ':')
        {
            apply_ansi_sgr_param(have_value ? value : 0);
            value = 0;
            have_value = false;
        }
    }
}

static char *handle_ansi_escape(char *p)
{
    if (*p != '\x1b') return p;
    p++;

    if (*p == '[')
    {
        p++;
        char *params = p;
        while (*p != '\0')
        {
            unsigned char c = (unsigned char)*p++;
            if (c >= 0x40 && c <= 0x7e)
            {
                if (c == 'm') apply_ansi_sgr_sequence(params, (int)((p - 1) - params));
                break;
            }
        }
        return p;
    }

    if (*p == ']')
    {
        p++;
        while (*p != '\0')
        {
            if (*p == '\a') return p + 1;
            if (*p == '\x1b' && p[1] == '\\') return p + 2;
            p++;
        }
        return p;
    }

    while (*p >= 0x20 && *p <= 0x2f) p++;
    if (*p != '\0') p++;
    return p;
}

void ptt_console_sp(char *str)
{
    hide_terminal_cursor(false);
    char *p = str;
    while (*p != '\0')
    {
        if (*p == '\x1b')
        {
            p = handle_ansi_escape(p);
            continue;
        }

        if (*p == '\n')
        {
            // 换行
            newline();
            p++;
            continue;
        }
        else if (*p == '\r')
        {
            cur_x = 8;
            p++;
            continue;
        }
        else if (*p == '\b')
        {
            // 退格
            backspace();
            p++;
            continue;
        }

        if (cur_x + 9 >= TML_X - 8) { newline(); }

        // 获取UTF-8字符长度
        int char_len = utf8_char_len((unsigned char)*p);

        if (char_len == 1)
        {
            // ASCII字符：使用等宽字体
            char temp_char_buffer[2];
            temp_char_buffer[0] = *p;
            temp_char_buffer[1] = '\0';
            xapi_DrawSWText(handle, *ab_x, *ab_y, temp_char_buffer, terminal_current_text_color);
            cur_x += 9;  // 等宽字体每个字符9像素
            p++;
            continue;
        }

        // 中文字符
        char utf8_char[5] = {0};
        int copied = 0;
        for (; copied < char_len && p[copied] != '\0'; copied++) {
            utf8_char[copied] = p[copied];
        }

        // 中文
        xapi_DrawSWText(handle, *ab_x, *ab_y, utf8_char, terminal_current_text_color);
        cur_x += 18;  // 中文字符宽度 = ASCII * 2 （也许）
        p += copied;
    }
}
void tam_main_loop()
{
    cur_y += 2;
    reset_terminal_cursor_blink();
    show_terminal_cursor(true);
    while (true)
    {
        // 获取输入
        xapi_Sleep(16);
        __asm__ volatile("pause\n\t");
        if (p_chbuffer) { tam_flush_buffer(); }
        update_terminal_cursor_blink(16);

        // 检查输出缓冲区
        char output_stream[1024];
        memset(output_stream, 0, sizeof(output_stream));
        read_xttp_buffer(output_stream);
        if (output_stream[0] != '\0') 
        {
            ptt_console_sp(output_stream);
            reset_terminal_cursor_blink();
            show_terminal_cursor(false);
            memset(output_stream, 0, (strlen(output_stream) + 1) * sizeof(uint8_t));
            xapi_RefreshWindow(handle);
            terminal_app_mark_finish_output();
        }

        // 检查输入缓冲区
        if (check_read_xttp_buffer() && input_is_enter)
        {
            newline();
            input_is_enter = false;
            strcat(app_input_buffer, "\0");
            write_xttp_buffer(app_input_buffer);
            app_input_buffer[0] = '\0';
            p_input_buffer      = 0;
            reset_terminal_cursor_blink();
            show_terminal_cursor(true);
        }
    }
}
