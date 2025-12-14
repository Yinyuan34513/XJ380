#include <x3api.h>
#include <krlibc.h>
#include "fm_proto.h"

bool get_file_type(char *name, char *type)
{
    char *p = name;
    char *q = type;
    bool have_type;
    while (*p)
    {
        p++;
        if (*p == '.')
            have_type = true;
    }

    if (!have_type)
        return false;
    
    while (*p != '.')
        p--;

    p++;

    while (*p)
    {
        *q = *p;
        p++; q++;
    }
    
    return true;
}

void cat_path(char *folder_name)
{
    char *p = current_path;

    while (*p) p++;

    p--;

    if (*p == '/')
    {
        strcat(current_path, folder_name);
    }
    else
    {
        strcat(current_path, "/");
        strcat(current_path, folder_name);
    }
}

void his_path(char * path)
{
    // 已经在历史记录中
    if (strcmp(path, path_his[path_p]) == 0)
        return;
    
    // 移动到下一个位置
    path_p = (path_p + 1) % 20;
    strcpy(path_his[path_p], path);
    path_r = path_p;  // 更新最新位置指针
}