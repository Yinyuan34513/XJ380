#include "./xapi/include/xguiapi.h"
#include "./xapi/include/xposix.h"
#include "./xapi/include/krlibc.h"

HDLE handle;

// 终端状态结构
typedef struct {
    char buffer[1024];      // 输入缓冲区
    int cursor_pos;         // 光标位置
    int buffer_len;         // 缓冲区长度
    int display_x, display_y; // 显示位置
    int char_width, char_height; // 字符尺寸
} TerminalState;

TerminalState term;

// 初始化终端状态
void init_terminal() {
    memset(term.buffer, 0, sizeof(term.buffer));
    term.cursor_pos = 0;
    term.buffer_len = 0;
    term.display_x = 10;
    term.display_y = 30;
    term.char_width = 8;
    term.char_height = 16;
}

// 绘制光标
void draw_cursor() {
    // 计算光标位置
    int cursor_x = term.display_x + term.cursor_pos * term.char_width;
    int cursor_y = term.display_y;
    
    // 绘制光标（竖线）
    xapi_DrawLine(handle, cursor_x, cursor_y, cursor_x, cursor_y + term.char_height, 0x000000);
}

// 绘制终端背景和文本
void draw_terminal() {
    // 绘制终端背景（灰色）
    xapi_DrawRect(handle, 5, 25, 315, 175, 0xC0C0C0FF, true);
    
    // 绘制标题栏
    xapi_DrawRect(handle, 5, 5, 315, 25, 0x000080FF, true);
    xapi_DrawText(handle, 10, 8, "终端", 12, 0xFFFFFFFF);
    
    // 绘制提示符
    xapi_DrawText(handle, term.display_x, term.display_y, "> ", 12, 0x000000FF);
    
    // 绘制输入文本
    if (term.buffer_len > 0) {
        char temp[1024];
        strncpy(temp, term.buffer, term.buffer_len);
        temp[term.buffer_len] = '\0';
        xapi_DrawText(handle, term.display_x + 16, term.display_y, temp, 12, 0x000000FF);
    }
    
    // 绘制光标
    draw_cursor();
}

// 处理字符输入
void handle_char_input(char ch) {
    if (term.buffer_len < sizeof(term.buffer) - 1) {
        // 如果是可打印字符
        if (ch >= 32 && ch <= 126) {
            // 在光标位置插入字符
            if (term.cursor_pos < term.buffer_len) {
                // 移动后面的字符
                for (int i = term.buffer_len; i > term.cursor_pos; i--) {
                    term.buffer[i] = term.buffer[i - 1];
                }
            }
            term.buffer[term.cursor_pos] = ch;
            term.buffer_len++;
            term.cursor_pos++;
        }
        // 退格键处理
        else if (ch == 8 && term.cursor_pos > 0) {
            // 删除光标前的字符
            for (int i = term.cursor_pos - 1; i < term.buffer_len - 1; i++) {
                term.buffer[i] = term.buffer[i + 1];
            }
            term.buffer_len--;
            term.cursor_pos--;
        }
        // 回车键处理
        else if (ch == 13) {
            // 这里可以添加命令执行逻辑
            // 清空缓冲区
            memset(term.buffer, 0, sizeof(term.buffer));
            term.buffer_len = 0;
            term.cursor_pos = 0;
        }
        // 左箭头键
        else if (ch == 37 && term.cursor_pos > 0) {
            term.cursor_pos--;
        }
        // 右箭头键
        else if (ch == 39 && term.cursor_pos < term.buffer_len) {
            term.cursor_pos++;
        }
        
        term.buffer[term.buffer_len] = '\0';
    }
}

void shell_MessagePrcor(UINT64 Type, UINT64 hData, UINT64 lData)
{
    if (Type == 0) {  // MSG_CHAR
        char ch = (char)lData;
        handle_char_input(ch);
        
        // 清屏并重绘
        xapi_DrawRect(handle, 0, 0, 320, 180, 0xFFFFFFFF, true);
        draw_terminal();
        xapi_RefreshWindow(handle);
    }
}

int main(int argc, char *argv[], char *envp[])
{
    (void)argc;
    (void)argv;
    (void)envp;

    XWINDOW Winfo;
    Winfo.title  = (char *)"终端";
    Winfo.width  = 320;
    Winfo.height = 180;
    xapi_CreateWindow(&handle, &Winfo);
    SetMsgPrcor(handle, shell_MessagePrcor);
    
    // 初始化终端
    init_terminal();
    
    // 初始绘制
    xapi_DrawRect(handle, 0, 0, 320, 180, 0xFFFFFFFF, true);
    draw_terminal();
    xapi_RefreshWindow(handle);

    while (1)
        __asm__ __volatile__("pause");
}