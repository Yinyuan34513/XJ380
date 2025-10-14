#include "convert.h"
#include "gh_sign.h"
#include "krlibc.h"
#include "st_proto.h"
#include "x3api.h"

int  cur_x  = 0;
int  cur_y  = 0;
int  ab_hsf = 0;
int *ab_x   = &ab_hsf;
int *ab_y   = &cur_y;

char char_buffer[64];
int  p_chbuffer = 0; // 改为int类型，作为缓冲区索引

char input_buf[1024];
int  p_inbuf = 0;

char current_path[512];

bool front_line_is_arrow = false;

bool command_ck_lock = false;

#define TML_X 720
#define TML_Y 405

int blkc = 0, cf = 0;

bool putchar_lock      = false;
bool paint_cursor_lock = false;

HDLE handle;

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
    (void)argc;
    (void)argv;
    (void)envp;

    XWINDOW Winfo;
    Winfo.title  = (char *)"终端";
    Winfo.width  = TML_X;
    Winfo.height = TML_Y;
    xapi_CreateWindow(&handle, &Winfo);
    SetMsgPrcor(handle, shell_MessagePrcor);

    // 初始绘制
    *ab_x = 8;
    *ab_y = 4;
    strcpy(current_path, "/");
    xapi_DrawRect(handle, 0, 0, TML_X - 1, TML_Y - 1, 0xFFFFFFFF, true);
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
void create_directory(char *path)
{
    char  full_path[512];
    bool  recursive   = false;
    char *actual_path = path;

    // 检查 -p 参数
    if (strncmp(path, "-p ", 3) == 0)
    {
        recursive   = true;
        actual_path = path + 3;
        while (*actual_path == ' ')
            actual_path++; // 跳过空格
    }

    // 处理路径
    if (actual_path[0] == '/')
    {
        // 绝对路径
        strcpy(full_path, actual_path);
    }
    else
    {
        // 相对路径
        strcpy(full_path, current_path);
        if (full_path[strlen(full_path) - 1] != '/') { strcat(full_path, "/"); }
        strcat(full_path, actual_path);
    }

    // 规范化路径（处理 .. 和 .）
    normalize_path(full_path);

    if (recursive)
    {
        // 递归创建目录
        if (create_directory_recursive(full_path))
        {
            char success_msg[256];
            strcpy(success_msg, "Directories created: ");
            strcat(success_msg, full_path);
            print_to_console(success_msg);
        }
        else
        {
            char error_msg[256];
            strcpy(error_msg, "mkdir: failed to create directories '");
            strcat(error_msg, full_path);
            strcat(error_msg, "'");
            print_to_console(error_msg);
        }
    }
    else
    {
        // 单级目录创建
        if (xapi_Mkdir(full_path) == 0)
        {
            char success_msg[256];
            strcpy(success_msg, "Directory created: ");
            strcat(success_msg, full_path);
            print_to_console(success_msg);
        }
        else
        {
            char error_msg[256];
            strcpy(error_msg, "mkdir: failed to create directory '");
            strcat(error_msg, full_path);
            strcat(error_msg, "'");
            print_to_console(error_msg);
        }
    }
}

// 递归创建目录
bool create_directory_recursive(char *path)
{
    char  temp_path[512] = "";
    char *token;
    char *rest = path;

    // 如果是绝对路径，从根目录开始
    if (path[0] == '/')
    {
        strcat(temp_path, "/");
        rest = path + 1;
    }

    token = strtok(rest, "/");
    while (token != NULL)
    {
        // 添加当前级目录
        if (strlen(temp_path) > 0 && temp_path[strlen(temp_path) - 1] != '/') { strcat(temp_path, "/"); }
        strcat(temp_path, token);

        // 检查目录是否存在
        uint32_t count;
        DirNode  fdir[256];
        xapi_SearchFile(temp_path, &count, fdir);
        int result = count;

        if (result == 404)
        {
            // 目录不存在，创建它
            if (xapi_Mkdir(temp_path) != 0)
            {
                return false; // 创建失败
            }
        }
        // 如果目录已存在，继续下一级

        token = strtok(NULL, "/");
    }

    return true;
}

// 切换目录函数
void change_directory(char *path)
{
    char new_path[512];
    char temp_path[512];

    if (path[0] == '/')
    {
        // 绝对路径
        strcpy(new_path, path);
    }
    else
    {
        // 相对路径
        strcpy(new_path, current_path);
        if (new_path[strlen(new_path) - 1] != '/') { strcat(new_path, "/"); }
        strcat(new_path, path);
    }

    // 规范化路径
    normalize_path(new_path);

    // 检查路径是否存在（是一个文件夹）
    strcpy(temp_path, new_path);
    if (strcmp(temp_path, "/") != 0 && temp_path[strlen(temp_path) - 1] == '/')
    {
        temp_path[strlen(temp_path) - 1] = '\0'; // 移除末尾的/
    }

    uint32_t count;
    DirNode  fdir[256];
    xapi_SearchFile(temp_path, &count, fdir);
    if (count == 404)
    {
        // 路径不存在
        char error_msg[256];
        strcpy(error_msg, "cd: no such directory: ");
        strcat(error_msg, path);
        print_to_console(error_msg);
        return;
    }

    // 检查是否是目录（通过检查是否包含文件项来判断）
    // 这里简化处理，假设能成功搜索到就是有效目录
    strcpy(current_path, temp_path);

    // 确保路径以/结尾（除了根目录）
    if (strcmp(current_path, "/") != 0 && current_path[strlen(current_path) - 1] != '/') { strcat(current_path, "/"); }
}

// 路径规范化函数，处理 .. 和 .
void normalize_path(char *path)
{
    char parts[50][50]; // 假设路径最多50级
    int  part_count  = 0;
    char result[512] = "/";

    // 分割路径
    char *token = strtok(path, "/");
    while (token != NULL && part_count < 50)
    {
        if (strcmp(token, "..") == 0)
        {
            // 上级目录
            if (part_count > 0) { part_count--; }
        }
        else if (strcmp(token, ".") == 0 || strcmp(token, "") == 0)
        {
            // 当前目录或空，忽略
        }
        else
        {
            // 普通目录
            strcpy(parts[part_count], token);
            part_count++;
        }
        token = strtok(NULL, "/");
    }

    // 重建路径
    for (int i = 0; i < part_count; i++)
    {
        if (i > 0) { strcat(result, "/"); }
        strcat(result, parts[i]);
    }

    // 如果是根目录，确保只有一个/
    if (strcmp(result, "") == 0) { strcpy(result, "/"); }

    strcpy(path, result);
}
void check_command()
{
    command_ck_lock = true;

    input_buf[p_inbuf] = '\0';
    newline();

    if (strncmp("version", input_buf, 7) == 0)
    {
        print_to_console("XINGJI SpaceTerminal for XJ380 [Release Edition]");
        print_to_console("Version 1.0.0 - 2025/10/4   Platform: Only for XJ380");
        print_to_console("Copyright(C) XINGJI Interactive Software 2017-2025 All rights reserved.");
    }
    else if (strncmp("dir", input_buf, 3) == 0) { print_dir(); }
    else if (strncmp("fork", input_buf, 4) == 0)
    {
        uint64_t pid = xapi_Fork();
        if (pid == 0) { xapi_Execve("/system/shell.elf", NULL, NULL); }
    }
    else if (strncmp("mkdir", input_buf, 5) == 0)
    {
        char *dir_name = input_buf + 5;
        while (*dir_name == ' ')
            dir_name++; // 跳过空格

        if (*dir_name == '\0')
        {
            print_to_console("mkdir: missing operand");
            print_to_console("Usage: mkdir [-p] directory_name");
        }
        else { create_directory(dir_name); }
    }
    // 新增 cd 命令
    else if (strncmp("cd", input_buf, 2) == 0)
    {
        char *path_name = input_buf + 2;
        while (*path_name == ' ')
            path_name++; // 跳过空格

        if (*path_name == '\0')
        {
            // 没有参数，默认切换到根目录
            strcpy(current_path, "/");
        }
        else { change_directory(path_name); }
    }
    else if (strncmp("../", input_buf, 3) == 0) { revert_path(current_path); }
    else
    {
        if (check_dir_com(input_buf) == 1)
        {
            // todo: 打开文件
        }
        else if (check_dir_com(input_buf) == 2)
        {
            merge_string(current_path, "/");
            merge_string(current_path, input_buf);
        }
        else if (p_inbuf > 0)
        {
            print_to_console_zh("不存在的命令或路径。请检查您的输入是否有误并再试一次。");
            print_to_console("Bad command. Please check your command and try again.");
        }
    }

    p_inbuf = 0;

    paint_arrow_sign(2, cur_y + 4);

    command_ck_lock = false;
}
void print_dir()
{
    uint32_t count;
    DirNode  fdir[256];
    xapi_SearchFile(current_path, &count, fdir);
    print_to_console("Filename         Type   Size");
    print_to_console("==============================================");
    if (count == 0 || count == 404) return;
    for (int i = 0; i < count; i++)
    {
        xapi_DrawSWText(handle, *ab_x, *ab_y, fdir[i].filename, 0x000000ff);
        xapi_DrawSWText(handle, *ab_x + 17 * 9, *ab_y, (char *)(fdir[i].filetype ? "Folder" : "File"), 0x000000ff);
        if (fdir[i].filetype == 0)
        {
            xapi_DrawSWText(handle, *ab_x + 24 * 9, *ab_y, xcr_int2char(fdir[i].length), 0x000000ff);
        }
        newline();
    }
}

int check_dir_com(char *str)
{
    uint32_t count;
    DirNode  fdir[256];
    xapi_SearchFile(current_path, &count, fdir);
    for (int i = 0; i < count; i++)
    {
        if (strcmp(fdir[i].filename, str) == 0)
        {
            // 不是文件夹
            if (fdir[i].filetype != 1) { return 1; }
            else { return 2; }
        }
    }
    return 0;
}

void scroll()
{
    XCOLOR winbuf[TML_X * TML_Y];
    xapi_ReadBuffer(handle, 0, 0, TML_X, TML_Y, winbuf);
    xapi_DrawRect(handle, 0, 0, TML_X - 1, TML_Y - 1, 0xFFFFFFFF, true);
    xapi_WriteBuffer(handle, 0, 0, TML_X, TML_Y - 16, winbuf + TML_X * 16);
    cur_y -= 16;
}

void print_to_console(char *str)
{
    xapi_DrawSWText(handle, *ab_x, *ab_y, str, 0x000000ff);
    newline();
}

void print_to_console_zh(char *str)
{
    xapi_DrawText(handle, *ab_x, (*ab_y) - 4, str, 12, 0x000000ff);
    *ab_x  = 8;
    *ab_y += 18;
}

void putchar_at_cur(char *ch)
{
    // 将字符存入缓冲区而不是直接显示
    if (p_chbuffer < 255)
    { // 确保不溢出
        char_buffer[p_chbuffer++] = *ch;
    }
    else
    {
        // 缓冲区满，强制刷新
        flush_buffer();
        // 将当前字符放入刷新后的缓冲区
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
    // 如果没有输入内容就无需退格
    if (p_inbuf == 0) return;
    if (cur_x != 8) { cur_x -= 9; }
    else
    {
        cur_x  = TML_X - 17;
        cur_y -= 16;
    }
    xapi_DrawRect(handle, cur_x, cur_y, cur_x + 9, cur_y + 16, 0xffffffff, true);
    xapi_RefreshWindow(handle);
    p_inbuf--;
}

// 新增：刷新缓冲区函数
void flush_buffer()
{
    while (true)
    {
        if (!command_ck_lock) break;
    }

    if (p_chbuffer > 0)
    {
        // 在缓冲区末尾添加字符串结束符
        char_buffer[p_chbuffer] = '\0';

        char temp[256];
        strcpy(temp, char_buffer);

        int temp_pchb = p_chbuffer;

        // 换行
        if (cur_x + temp_pchb * 9 >= TML_X - 8) { newline(); }

        // 重置缓冲区
        p_chbuffer = 0;

        // 绘制缓冲区内容
        xapi_DrawSWText(handle, cur_x, cur_y, temp, 0x000000ff);

        strcpy(&input_buf[p_inbuf], char_buffer);
        p_inbuf += strlen(char_buffer);

        // 更新光标位置（基于字符数量）
        cur_x += temp_pchb * 9;

        // 刷新窗口显示
        xapi_RefreshWindow(handle);
    }
}

#define CALC_FONT_WIDTH(str) ((strlen(str) - 1) * 10)

void paint_arrow_sign(int sx, int sy)
{
    front_line_is_arrow = true;
    UserInfo current_user;
    xapi_GetCurrentUser(&current_user);
    // #00A2E8FF
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

    xapi_DrawSWText(handle, sx + 8, sy + 2, current_path, 0xffffffff);
    xapi_DrawSWText(handle, sx + CALC_FONT_WIDTH(current_path) + 34, sy + 2,
                    user_type_convert_table[current_user.user_type], 0x000000ff);

    cur_x =
        sx + CALC_FONT_WIDTH(current_path) + 27 + CALC_FONT_WIDTH(user_type_convert_table[current_user.user_type]) + 29;
    cur_y = sy + 4;

    if (cur_y >= TML_Y - 32) scroll();

    xapi_RefreshWindow(handle);
}

void merge_string(char *dst, char *src)
{
    char *s1 = dst;
    char *s2 = src;

    while (*s1)
        s1++;

    while (*s2)
    {
        if (*(s1 - 1) == '/' && *s2 == '/')
        {
            s1++;
            s2++;
            continue;
        }
        *s1 = *s2;
        s1++;
        s2++;
    }

    *s1 = '\0';
}

void revert_path(char *path)
{
    char *s = path;
    while (*s)
        s++;

    while (*s != '/')
        s--;

    if (s == path) s++;

    *s = '\0';
}
