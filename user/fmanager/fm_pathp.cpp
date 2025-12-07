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
    
    while (*p != '.' && *p != '\0' && *p != '/' && p != name)
        p--;

    p++;

    while (*p)
    {
        *q = *p;
        p++; q++;
    }
    
    return true;
}
