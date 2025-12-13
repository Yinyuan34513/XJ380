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
