#include "st_proto.h"

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
            if (fdir[i].filetype != 1) { return 1; }
            else { return 2; }
        }
    }
    return 0;
}

void create_directory(char *path)
{
    char  full_path[512];
    bool  recursive   = false;
    char *actual_path = path;

    if (strncmp(path, "-p ", 3) == 0)
    {
        recursive   = true;
        actual_path = path + 3;
        while (*actual_path == ' ')
            actual_path++;
    }

    if (actual_path[0] == '/')
    {
        strcpy(full_path, actual_path);
    }
    else
    {
        strcpy(full_path, current_path);
        if (full_path[strlen(full_path) - 1] != '/') { strcat(full_path, "/"); }
        strcat(full_path, actual_path);
    }

    normalize_path(full_path);

    if (recursive)
    {
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

bool create_directory_recursive(char *path)
{
    char  temp_path[512] = "";
    char *token;
    char *rest = path;

    if (path[0] == '/')
    {
        strcat(temp_path, "/");
        rest = path + 1;
    }

    token = strtok(rest, "/");
    while (token != NULL)
    {
        if (strlen(temp_path) > 0 && temp_path[strlen(temp_path) - 1] != '/') { strcat(temp_path, "/"); }
        strcat(temp_path, token);

        uint32_t count;
        DirNode  fdir[256];
        xapi_SearchFile(temp_path, &count, fdir);
        int result = count;

        if (result == 404)
        {
            if (xapi_Mkdir(temp_path) != 0) { return false; }
        }

        token = strtok(NULL, "/");
    }

    return true;
}

void change_directory(char *path)
{
    char new_path[512];
    char temp_path[512];

    if (path[0] == '/')
    {
        strcpy(new_path, path);
    }
    else
    {
        strcpy(new_path, current_path);
        if (new_path[strlen(new_path) - 1] != '/') { strcat(new_path, "/"); }
        strcat(new_path, path);
    }

    normalize_path(new_path);

    strcpy(temp_path, new_path);
    if (strcmp(temp_path, "/") != 0 && temp_path[strlen(temp_path) - 1] == '/')
    {
        temp_path[strlen(temp_path) - 1] = '\0';
    }

    uint32_t count;
    DirNode  fdir[256];
    xapi_SearchFile(temp_path, &count, fdir);
    if (count == 404)
    {
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

    char *token = strtok(path, "/");
    while (token != NULL && part_count < 50)
    {
        if (strcmp(token, "..") == 0)
        {
            if (part_count > 0) { part_count--; }
        }
        else if (strcmp(token, ".") == 0 || strcmp(token, "") == 0)
        {
            // 忽略
        }
        else
        {
            strcpy(parts[part_count], token);
            part_count++;
        }
        token = strtok(NULL, "/");
    }

    for (int i = 0; i < part_count; i++)
    {
        if (i > 0) { strcat(result, "/"); }
        strcat(result, parts[i]);
    }

    if (strcmp(result, "") == 0) { strcpy(result, "/"); }
    strcpy(path, result);
}

void print_file_content(char *filename)
{
    char full_path[512];

    if (filename[0] == '/') { strcpy(full_path, filename); }
    else
    {
        strcpy(full_path, current_path);
        if (full_path[strlen(full_path) - 1] != '/') strcat(full_path, "/");
        strcat(full_path, filename);
    }

    char buffer[4096];
    int  ret = xapi_ReadFile(full_path, buffer, sizeof(buffer) - 1, 0);

    if (ret < 0)
    {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "cat: cannot read '%s': No such file or directory", filename);
        print_to_console(error_msg);
        return;
    }

    buffer[ret] = '\0';

    char *line = buffer;
    char *next_line;
    while ((next_line = strchr(line, '\n')) != NULL)
    {
        *next_line = '\0';
        print_to_console(line);
        line = next_line + 1;
    }

    if (*line != '\0') { print_to_console(line); }
}

void create_file(char *filename)
{
    char full_path[512];

    if (filename[0] == '/') { strcpy(full_path, filename); }
    else
    {
        strcpy(full_path, current_path);
        if (full_path[strlen(full_path) - 1] != '/') strcat(full_path, "/");
        strcat(full_path, filename);
    }

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

    if (path[0] == '/') { strcpy(full_path, path); }
    else
    {
        strcpy(full_path, current_path);
        if (full_path[strlen(full_path) - 1] != '/') strcat(full_path, "/");
        strcat(full_path, path);
    }

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

    char buffer[8192];
    int  bytes_read = xapi_ReadFile(src_path, buffer, sizeof(buffer), 0);

    if (bytes_read < 0)
    {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "cp: cannot copy '%s': No such file", src);
        print_to_console(error_msg);
        return;
    }

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