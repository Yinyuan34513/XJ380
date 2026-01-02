#include "../include/krlibc.h"
#include "../include/x3api.h"
// 柔和配色方案 (RGBA格式: 0xRRGGBBAA)
#define COLOR_BACKGROUND     0xF5F7FAFF // 浅灰蓝背景
#define COLOR_DISPLAY_BG     0xFFFFFFFF // 白色显示区
#define COLOR_DISPLAY_BORDER 0xD1D9E6FF // 浅灰边框
#define COLOR_NUMBER_BG      0xE3E8F0FF // 浅灰数字按钮
#define COLOR_OPERATOR_BG    0xA0AEC0FF // 灰蓝色运算符
#define COLOR_EQUAL_BG       0x4C6EF5FF // 柔和的蓝色等号
#define COLOR_FUNCTION_BG    0xF0B429FF // 柔和的橙色功能键
#define COLOR_TEXT_DARK      0x2D3748FF // 深灰文字
#define COLOR_TEXT_LIGHT     0xFFFFFFFF // 白色文字

// 计算器状态结构
typedef struct
{
    char      display[32];    // 显示内容
    long long result;         // 计算结果（整数）
    long long operand1;       // 操作数1
    long long operand2;       // 操作数2
    char      operation;      // 当前操作符：+ - * / =
    bool      is_new_operand; // 是否开始新的操作数
    bool      running;        // 程序是否运行
} CalculatorState;

CalculatorState calc_state;
HDLE            window_handle = 0;

// 字符串转换辅助函数
long long str_to_int(const char *str)
{
    if (str == NULL || strlen(str) == 0) return 0;

    long long   result   = 0;
    bool        negative = false;
    const char *p        = str;

    // 处理符号
    if (*p == '-')
    {
        negative = true;
        p++;
    }
    else if (*p == '+') { p++; }

    // 解析数字
    while (*p != '\0')
    {
        if (*p >= '0' && *p <= '9') { result = result * 10 + (*p - '0'); }
        else
        {
            break; // 遇到非数字字符
        }
        p++;
    }

    if (negative) { result = -result; }

    return result;
}

void int_to_str(long long value, char *buffer, int buffer_size)
{
    if (value == 0)
    {
        strcpy(buffer, "0");
        return;
    }

    bool negative = false;
    if (value < 0)
    {
        negative = true;
        value    = -value;
    }

    // 反向构建字符串
    int index = 0;
    while (value > 0 && index < buffer_size - 1)
    {
        buffer[index++]  = '0' + (value % 10);
        value           /= 10;
    }

    if (negative) { buffer[index++] = '-'; }

    buffer[index] = '\0';

    // 反转字符串
    for (int i = 0, j = index - 1; i < j; i++, j--)
    {
        char temp = buffer[i];
        buffer[i] = buffer[j];
        buffer[j] = temp;
    }
}

// 初始化计算器状态
void init_calculator()
{
    memset(&calc_state, 0, sizeof(calc_state));
    strcpy(calc_state.display, "0");
    calc_state.result         = 0;
    calc_state.operand1       = 0;
    calc_state.operand2       = 0;
    calc_state.operation      = '\0';
    calc_state.is_new_operand = true;
    calc_state.running        = true;
}

// 更新显示
void update_display(HDLE handle)
{
    // 绘制显示区域背景
    xapi_DrawRect(handle, 20, 20, 340, 90, COLOR_DISPLAY_BG, true);
    xapi_DrawRect(handle, 20, 20, 340, 90, COLOR_DISPLAY_BORDER, false);

    // 计算文本位置（右对齐）
    // 文本大小24，将文本垂直居中（显示区域高度70，文本高度约24）
    // 垂直位置 = 20(上边距) + (70-24)/2 + 文本基线调整
    UINT32 text_y = 20 + (70 - 24) / 2-10;

    // 右对齐文本
    int    text_len = strlen(calc_state.display);
    UINT32 text_x   = 340 - text_len * 18; // 每个字符约14像素宽
    if (text_x < 30) text_x = 30;

    // 显示文本
    xapi_DrawText(handle, text_x, text_y, calc_state.display, 24, COLOR_TEXT_DARK);
}

// 处理数字按钮
void handle_number(int num)
{
    if (calc_state.is_new_operand)
    {
        strcpy(calc_state.display, "0");
        calc_state.is_new_operand = false;
    }

    // 检查长度限制
    if (strlen(calc_state.display) >= 8) { return; }

    if (strcmp(calc_state.display, "0") == 0)
    {
        calc_state.display[0] = num + '0';
        calc_state.display[1] = '\0';
    }
    else
    {
        int len                     = strlen(calc_state.display);
        calc_state.display[len]     = num + '0';
        calc_state.display[len + 1] = '\0';
    }
}

// 执行计算
void perform_calculation()
{
    calc_state.operand2 = str_to_int(calc_state.display);

    switch (calc_state.operation)
    {
    case '+': calc_state.result = calc_state.operand1 + calc_state.operand2; break;
    case '-': calc_state.result = calc_state.operand1 - calc_state.operand2; break;
    case '*': calc_state.result = calc_state.operand1 * calc_state.operand2; break;
    case '/':
        if (calc_state.operand2 != 0) { calc_state.result = calc_state.operand1 / calc_state.operand2; }
        else
        {
            strcpy(calc_state.display, "Error");
            calc_state.is_new_operand = true;
            return;
        }
        break;
    default: calc_state.result = calc_state.operand2; break;
    }

    // 格式化结果显示
    char temp_buffer[32];
    int_to_str(calc_state.result, temp_buffer, sizeof(temp_buffer));
    strcpy(calc_state.display, temp_buffer);

    calc_state.is_new_operand = true;
    calc_state.operation      = '\0';
}

// 处理操作符
void handle_operator(char op)
{
    if (calc_state.operation != '\0' && !calc_state.is_new_operand) { perform_calculation(); }

    calc_state.operand1       = str_to_int(calc_state.display);
    calc_state.operation      = op;
    calc_state.is_new_operand = true;
}

// 清空计算器
void handle_clear()
{
    init_calculator();
}

// 处理回退
void handle_backspace()
{
    if (strcmp(calc_state.display, "0") == 0 || calc_state.is_new_operand) { return; }

    int len = strlen(calc_state.display);
    if (len == 1)
    {
        strcpy(calc_state.display, "0");
        calc_state.is_new_operand = true;
    }
    else
    {
        calc_state.display[len - 1] = '\0';
    }
}

// 处理正负号切换
void handle_negate()
{
    if (strcmp(calc_state.display, "0") == 0) return;

    long long value = str_to_int(calc_state.display);
    value           = -value;

    char temp_buffer[32];
    int_to_str(value, temp_buffer, sizeof(temp_buffer));
    strcpy(calc_state.display, temp_buffer);
    calc_state.is_new_operand = true;
}

// 绘制按钮
void draw_button(HDLE handle, UINT64 id, int x, int y, const char *text, UINT32 bg_color, UINT32 text_color)
{
    int width  = 75;
    int height = 60;

    // 绘制按钮背景
    xapi_DrawRect(handle, x, y, x + width, y + height, bg_color, true);
    xapi_DrawRect(handle, x, y, x + width, y + height, COLOR_DISPLAY_BORDER, false);

    UINT32 text_y = y + (height - 20) / 2 - 10;

    // 水平居中
    int    text_len = strlen(text);
    UINT32 text_x   = x + (width - text_len * 12) / 2;
    if (text_x < x + 5) text_x = x + 5;

    // 绘制按钮文本
    xapi_DrawText(handle, text_x, text_y, (WSTR)text, 20, text_color);

}

void draw_wide_button(HDLE handle, UINT64 id, int x, int y, const char *text, UINT32 bg_color, UINT32 text_color)
{
    int width  = 160;
    int height = 60;

    // 绘制按钮背景
    xapi_DrawRect(handle, x, y, x + width, y + height, bg_color, true);
    xapi_DrawRect(handle, x, y, x + width, y + height, COLOR_DISPLAY_BORDER, false);

    // 计算文本居中位置
    // 按钮高度60，字体大小20，垂直居中调整
    UINT32 text_y = y + (height - 20) / 2 - 10; 

    // 水平居中
    int    text_len = strlen(text);
    UINT32 text_x   = x + (width - text_len * 12) / 2;
    if (text_x < x + 5) text_x = x + 5;

    // 绘制按钮文本
    xapi_DrawText(handle, text_x, text_y, (WSTR)text, 20, text_color);

}

// 绘制计算器界面
void draw_calculator_interface(HDLE handle)
{
    // 清除背景
    xapi_DrawRect(handle, 0, 0, 380, 480, COLOR_BACKGROUND, true);

    // 绘制标题
    UINT32 title_y = 10;
    xapi_DrawText(handle, 120, title_y, "Calculator", 20, COLOR_TEXT_DARK);

    // 更新显示区域
    update_display(handle);

    // 第一行：功能按钮
    draw_button(handle, 300, 20, 110, "C", COLOR_FUNCTION_BG, COLOR_TEXT_LIGHT);  // 清除
    draw_button(handle, 301, 105, 110, "←", COLOR_FUNCTION_BG, COLOR_TEXT_LIGHT); // 回退
    draw_button(handle, 302, 190, 110, "±", COLOR_FUNCTION_BG, COLOR_TEXT_LIGHT); // 正负号
    draw_button(handle, 203, 275, 110, "÷", COLOR_OPERATOR_BG, COLOR_TEXT_LIGHT); // 除

    // 第二行：7 8 9 ×
    draw_button(handle, 107, 20, 180, "7", COLOR_NUMBER_BG, COLOR_TEXT_DARK);
    draw_button(handle, 108, 105, 180, "8", COLOR_NUMBER_BG, COLOR_TEXT_DARK);
    draw_button(handle, 109, 190, 180, "9", COLOR_NUMBER_BG, COLOR_TEXT_DARK);
    draw_button(handle, 202, 275, 180, "×", COLOR_OPERATOR_BG, COLOR_TEXT_LIGHT);

    // 第三行：4 5 6 -
    draw_button(handle, 104, 20, 250, "4", COLOR_NUMBER_BG, COLOR_TEXT_DARK);
    draw_button(handle, 105, 105, 250, "5", COLOR_NUMBER_BG, COLOR_TEXT_DARK);
    draw_button(handle, 106, 190, 250, "6", COLOR_NUMBER_BG, COLOR_TEXT_DARK);
    draw_button(handle, 201, 275, 250, "-", COLOR_OPERATOR_BG, COLOR_TEXT_LIGHT);

    // 第四行：1 2 3 +
    draw_button(handle, 101, 20, 320, "1", COLOR_NUMBER_BG, COLOR_TEXT_DARK);
    draw_button(handle, 102, 105, 320, "2", COLOR_NUMBER_BG, COLOR_TEXT_DARK);
    draw_button(handle, 103, 190, 320, "3", COLOR_NUMBER_BG, COLOR_TEXT_DARK);
    draw_button(handle, 200, 275, 320, "+", COLOR_OPERATOR_BG, COLOR_TEXT_LIGHT);

    // 第五行：0 =
    draw_wide_button(handle, 100, 20, 390, "0", COLOR_NUMBER_BG, COLOR_TEXT_DARK); // 宽按钮
    draw_button(handle, 204, 190, 390, "=", COLOR_EQUAL_BG, COLOR_TEXT_LIGHT);
}
UINT64 get_btid(int x, int y)
{
    if (x >= 20 && x <= 20 + 75 && y >= 110 && y <= 110 + 60) return 300; //C
    if (x >= 105 && x <= 105 + 75 && y >= 110 && y <= 110 + 60) return 301; //C
    if (x >= 190 && x <= 190 + 75 && y >= 110 && y <= 110 + 60) return 302; //C
    if (x >= 275 && x <= 275 + 75 && y >= 110 && y <= 110 + 60) return 203; //C

    if (x >= 20 && x <= 20 + 75 && y >= 180 && y <= 180 + 60) return 107; //C
    if (x >= 105 && x <= 105 + 75 && y >= 180 && y <= 180 + 60) return 108; //C
    if (x >= 190 && x <= 190 + 75 && y >= 180 && y <= 180 + 60) return 109; //C
    if (x >= 275 && x <= 275 + 75 && y >= 180 && y <= 180 + 60) return 202; //C

    if (x >= 20 && x <= 20 + 75 && y >= 250 && y <= 250 + 60) return 104; //C
    if (x >= 105 && x <= 105 + 75 && y >= 250 && y <= 250 + 60) return 105; //C
    if (x >= 190 && x <= 190 + 75 && y >= 250 && y <= 250 + 60) return 106; //C
    if (x >= 275 && x <= 275 + 75 && y >= 250 && y <= 250 + 60) return 201; //C

    if (x >= 20 && x <= 20 + 75 && y >= 320 && y <= 320 + 60) return 101; //C
    if (x >= 105 && x <= 105 + 75 && y >= 320 && y <= 320 + 60) return 102; //C
    if (x >= 190 && x <= 190 + 75 && y >= 320 && y <= 320 + 60) return 103; //C
    if (x >= 275 && x <= 275 + 75 && y >= 320 && y <= 320 + 60) return 200; //C

    if (x >= 20 && x <= 20 + 160 && y >= 390 && y <= 390 + 60) return 100; //C

    if (x >= 190 && x <= 190 + 75 && y >= 390 && y <= 390 + 60) return 204; //C
}
void process_left_b(int x, int y)
{
    UINT64 button_id = get_btid(x, y);

    // 数字按钮 0-9
    if (button_id >= 100 && button_id <= 109) { handle_number(button_id - 100); }
    // 操作符
    else if (button_id == 200)
    { // +
        handle_operator('+');
    }
    else if (button_id == 201)
    { // -
        handle_operator('-');
    }
    else if (button_id == 202)
    { // × (乘法)
        handle_operator('*');
    }
    else if (button_id == 203)
    { // ÷ (除法)
        handle_operator('/');
    }
    else if (button_id == 204)
    { // =
        if (calc_state.operation != '\0') { perform_calculation(); }
    }
    // 功能按钮
    else if (button_id == 300)
    { // C
        handle_clear();
    }
    else if (button_id == 301)
    { // ⌫
        handle_backspace();
    }
    else if (button_id == 302)
    { // ±
        handle_negate();
    }

    // 更新显示
    update_display(window_handle);
    xapi_RefreshWindow(window_handle);
}
// 消息处理函数
void message_handler(UINT64 Type, UINT64 hData, UINT64 lData)
{
    if (Type == MSG_LBUTTON) { process_left_b(hData, lData); }
}
int main(int argc, char *argv[], char *envp[])
{
    // 初始化计算器状态
    init_calculator();

    // 创建窗口
    XWINDOW window;
    window.width  = 380;
    window.height = 480;
    window.title  = "Soft Calculator";
    window.sets   = XWIN_NORMAL;

    xapi_CreateWindow(&window_handle, &window);
    xapi_SetIcon(window_handle, "/system/icon/calc.png");

    // 设置消息处理器
    SetMsgPrcor(window_handle, message_handler);

    // 绘制初始界面
    draw_calculator_interface(window_handle);

    // 刷新窗口
    xapi_RefreshWindow(window_handle);

    while (calc_state.running)
    {
        __asm__ volatile("pause\n\t");
    }

    return 0;
}