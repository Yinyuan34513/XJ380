#include "st_proto.h"
#include "gh_sign.h"
#include <stdint.h>

uint64_t rp = 0;

extern bool input_is_enter; // shell_tam.cpp
extern bool server_mode; // shell_main.cpp

static int build_external_argv(char *exe_path, char *args, char *argv[], int max_argc)
{
    int argc = 0;
    if (max_argc <= 1) return 0;

    argv[argc++] = exe_path;
    while (args != NULL && *args != '\0' && argc < max_argc - 1)
    {
        while (*args == ' ')
            args++;
        if (*args == '\0') break;

        char quote = 0;
        if (*args == '\'' || *args == '"')
        {
            quote = *args;
            args++;
        }

        argv[argc++] = args;
        char *out = args;
        while (*args != '\0')
        {
            if (quote)
            {
                if (*args == quote)
                {
                    args++;
                    break;
                }
            }
            else if (*args == ' ')
            {
                break;
            }

            if (*args == '\\' && args[1] != '\0') args++;
            *out++ = *args++;
        }
        bool has_more = *args != '\0';
        *out = '\0';
        if (!has_more) break;
        args++;
    }
    argv[argc] = NULL;
    return argc;
}

static bool append_to_buffer(char *out, size_t out_size, const char *suffix)
{
    size_t used = strlen(out);
    size_t add  = strlen(suffix);
    if (used + add + 1 > out_size) return false;
    strcat(out, suffix);
    return true;
}

static bool resolve_external_command_path(char *cmd, char *out, size_t out_size)
{
    if (cmd == NULL || *cmd == '\0' || out == NULL || out_size == 0) return false;

    out[0] = '\0';
    if (cmd[0] == '/')
    {
        strncpy(out, cmd, out_size - 1);
        out[out_size - 1] = '\0';
        normalize_path(out);
        return true;
    }

    if (strchr(cmd, '/') != NULL)
    {
        strncpy(out, current_path, out_size - 1);
        out[out_size - 1] = '\0';
        if (out[strlen(out) - 1] != '/' && !append_to_buffer(out, out_size, "/")) return false;
        if (!append_to_buffer(out, out_size, cmd)) return false;
        normalize_path(out);
        return true;
    }

    if (check_dir_com(cmd) != 1) return false;
    strncpy(out, current_path, out_size - 1);
    out[out_size - 1] = '\0';
    if (out[strlen(out) - 1] != '/' && !append_to_buffer(out, out_size, "/")) return false;
    if (!append_to_buffer(out, out_size, cmd)) return false;
    normalize_path(out);
    return true;
}

static int run_external_command(char *cmd, char *args)
{
    char runfile_path[512];
    if (!resolve_external_command_path(cmd, runfile_path, sizeof(runfile_path))) return 0;

    char *argv[32];
    build_external_argv(runfile_path, args, argv, 32);
    // xapi_OutputSerial((char *)"[busybox-debug] shell run_external path=");
    // xapi_OutputSerial(runfile_path);
    // xapi_OutputSerial((char *)"\n");
    // for (int i = 0; argv[i] != NULL; i++)
    // {
    //     xapi_OutputSerial((char *)"[busybox-debug] shell argv[");
    //     char idx[16];
    //     snprintf(idx, sizeof(idx), "%d", i);
    //     xapi_OutputSerial(idx);
    //     xapi_OutputSerial((char *)"]=");
    //     xapi_OutputSerial(argv[i]);
    //     xapi_OutputSerial((char *)"\n");
    // }

    int64_t pid = (int64_t)xapi_RunArgs(runfile_path, argv);
    if (pid < 0)
    {
        print_to_console("run failed");
        return -1;
    }
    return (int)pid;
}

void check_command()
{
    if (server_mode)
    {
        input_is_enter = true;
        return;
    }

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

    bool paint_prompt = true;

    if (strncmp("version", cmd, 7) == 0)
    {
        print_to_console("XINGJI SpaceTerminal for XJ380 [Release Edition]");
        print_to_console("Version 1.0.0 - 2026/10/4   Platform: Only for XJ380");
        print_to_console("Copyright(C) XINGJI Interactive Software 2017-2025 All rights reserved.");
    }
    else if (strncmp("dir", cmd, 3) == 0 || strncmp("ls", cmd, 2) == 0) { print_dir(); }
    else if (strncmp("fork", cmd, 4) == 0)
    {
        uint64_t pid = xapi_Fork();
        xapi_Execve("/system/shell.elf", NULL, NULL);
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
        if (args == NULL || *args == '\0') { change_directory((char *)"/"); }
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
        //会PF
        // char *src = args;
        // char *dst = NULL;

        // if (src)
        // {
        //     while (*src && *src != ' ')
        //         src++;
        //     if (*src)
        //     {
        //         *src = '\0';
        //         src  = args;
        //         dst  = src + strlen(src) + 1;
        //         while (*dst == ' ')
        //             dst++;
        //     }
        // }

        // if (src == NULL || dst == NULL || *dst == '\0')
        // {
        //     print_to_console("mv: missing operand");
        //     print_to_console("Usage: mv source destination");
        // }
        // else
        // {
        //     move_file(src, dst);
        // }
    }
    else if (strncmp("pwd", cmd, 3) == 0) { print_working_directory(); }
    else if (strncmp("clear", cmd, 5) == 0) { clear_screen(); }
    else if (strncmp("taskmgr", cmd, 7) == 0) { xapi_Run((char *)"/system/taskmgr.elf"); }
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
        print_to_console("  taskmgr     - Open task manager");
        print_to_console("  <program> [args...] - Run executable with arguments");
    }
    else if (strncmp("../", cmd, 3) == 0) { change_directory(cmd); }
    else if (strncmp("edit", cmd, 4) == 0)
    {
        // xapi_Execve("/system/shell.elf",NULL,NULL);
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
    }else if(strncmp("RP++", cmd, 4)==0)
    {
        print_to_console("Your RP: ");
        rp++;
        char buf[1024];
        uint64_to_string(rp,buf,sizeof(buf));
        print_to_console(buf);
        print_to_console_zh(" 祝你2025 CSP J/S 一等奖!\n");
    }
    else
    {
        int external_pid = run_external_command(cmd, args);
        if (external_pid > 0)
        {
            shell_running_external_pid = external_pid;
            shell_prompt_pending = true;
            paint_prompt = false;
        }
        else if (external_pid < 0)
        {
        }
        else if (check_dir_com(cmd) == 2)
        {
            strcat(current_path, "/");
            strcat(current_path, cmd);
        }
        else if (p_inbuf > 0)
        {
            print_to_console_zh("不存在的命令或路径。请检查您的输入是否有误并再试一次。");
            print_to_console("Bad command. Please check your command and try again.");
        }
    }

    p_inbuf = 0;
    if (paint_prompt) { paint_arrow_sign(2, cur_y + 4); }
    command_ck_lock = false;
}
