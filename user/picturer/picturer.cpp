#include <x3api.h>

static HDLE picturer_handle;
static char picturer_path[256];
static UINT32 picturer_width;
static UINT32 picturer_height;
static bool picturer_need_redraw = false;

static void picturer_redraw()
{
    xapi_DrawRect(picturer_handle, 0, 0, picturer_width - 1, picturer_height - 1, 0xffffffff, true);
    xapi_DrawPicture(picturer_handle, 0, 0, picturer_width, picturer_height, picturer_path);
    xapi_RefreshWindow(picturer_handle);
}

static void picturer_MessagePrcor(UINT64 Type, UINT64 hData, UINT64 lData)
{
    if (Type == MSG_RESIZE)
    {
        picturer_width = (UINT32)hData;
        picturer_height = (UINT32)lData;
        picturer_need_redraw = true;
    }
}

static int picturer_main_impl(int argc, char *argv[], char *envp[])
{
    if (argc == 1)
    {
        strcpy(picturer_path, argv[0]);
    }
    else
    {
        return 0;
    }

    UINT32 w, h = 0;
    xapi_GetPicSize(&w, &h, picturer_path);
    picturer_width = w;
    picturer_height = h;

    XWINDOW Winfo;
    Winfo.title = (char *)"图片查看器";
    Winfo.sets  = XWIN_NORMAL | XWIN_SUPPORT_MAXIMIZE;
    Winfo.width = w;
    Winfo.height = h;
    xapi_CreateWindow(&picturer_handle, &Winfo);
    xapi_SetIcon(picturer_handle, "/system/icon/picviewer.png");
    SetMsgPrcor(picturer_handle, picturer_MessagePrcor);
    picturer_redraw();

    while (1)
    {
        if (picturer_need_redraw)
        {
            picturer_need_redraw = false;
            picturer_redraw();
        }
        __asm__ __volatile__("pause");
    }

    return 0;
}

extern "C" int picturer_main_cpp(int argc, char *argv[], char *envp[]) asm("_Z4mainiPPcS0_");
extern "C" int picturer_main_cpp(int argc, char *argv[], char *envp[])
{
    return picturer_main_impl(argc, argv, envp);
}
