#include "../../../include/include.h"

int do_xapi_CreateWindow(UINT64 handle, UINT64 xwin) {
    while (true) {
        if (winRD_lock) 
        {
            break;
        }
    }
    handle = (UINT64)(malloc(sizeof(WINDOW_HANDLE)));
    WINDOW_HANDLE   *WindowHandle   = (WINDOW_HANDLE *)(handle);
    XWINDOW         *WindowInfo     = (XWINDOW *)(xwin);
    create_window(xwmii, sht_img, 
                    WindowHandle->WindowNodePtr, 
                    WindowInfo->title, 
                    WindowInfo->width, WindowInfo->height);
    
    return true;
}

int do_xapi_SetWindowTitle(UINT64 handle, UINT64 str) {
    handle = (UINT64)(malloc(sizeof(WINDOW_HANDLE)));
    WINDOW_HANDLE   *WindowHandle   = (WINDOW_HANDLE *)(handle);
    char            *WindowTitle    = (char *)(str);
    change_title(xwmii, sht_img, 
                    WindowHandle->WindowNodePtr,
                    WindowTitle);
    
    return true;
}
