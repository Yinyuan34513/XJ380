#include "st_proto.h"
#include "gh_sign.h"
#include <stdint.h>

uint64_t rp = 0;

extern bool input_is_enter; // shell_tam.cpp
extern bool server_mode; // shell_main.cpp

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
    bool should_paint_prompt = true;

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
    else if (strncmp("taskmgr", cmd, 7) == 0)
    {
        should_paint_prompt = false;
        int status = 0;
        uint64_t pid = fork();
        if ((int64_t)pid < 0)
        {
            should_paint_prompt = true;
            print_to_console("taskmgr: failed to start process");
        }
        else if (pid == 0)
        {
            char *taskmgr_argv[] = {(char *)"/system/taskmgr.elf", NULL};
            xapi_Execve("/system/taskmgr.elf", taskmgr_argv, NULL);
            exit(127);
        }
        else
        {
            waitpid((int)pid, &status, 0);
        }
    }
    else if (strcmp(cmd, "nut") == 0)
    {
        char *nut_argv[16] = {0};
        int   nut_argc = 1;

        nut_argv[0] = (char *)"/apps/nut.elf";

        if (args != NULL && *args != '\0')
        {
            char *p = args;
            while (*p != '\0' && nut_argc < 15)
            {
                while (*p == ' ')
                {
                    p++;
                }

                if (*p == '\0')
                {
                    break;
                }

                nut_argv[nut_argc++] = p;

                while (*p != '\0' && *p != ' ')
                {
                    p++;
                }

                if (*p == '\0')
                {
                    break;
                }

                *p = '\0';
                p++;
            }
        }

        nut_argv[nut_argc] = NULL;

        should_paint_prompt = false;
        int status = 0;
        uint64_t pid = fork();
        if ((int64_t)pid < 0)
        {
            should_paint_prompt = true;
            print_to_console("nut: failed to start process");
        }
        else if (pid == 0)
        {
            uint64_t exec_ret = xapi_Execve("/apps/nut.elf", nut_argv, NULL);
            char     nut_log[128];
            snprintf(nut_log, sizeof(nut_log), "nut: execve failed ret=%d", (int)((int64_t)exec_ret));
            xapi_OutputSerial(nut_log);
            print_to_console(nut_log);
            exit(127);
        }
        else
        {
            waitpid((int)pid, &status, 0);
            should_paint_prompt = true;
        }
    }
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
    }
    else if (strncmp("../", input_buf, 3) == 0) { change_directory(input_buf); }
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
        if (check_dir_com(input_buf) == 1)
        {
            char runfile_path[256];
            memset(runfile_path, 0, sizeof(runfile_path));
            strcat(runfile_path, current_path);
            strcat(runfile_path, "/");
            strcat(runfile_path, input_buf);
            normalize_path(runfile_path);
            should_paint_prompt = false;
            int status = 0;
            uint64_t pid = fork();
            if ((int64_t)pid < 0)
            {
                should_paint_prompt = true;
                print_to_console("run: failed to start process");
            }
            else if (pid == 0)
            {
                char *run_argv[] = {runfile_path, NULL};
                xapi_Execve(runfile_path, run_argv, NULL);
                exit(127);
            }
            else
            {
                waitpid((int)pid, &status, 0);
            }
        }
        else if (check_dir_com(input_buf) == 2)
        {
            strcat(current_path, "/");
            strcat(current_path, input_buf);
        }
        else if (p_inbuf > 0)
        {
            print_to_console_zh("不存在的命令或路径。请检查您的输入是否有误并再试一次。");
            print_to_console("Bad command. Please check your command and try again.");
        }
    }

    p_inbuf = 0;
    if (should_paint_prompt)
    {
        paint_arrow_sign(2, cur_y + 4);
    }
    command_ck_lock = false;
}
