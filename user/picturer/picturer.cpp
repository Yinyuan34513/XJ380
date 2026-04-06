#include <x3api.h>

static int picturer_main_impl(int argc, char *argv[], char *envp[])
{
    char picpath[256];

    if (argc == 1)
    {
        strcpy(picpath, argv[0]);
    }
    else
    {
        return 0;
    }

    UINT32 w, h = 0;
    xapi_GetPicSize(&w, &h, picpath);

    XWINDOW Winfo;
    HDLE handle;
    Winfo.title = (char *)"图片查看器";
    Winfo.sets  = XWIN_NORMAL;  
    Winfo.width = w;
    Winfo.height = h;
    xapi_CreateWindow(&handle, &Winfo);
    xapi_SetIcon(handle, "/system/icon/picviewer.png");
    xapi_DrawPicture(handle, 0, 0, w, h, picpath);

    xapi_RefreshWindow(handle);

    return 0;
}

extern "C" int picturer_main_cpp(int argc, char *argv[], char *envp[]) asm("_Z4mainiPPcS0_");
extern "C" int picturer_main_cpp(int argc, char *argv[], char *envp[])
{
    return picturer_main_impl(argc, argv, envp);
}
