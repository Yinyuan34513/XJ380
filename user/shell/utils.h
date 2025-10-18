#include "../xapi/include/krlibc.h"
#include "../xapi/include/xtuiapi.h"
#include "st_proto.h"
extern char         current_path[512];
static inline void *memmove(void *dest, const void *src, size_t n)
{
    uint8_t       *pdest = (uint8_t *)dest;
    const uint8_t *psrc  = (const uint8_t *)src;

    if (src > dest)
    {
        for (size_t i = 0; i < n; i++)
        {
            pdest[i] = psrc[i];
        }
    }
    else if (src < dest)
    {
        for (size_t i = n; i > 0; i--)
        {
            pdest[i - 1] = psrc[i - 1];
        }
    }

    return dest;
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

    strcpy(current_path, temp_path);
}

void normalize_path(char *path)
{
    char parts[50][50];
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
    // char src_path[512], dst_path[512];

    // // 构建路径（同上）
    // if (src[0] == '/') { strcpy(src_path, src); }
    // else
    // {
    //     strcpy(src_path, current_path);
    //     if (src_path[strlen(src_path) - 1] != '/') strcat(src_path, "/");
    //     strcat(src_path, src);
    // }

    // if (dst[0] == '/') { strcpy(dst_path, dst); }
    // else
    // {
    //     strcpy(dst_path, current_path);
    //     if (dst_path[strlen(dst_path) - 1] != '/') strcat(dst_path, "/");
    //     strcat(dst_path, dst);
    // }

    // 使用重命名系统调用
    int ret = xapi_RenameFile(src, dst);

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

// 文本编辑器全局变量
char editor_buffer[8192]; // 8KB 编辑缓冲区
int  editor_cursor  = 0;
bool editor_running = false;

// 读取文件内容到编辑器缓冲区
char *editor_read_file(char *filename)
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

    // 读取文件
    int ret = xapi_ReadFile(full_path, editor_buffer, sizeof(editor_buffer) - 1, 0);
    if (ret < 0)
    {
        // 文件不存在，创建空缓冲区
        editor_buffer[0] = '\0';
    }
    else
    {
        editor_buffer[ret] = '\0'; // 确保以 null 结尾
    }

    editor_cursor = 0;
    return editor_buffer;
}

// 保存编辑器内容到文件
void editor_save_file(char *filename, char *content)
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

    // 写入文件
    int ret = xapi_WriteFile(full_path, content, strlen(content), 0);
    if (ret < 0)
    {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "编辑器: 无法保存文件 '%s'", filename);
        print_to_console(error_msg);
    }
    else
    {
        char success_msg[256];
        snprintf(success_msg, sizeof(success_msg), "编辑器: 文件 '%s' 已保存 (%d 字节)", filename, strlen(content));
        print_to_console(success_msg);
    }
}

// 显示编辑器帮助
void editor_display_help()
{
    print_to_console("=== 文本编辑器命令 ===");
    print_to_console("i [文本]    - 在当前位置插入文本");
    print_to_console("d [行号]    - 删除指定行 (不指定则删除当前行)");
    print_to_console("l           - 显示所有行");
    print_to_console("l [起始]-[结束] - 显示指定范围的行");
    print_to_console("s /旧文本/新文本 - 替换文本");
    print_to_console("g [行号]    - 跳转到指定行");
    print_to_console("c           - 清空缓冲区");
    print_to_console("w           - 保存文件");
    print_to_console("q           - 退出编辑器");
    print_to_console("h           - 显示此帮助");
    print_to_console("=====================");
}

// 获取指定行的内容
char *editor_get_line(int line_num, char *buffer, int buffer_size)
{
    static char line_buf[256];
    char       *start        = buffer;
    int         current_line = 1;

    while (*start && current_line < line_num)
    {
        if (*start == '\n') { current_line++; }
        start++;
    }

    if (current_line < line_num)
    {
        return NULL; // 行号超出范围
    }

    // 复制行内容
    int i = 0;
    while (*start && *start != '\n' && i < buffer_size - 1)
    {
        line_buf[i++] = *start++;
    }
    line_buf[i] = '\0';

    return line_buf;
}

// 计算总行数
int editor_count_lines(char *buffer)
{
    int   lines = 1;
    char *ptr   = buffer;

    while (*ptr)
    {
        if (*ptr == '\n') { lines++; }
        ptr++;
    }

    return lines;
}

extern int  cur_x;
extern int  cur_y;
extern char input_buf[1024];
extern int  p_inbuf;
extern int  p_chbuffer;
extern char char_buffer[64];

// 文本编辑器主函数
void text_editor(char *filename)
{
    if (filename == NULL || *filename == '\0')
    {
        print_to_console_zh("编辑器: 缺少文件名");
        print_to_console_zh("用法: edit 文件名");
        return;
    }

    // 读取文件
    char *content     = editor_read_file(filename);
    int   total_lines = editor_count_lines(content);

    char welcome_msg[256];
    snprintf(welcome_msg, sizeof(welcome_msg), "文本编辑器 - 编辑: %s (%d 行, %d 字节)", filename, total_lines,
             strlen(content));
    print_to_console_zh(welcome_msg);
    print_to_console_zh("输入 'h' 查看帮助命令");

    editor_running = true;
    char edit_cmd[256];
    char edit_args[256];

    while (editor_running)
    {
        // 显示简单的提示符
        char prompt[32];
        int  current_line = editor_count_lines(editor_buffer) - editor_count_lines(editor_buffer + editor_cursor) + 1;
        snprintf(prompt, sizeof(prompt), "EDIT[%d]:", current_line);
        xapi_DrawSWText(handle, 8, cur_y, prompt, 0x000000ff);

        // 等待输入
        p_inbuf = 0;
        while (p_inbuf == 0) {
            __asm__ __volatile__("pause");
        }
        input_buf[p_inbuf] = '\0';
        strcpy(edit_cmd, input_buf);

        // 解析命令
        char  command = edit_cmd[0];
        char *args    = edit_cmd + 1;
        while (*args == ' ')
            args++; // 跳过空格

        switch (command)
        {
        case 'i': // 插入文本
            if (strlen(args) > 0)
            {
                // 在当前位置插入文本
                int len = strlen(args);
                if (strlen(editor_buffer) + len < sizeof(editor_buffer) - 1)
                {
                    // 移动后面的文本
                    memmove(editor_buffer + editor_cursor + len, editor_buffer + editor_cursor,
                            strlen(editor_buffer + editor_cursor) + 1);
                    // 插入新文本
                    memcpy(editor_buffer + editor_cursor, args, len);
                    editor_cursor += len;

                    char msg[64];
                    snprintf(msg, sizeof(msg), "已插入 %d 字符", len);
                    print_to_console_zh(msg);
                }
                else
                {
                    print_to_console_zh("错误: 缓冲区已满");
                }
            }
            break;

        case 'l': // 显示行
            if (strlen(args) == 0)
            {
                // 显示所有行
                char *line_start = editor_buffer;
                int   line_num   = 1;

                while (*line_start)
                {
                    char  line_display[300];
                    char *line_end = strchr(line_start, '\n');
                    int   line_len = line_end ? (line_end - line_start) : strlen(line_start);

                    if (line_len > 250) line_len = 250;
                    strncpy(line_display, line_start, line_len);
                    line_display[line_len] = '\0';

                    char line_num_str[16];
                    snprintf(line_num_str, sizeof(line_num_str), "%3d: ", line_num);

                    char full_line[320];
                    strcpy(full_line, line_num_str);
                    strcat(full_line, line_display);
                    print_to_console(full_line);

                    if (line_end) { line_start = line_end + 1; }
                    else
                    {
                        break;
                    }
                    line_num++;
                }
            }
            break;

        case 'd': // 删除行
            // 简化实现：清空当前行
            {
                char *line_start = editor_buffer;
                char *line_end   = strchr(editor_buffer, '\n');
                if (line_end)
                {
                    int line_len = line_end - editor_buffer;
                    memmove(editor_buffer, line_end + 1, strlen(line_end + 1) + 1);
                    editor_cursor = 0;
                    print_to_console_zh("已删除当前行");
                }
                else
                {
                    editor_buffer[0] = '\0';
                    editor_cursor    = 0;
                    print_to_console_zh("已清空缓冲区");
                }
            }
            break;

        case 's': // 搜索替换（简化版）
            if (args[0] == '/' && strchr(args + 1, '/'))
            {
                char *old_text = args + 1;
                char *slash    = strchr(old_text, '/');
                *slash         = '\0';
                char *new_text = slash + 1;

                // 简单替换实现
                char *pos = strstr(editor_buffer, old_text);
                if (pos)
                {
                    int old_len = strlen(old_text);
                    int new_len = strlen(new_text);

                    if (strlen(editor_buffer) + new_len - old_len < sizeof(editor_buffer))
                    {
                        memmove(pos + new_len, pos + old_len, strlen(pos + old_len) + 1);
                        memcpy(pos, new_text, new_len);

                        char msg[64];
                        snprintf(msg, sizeof(msg), "已替换 '%s' -> '%s'", old_text, new_text);
                        print_to_console_zh(msg);
                    }
                }
                else
                {
                    print_to_console_zh("未找到匹配文本");
                }
            }
            break;

        case 'c': // 清空
            editor_buffer[0] = '\0';
            editor_cursor    = 0;
            print_to_console_zh("缓冲区已清空");
            break;

        case 'w': // 保存
            editor_save_file(filename, editor_buffer);
            break;

        case 'q': // 退出
            editor_running = false;
            print_to_console_zh("退出编辑器");
            break;

        case 'h': // 帮助
            editor_display_help();
            break;

        default:
            if (strlen(edit_cmd) > 0)
            {
                char error_msg[64];
                snprintf(error_msg, sizeof(error_msg), "未知命令: %c (输入 'h' 查看帮助)", command);
                print_to_console_zh(error_msg);
            }
            break;
        }

        p_inbuf = 0;
    }

    // 恢复正常的提示符
    paint_arrow_sign(2, cur_y + 4);
}