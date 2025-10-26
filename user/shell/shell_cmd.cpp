#include "st_proto.h"
#include "gh_sign.h"
#include <stdint.h>

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
                args++;
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
        print_to_console("  edit        - Edit file");
    }
    else if (strncmp("../", input_buf, 3) == 0) { revert_path(current_path); }
    else if (strncmp("edit", cmd, 4) == 0)
    {
        if (args == NULL || *args == '\0')
        {
            print_to_console("edit: 缺少文件名");
            print_to_console("用法: edit 文件名");
        }
        else
        {
            text_editor(args);
        }
    }
    else if (strncmp("info", cmd, 4) == 0)
    {
        char version_info[100];
        xapi_GetSystemVersion(version_info);
        print_to_console(info_logo[0]);
        print_to_console(info_logo[1]);
        print_to_console(info_logo[2]);
        print_to_console(info_logo[3]);
        print_to_console(info_logo[4]);
        print_to_console(info_logo[5]);
        print_to_console(version_info);
    }
    else
    {
        if (check_dir_com(input_buf) == 1)
        {
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