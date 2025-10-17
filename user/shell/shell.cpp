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

void print_file_content(char *filename)
{
    char full_path[512];

    // 构建完整路径
    if (filename[0] == '/') { strcpy(full_path, filename); }
    else
    {
        strcpy(full_path, current_path);
        if (full_path[strlen(full_path) - 1] != '/') strcat(full_path, "/");
        strcat(full_path, filename);
    }

    // 使用系统调用读取文件
    char buffer[4096];
    int  ret = xapi_ReadFile(full_path, buffer, sizeof(buffer) - 1, 0);

    if (ret < 0)
    {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "cat: cannot read '%s': No such file or directory", filename);
        print_to_console(error_msg);
        return;
    }

    // 确保字符串以null结尾
    buffer[ret] = '\0';

    // 逐行输出文件内容
    char *line = buffer;
    char *next_line;
    while ((next_line = strchr(line, '\n')) != NULL)
    {
        *next_line = '\0';
        print_to_console(line);
        line = next_line + 1;
    }

    // 输出最后一行（如果没有换行符）
    if (*line != '\0') { print_to_console(line); }
}

void create_file(char *filename)
{
    char full_path[512];

    // 构建完整路径
    if (filename[0] == '/') { strcpy(full_path, filename); }
    else
    {
        strcpy(full_path, current_path);
        if (full_path[strlen(full_path) - 1] != '/') strcat(full_path, "/");
        strcat(full_path, filename);
    }

    // 使用系统调用创建文件
    int ret = xapi_CreateFile(full_path);

    if (ret < 0)
    {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "touch: cannot create '%s': File exists or permission denied", filename);
        print_to_console(error_msg);
    }
    else
    {
        char success_msg[256];
        snprintf(success_msg, sizeof(success_msg), "File created: %s", filename);
        print_to_console(success_msg);
    }
}

void remove_file_or_directory(char *path)
{
    char full_path[512];

    // 构建完整路径
    if (path[0] == '/') { strcpy(full_path, path); }
    else
    {
        strcpy(full_path, current_path);
        if (full_path[strlen(full_path) - 1] != '/') strcat(full_path, "/");
        strcat(full_path, path);
    }

    // 先检查是文件还是目录
    uint32_t count;
    DirNode  fdir[256];
    xapi_SearchFile(full_path, &count, fdir);

    if (count == 404)
    {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "rm: cannot remove '%s': No such file or directory", path);
        print_to_console(error_msg);
        return;
    }

    // 使用系统调用删除
    int ret = xapi_DeleteFile(full_path);

    if (ret < 0)
    {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "rm: cannot remove '%s': Operation failed", path);
        print_to_console(error_msg);
    }
    else
    {
        char success_msg[256];
        snprintf(success_msg, sizeof(success_msg), "Removed: %s", path);
        print_to_console(success_msg);
    }
}

void copy_file(char *src, char *dst)
{
    char src_path[512], dst_path[512];

    // 构建源文件完整路径
    if (src[0] == '/') { strcpy(src_path, src); }
    else
    {
        strcpy(src_path, current_path);
        if (src_path[strlen(src_path) - 1] != '/') strcat(src_path, "/");
        strcat(src_path, src);
    }

    // 构建目标文件完整路径
    if (dst[0] == '/') { strcpy(dst_path, dst); }
    else
    {
        strcpy(dst_path, current_path);
        if (dst_path[strlen(dst_path) - 1] != '/') strcat(dst_path, "/");
        strcat(dst_path, dst);
    }

    // 读取源文件
    char buffer[8192];
    int  bytes_read = xapi_ReadFile(src_path, buffer, sizeof(buffer), 0);

    if (bytes_read < 0)
    {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "cp: cannot copy '%s': No such file", src);
        print_to_console(error_msg);
        return;
    }

    // 写入目标文件
    int bytes_written = xapi_WriteFile(dst_path, buffer, bytes_read, 0);

    if (bytes_written < 0)
    {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "cp: cannot create '%s': Permission denied", dst);
        print_to_console(error_msg);
    }
    else
    {
        char success_msg[256];
        snprintf(success_msg, sizeof(success_msg), "Copied '%s' to '%s'", src, dst);
        print_to_console(success_msg);
    }
}

void move_file(char *src, char *dst)
{
    char src_path[512], dst_path[512];

    // 构建路径（同上）
    if (src[0] == '/') { strcpy(src_path, src); }
    else
    {
        strcpy(src_path, current_path);
        if (src_path[strlen(src_path) - 1] != '/') strcat(src_path, "/");
        strcat(src_path, src);
    }

    if (dst[0] == '/') { strcpy(dst_path, dst); }
    else
    {
        strcpy(dst_path, current_path);
        if (dst_path[strlen(dst_path) - 1] != '/') strcat(dst_path, "/");
        strcat(dst_path, dst);
    }

    // 使用重命名系统调用
    int ret = xapi_RenameFile(src_path, dst_path);

    if (ret < 0)
    {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "mv: cannot move '%s' to '%s': Operation failed", src, dst);
        print_to_console(error_msg);
    }
    else
    {
        char success_msg[256];
        snprintf(success_msg, sizeof(success_msg), "Moved '%s' to '%s'", src, dst);
        print_to_console(success_msg);
    }
}

void print_working_directory()
{
    char pwd_msg[512];
    snprintf(pwd_msg, sizeof(pwd_msg), "Current directory: %s", current_path);
    print_to_console(pwd_msg);
}

void clear_screen()
{
    xapi_DrawRect(handle, 0, 0, TML_X - 1, TML_Y - 1, 0xFFFFFFFF, true);
    cur_x = 8;
    cur_y = 4;
    // paint_arrow_sign(2, cur_y);
    xapi_RefreshWindow(handle);
}

void check_command()
{
    command_ck_lock = true;

    input_buf[p_inbuf] = '\0';
    newline();

    char *cmd  = input_buf;
    char *args = NULL;

    // 分割命令和参数
    for (int i = 0; i < p_inbuf; i++)
    {
        if (input_buf[i] == ' ')
        {
            input_buf[i] = '\0';
            args         = input_buf + i + 1;
            while (*args == ' ')
                args++; // 跳过多余空格
            break;
        }
    }

    if (strncmp("version", cmd, 7) == 0)
    {
        print_to_console("XINGJI SpaceTerminal for XJ380 [Release Edition]");
        print_to_console("Version 1.0.0 - 2025/10/4   Platform: Only for XJ380");
        print_to_console("Copyright(C) XINGJI Interactive Software 2017-2025 All rights reserved.");
    }
    else if (strncmp("dir", cmd, 3) == 0 || strncmp("ls", cmd, 2) == 0) { print_dir(); }
    else if (strncmp("fork", cmd, 4) == 0)
    {
        uint64_t pid = xapi_Fork();
        if (pid == 0) { xapi_Execve("/system/shell.elf", NULL, NULL); }
    }
    else if (strncmp("mkdir", cmd, 5) == 0)
    {
        if (args == NULL || *args == '\0')
        {
            print_to_console("mkdir: missing operand");
            print_to_console("Usage: mkdir [-p] directory_name");
        }
        else
        {
            create_directory(args);
        }
    }
    else if (strncmp("cd", cmd, 2) == 0)
    {
        if (args == NULL || *args == '\0') { strcpy(current_path, "/"); }
        else
        {
            change_directory(args);
        }
    }
    else if (strncmp("cat", cmd, 3) == 0)
    {
        if (args == NULL || *args == '\0')
        {
            print_to_console("cat: missing operand");
            print_to_console("Usage: cat filename");
        }
        else
        {
            print_file_content(args);
        }
    }
    else if (strncmp("touch", cmd, 5) == 0)
    {
        if (args == NULL || *args == '\0')
        {
            print_to_console("touch: missing operand");
            print_to_console("Usage: touch filename");
        }
        else
        {
            create_file(args);
        }
    }
    else if (strncmp("rm", cmd, 2) == 0)
    {
        if (args == NULL || *args == '\0')
        {
            print_to_console("rm: missing operand");
            print_to_console("Usage: rm filename");
        }
        else
        {
            remove_file_or_directory(args);
        }
    }
    else if (strncmp("cp", cmd, 2) == 0)
    {
        char *src = args;
        char *dst = NULL;

        if (src)
        {
            // 查找第二个参数
            while (*src && *src != ' ')
                src++;
            if (*src)
            {
                *src = '\0';
                src  = args;
                dst  = src + strlen(src) + 1;
                while (*dst == ' ')
                    dst++;
            }
        }

        if (src == NULL || dst == NULL || *dst == '\0')
        {
            print_to_console("cp: missing operand");
            print_to_console("Usage: cp source destination");
        }
        else
        {
            copy_file(src, dst);
        }
    }
    else if (strncmp("mv", cmd, 2) == 0)
    {
        char *src = args;
        char *dst = NULL;

        if (src)
        {
            while (*src && *src != ' ')
                src++;
            if (*src)
            {
                *src = '\0';
                src  = args;
                dst  = src + strlen(src) + 1;
                while (*dst == ' ')
                    dst++;
            }
        }

        if (src == NULL || dst == NULL || *dst == '\0')
        {
            print_to_console("mv: missing operand");
            print_to_console("Usage: mv source destination");
        }
        else
        {
            move_file(src, dst);
        }
    }
    else if (strncmp("pwd", cmd, 3) == 0) { print_working_directory(); }
    else if (strncmp("clear", cmd, 5) == 0) { clear_screen(); }
    else if (strncmp("help", cmd, 4) == 0)
    {
        print_to_console("Available commands:");
        print_to_console("  ls, dir     - List directory contents");
        print_to_console("  cd          - Change directory");
        print_to_console("  pwd         - Print working directory");
        print_to_console("  mkdir       - Create directory");
        print_to_console("  touch       - Create empty file");
        print_to_console("  cat         - Print file content");
        print_to_console("  rm          - Remove file or directory");
        print_to_console("  cp          - Copy file");
        print_to_console("  mv          - Move/rename file");
        print_to_console("  clear       - Clear screen");
        print_to_console("  version     - Show version information");
        print_to_console("  help        - Show this help");
    }
    else if (strncmp("../", input_buf, 3) == 0) { revert_path(current_path); }
    else if (strncmp("edit", cmd, 4) == 0)
    {
        if (args == NULL || *args == '\0')
        {
            print_to_console("edit: missing filename");
            print_to_console("Usage: edit filename");
        }
        else
        {
            text_editor(args);
        }
    }
    else
    {
        if (check_dir_com(input_buf) == 1)
        {
            // todo: 打开文件或执行程序
            print_to_console("Executing files is not yet implemented");
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
            else
            {
                return 2;
            }
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

void text_editor(char *filename)
{
    char full_path[512];

    // 构建完整路径
    if (filename[0] == '/') { strcpy(full_path, filename); }
    else
    {
        strcpy(full_path, current_path);
        if (full_path[strlen(full_path) - 1] != '/') strcat(full_path, "/");
        strcat(full_path, filename);
    }

    // 显示编辑器信息
    print_to_console("XINGJI Simple Text Editor");
    print_to_console("Press Ctrl+C to save and exit");
    print_to_console("Press Backspace to delete characters");
    print_to_console("Start typing...");
    print_to_console("==============================================");

    // 编辑器缓冲区
    char editor_buffer[8192]; // 8KB的编辑缓冲区
    int  buffer_pos = 0;

    // 尝试读取现有文件内容
    char file_buffer[4096];
    int  file_size = xapi_ReadFile(full_path, file_buffer, sizeof(file_buffer) - 1, 0);

    if (file_size > 0)
    {
        // 文件存在，加载内容
        file_buffer[file_size] = '\0';
        strcpy(editor_buffer, file_buffer);
        buffer_pos = file_size;

        char        info_msg[128];
        char       *p    = info_msg;
        const char *text = "Loaded existing file: ";
        while (*text && p < info_msg + sizeof(info_msg) - 1)
        {
            *p++ = *text++;
        }
        text = filename;
        while (*text && p < info_msg + sizeof(info_msg) - 1)
        {
            *p++ = *text++;
        }
        *p = '\0';
        print_to_console(info_msg);
    }
    else
    {
        // 新文件
        editor_buffer[0] = '\0';
        buffer_pos       = 0;

        char        info_msg[128];
        char       *p    = info_msg;
        const char *text = "Creating new file: ";
        while (*text && p < info_msg + sizeof(info_msg) - 1)
        {
            *p++ = *text++;
        }
        text = filename;
        while (*text && p < info_msg + sizeof(info_msg) - 1)
        {
            *p++ = *text++;
        }
        *p = '\0';
        print_to_console(info_msg);
    }

    // 显示当前内容（如果有）
    if (buffer_pos > 0)
    {
        print_to_console("Current content:");
        print_to_console(editor_buffer);
    }

    print_to_console("==============================================");
    print_to_console("Ready for editing...");

    // 保存当前光标位置
    int saved_cur_x = cur_x;
    int saved_cur_y = cur_y;

    // 进入编辑模式
    bool editing = true;
    char input_line[256];
    int  input_pos = 0;

    // 临时禁用命令检查锁
    bool saved_command_lock = command_ck_lock;
    command_ck_lock         = false;

    while (editing)
    {
        // 等待输入
        flush_buffer();

        // 检查是否有输入
        if (p_inbuf > 0)
        {
            // 处理输入
            for (int i = 0; i < p_inbuf; i++)
            {
                char ch = input_buf[i];

                // 检查Ctrl+C (ASCII 3)
                if (ch == 3)
                {
                    // Ctrl+C 保存并退出
                    editing = false;
                    break;
                }
                // 检查退格键
                else if (ch == '\b')
                {
                    if (input_pos > 0)
                    {
                        input_pos--;
                        input_line[input_pos] = '\0';

                        // 在屏幕上回显退格
                        if (cur_x > 8)
                        {
                            cur_x -= 9;
                            xapi_DrawRect(handle, cur_x, cur_y, cur_x + 9, cur_y + 16, 0xffffffff, true);
                            xapi_RefreshWindow(handle);
                        }
                    }
                }
                // 检查回车键
                else if (ch == '\n')
                {
                    // 将当前行添加到编辑器缓冲区
                    if (buffer_pos + input_pos + 1 < sizeof(editor_buffer) - 1)
                    {
                        // 添加换行符
                        if (buffer_pos > 0) { editor_buffer[buffer_pos++] = '\n'; }
                        // 添加当前行内容
                        for (int j = 0; j < input_pos; j++)
                        {
                            editor_buffer[buffer_pos++] = input_line[j];
                        }
                        editor_buffer[buffer_pos] = '\0';
                    }

                    // 重置输入行
                    input_pos     = 0;
                    input_line[0] = '\0';

                    // 新行
                    newline();
                }
                // 普通字符
                else if (ch >= 32 && ch <= 126)
                { // 可打印ASCII字符
                    if (input_pos < sizeof(input_line) - 1)
                    {
                        input_line[input_pos++] = ch;
                        input_line[input_pos]   = '\0';

                        // 在屏幕上显示字符
                        char temp[2] = {ch, '\0'};
                        xapi_DrawSWText(handle, cur_x, cur_y, temp, 0x000000ff);
                        cur_x += 9;
                        if (cur_x >= TML_X - 8) { newline(); }
                        xapi_RefreshWindow(handle);
                    }
                }
            }

            // 清空输入缓冲区
            p_inbuf      = 0;
            input_buf[0] = '\0';
        }

        // 短暂暂停以避免过度占用CPU
        __asm__ __volatile__("pause");
    }

    // 恢复命令检查锁
    command_ck_lock = saved_command_lock;

    // 保存文件
    if (buffer_pos > 0)
    {
        int write_result = xapi_WriteFile(full_path, editor_buffer, buffer_pos, 0);

        if (write_result >= 0)
        {
            char        success_msg[128];
            char       *p    = success_msg;
            const char *text = "File saved: ";
            while (*text && p < success_msg + sizeof(success_msg) - 1)
            {
                *p++ = *text++;
            }
            text = filename;
            while (*text && p < success_msg + sizeof(success_msg) - 1)
            {
                *p++ = *text++;
            }

            text = " (";
            while (*text && p < success_msg + sizeof(success_msg) - 1)
            {
                *p++ = *text++;
            }

            // 转换字节数为字符串
            char  size_str[16];
            char *size_p = size_str;
            int   size   = buffer_pos;
            if (size == 0) { *size_p++ = '0'; }
            else
            {
                char  temp[16];
                char *temp_p = temp;
                while (size > 0)
                {
                    *temp_p++  = '0' + (size % 10);
                    size      /= 10;
                }
                while (temp_p > temp)
                {
                    *size_p++ = *(--temp_p);
                }
            }
            *size_p = '\0';

            text = size_str;
            while (*text && p < success_msg + sizeof(success_msg) - 1)
            {
                *p++ = *text++;
            }

            text = " bytes)";
            while (*text && p < success_msg + sizeof(success_msg) - 1)
            {
                *p++ = *text++;
            }
            *p = '\0';

            print_to_console(success_msg);
        }
        else
        {
            char        error_msg[128];
            char       *p    = error_msg;
            const char *text = "Error saving file: ";
            while (*text && p < error_msg + sizeof(error_msg) - 1)
            {
                *p++ = *text++;
            }
            text = filename;
            while (*text && p < error_msg + sizeof(error_msg) - 1)
            {
                *p++ = *text++;
            }
            *p = '\0';

            print_to_console(error_msg);
        }
    }
    else
    {
        // 空文件，创建空文件
        int create_result = xapi_CreateFile(full_path);
        if (create_result >= 0)
        {
            char        success_msg[128];
            char       *p    = success_msg;
            const char *text = "Empty file created: ";
            while (*text && p < success_msg + sizeof(success_msg) - 1)
            {
                *p++ = *text++;
            }
            text = filename;
            while (*text && p < success_msg + sizeof(success_msg) - 1)
            {
                *p++ = *text++;
            }
            *p = '\0';

            print_to_console(success_msg);
        }
    }

    // 恢复原来的光标位置并绘制新的提示符
    cur_x = saved_cur_x;
    cur_y = saved_cur_y;
    paint_arrow_sign(2, cur_y + 4);
}
