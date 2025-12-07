#include <x3api.h>
#include <krlibc.h>
#include "fm_proto.h"

char current_path[1024] = "/";

HDLE handle;

int main(int argc, char *argv[], char *envp[])
{
    (void)argc;
    (void)argv;
    (void)envp;

    XWINDOW Winfo;
    Winfo.title  = (char *)"文件管理器";
    Winfo.width  = FMR_X;
    Winfo.height = FMR_Y;
    xapi_CreateWindow(&handle, &Winfo);
    memset(file_ti_index, 0, 2048 * sizeof(FILE_TYPEINFO_INDEX));

    paint_fm_back();
    paint_dir(current_path);

    xapi_RefreshWindow(handle);

    while (1)
    {
        __asm__ __volatile__("pause");
    }
}


