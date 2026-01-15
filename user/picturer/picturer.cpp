#include <x3api.h>

int main(int argc, char *argv[], char *envp[])
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
