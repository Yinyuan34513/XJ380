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

        if (cur_x + temp_pchb * 9 >= TML_X - 8) { newline(); }

        p_chbuffer = 0;
        xapi_DrawSWText(handle, cur_x, cur_y, temp, TERMINAL_TEXT_COLOR);

        strcpy(&app_input_buffer[p_input_buffer], char_buffer);
        p_input_buffer += strlen(char_buffer);
        cur_x += temp_pchb * 9;
        xapi_RefreshWindow(handle);
    }
}

void ptt_console_sp(char *str)
{
    char *p = str;
    while (*p != '\0')
    {
        if (cur_x + 9 >= TML_X - 8) { newline(); }

        if (*p == '\n')
        {
            // 换行
            newline();
        }
        else if (*p == '\b')
        {
            // 退格
            backspace();
        }
        else
        {
            // 可见字符
            char temp_char_buffer[2];
            temp_char_buffer[0] = *p;
            temp_char_buffer[1] = '\0';
            xapi_DrawSWText(handle, *ab_x, *ab_y, temp_char_buffer, TERMINAL_TEXT_COLOR);
            cur_x += 9;
        }
        p++;
    }
}

void tam_main_loop()
{
    cur_y += 2;
    while (true)
    {
        // 获取输入
        __asm__ volatile("pause\n\t");
        if (p_chbuffer) { tam_flush_buffer(); }

        // 检查输出缓冲区
        char output_stream[1024];
        read_xttp_buffer(output_stream);
        if (output_stream[0] != '\0') 
        {
            ptt_console_sp(output_stream);
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
        }
    }
}
