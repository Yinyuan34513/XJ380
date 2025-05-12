#include "../../../include/include.h"

/*
 *
 *  @param handle 指向HDLE的指针，HDLE存储WINDOW_HANDLE的地址，即handle=**WH
 *  @param xwin 窗口参数
 *
 */
int do_xapi_CreateWindow(UINT64 handle, UINT64 xwin)
{
    while (true)
    {
        if (winRD_lock)
        {
            break;
        }
    }
    *(UINT64 *)(handle) = (UINT64)(malloc(sizeof(WINDOW_HANDLE))); // HDLE
    WINDOW_HANDLE *WindowHandle = *(WINDOW_HANDLE **)(handle);
    XWINDOW *WindowInfo = (XWINDOW *)(xwin);
    create_window(xwmii, sht_img,
                  &WindowHandle->WindowNodePtr,
                  WindowInfo->title,
                  WindowInfo->width, WindowInfo->height);

    return true;
}

int do_xapi_SetWindowTitle(UINT64 handle, UINT64 str)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);
    char *WindowTitle = (char *)(str);
    change_title(xwmii, sht_img,
                 WindowHandle->WindowNodePtr,
                 WindowTitle);

    return true;
}

int do_xapi_CloseWindow(UINT64 handle)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);
    // 呃现在还没做删除图层和窗口，等会儿再说吧（（（

    return true;
}

int do_xapi_SetIcon(UINT64 handle, UINT64 path)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);
    char *WindowTitle = (char *)(path);
    // 你也给我等着

    return true;
}

int do_xapi_DrawPoint(UINT64 handle, UINT32 x, UINT32 y, UINT32 color)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);
    draw_point(sht_img, &(WindowHandle->WindowNodePtr->w_sheet), x, y, {(UINT8)((color >> 24) & 0xFF), (UINT8)((color >> 16) & 0xFF), (UINT8)((color >> 8) & 0xFF), (UINT8)((color) & 0xFF)});

    return true;
}

int do_xapi_DrawLine(UINT64 handle, UINT32 x1, UINT32 y1, UINT32 x2, UINT32 y2, UINT32 color)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);
    draw_line(sht_img, &(WindowHandle->WindowNodePtr->w_sheet), x1 + 5, y1 + 22, x2 + 5, y2 + 22, {(UINT8)((color >> 24) & 0xFF), (UINT8)((color >> 16) & 0xFF), (UINT8)((color >> 8) & 0xFF), (UINT8)((color) & 0xFF)});

    return true;
}

int do_xapi_DrawRect_fill(UINT64 handle, UINT32 x1, UINT32 y1, UINT32 x2, UINT32 y2, UINT32 color)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);
    draw_rect(sht_img, &(WindowHandle->WindowNodePtr->w_sheet), x1 + 5, y1 + 22, x2 + 5, y2 + 22, {(UINT8)((color >> 24) & 0xFF), (UINT8)((color >> 16) & 0xFF), (UINT8)((color >> 8) & 0xFF), (UINT8)((color) & 0xFF)});

    return true;
}

int do_xapi_DrawRect(UINT64 handle, UINT32 x1, UINT32 y1, UINT32 x2, UINT32 y2, UINT32 color)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);

    draw_line(sht_img, &(WindowHandle->WindowNodePtr->w_sheet), x1 + 5, y1 + 22, x2 + 5, y1 + 22, {(UINT8)((color >> 24) & 0xFF), (UINT8)((color >> 16) & 0xFF), (UINT8)((color >> 8) & 0xFF), (UINT8)((color) & 0xFF)});
    draw_line(sht_img, &(WindowHandle->WindowNodePtr->w_sheet), x1 + 5, y2 + 22, x2 + 5, y2 + 22, {(UINT8)((color >> 24) & 0xFF), (UINT8)((color >> 16) & 0xFF), (UINT8)((color >> 8) & 0xFF), (UINT8)((color) & 0xFF)});

    draw_line(sht_img, &(WindowHandle->WindowNodePtr->w_sheet), x1 + 5, y1 + 22, x1 + 5, y2 + 22, {(UINT8)((color >> 24) & 0xFF), (UINT8)((color >> 16) & 0xFF), (UINT8)((color >> 8) & 0xFF), (UINT8)((color) & 0xFF)});
    draw_line(sht_img, &(WindowHandle->WindowNodePtr->w_sheet), x2 + 5, y1 + 22, x2 + 5, y2 + 22, {(UINT8)((color >> 24) & 0xFF), (UINT8)((color >> 16) & 0xFF), (UINT8)((color >> 8) & 0xFF), (UINT8)((color) & 0xFF)});

    return true;
}
