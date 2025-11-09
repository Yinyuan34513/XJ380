#include <ahci/ahci.h>
#include <efi/boot.h>
#include <efi/efi.h>
#include <efi/fbc.h>
#include <font.h>
#include <fs/fatfs/fatfs.h>
#include <fs/partition.h>
#include <fs/vfs/devfs.h>
#include <fs/vfs/sys.h>
#include <fs/vfs/vfs.h>
#include <global_color.h>
#include <graphics/GOP.hpp>
#include <graphics/sheet.h>
#include <graphics/window/window.h>
#include <krlibc.h>
#include <mm/frame.h>
#include <pci/pci.h>
#include <proto.hpp>
#include <rtc.h>
#include <syscall/pxapi.h>
#include <task/pcb.h>
#include <ttf.h>

bool winRD_lock = false;

/*
 *
 *  @param handle 指向HDLE的指针，HDLE存储WINDOW_HANDLE的地址，即handle=**WH
 *  @param xwin 窗口参数
 *
 */
int do_xapi_CreateWindow(uint64_t handle, uint64_t xwin)
{
    while (true)
    {
        if (winRD_lock) { break; }
    }
    *(uint64_t *)(handle)       = (uint64_t)(malloc(sizeof(WINDOW_HANDLE))); // HDLE
    WINDOW_HANDLE *WindowHandle = *(WINDOW_HANDLE **)(handle);
    XWINDOW       *WindowInfo   = (XWINDOW *)(xwin);
    if (WindowInfo->sets == XWIN_NORMAL || WindowInfo->sets == NULL)
    {
        create_window(xwmii, sht_img, &WindowHandle->WindowNodePtr, WindowInfo->title, WindowInfo->width,
                      WindowInfo->height);
    }
    else if (WindowInfo->sets == XWIN_FRAME_OFF)
    {
        create_window_fmoff(xwmii, sht_img, &WindowHandle->WindowNodePtr, WindowInfo->width, WindowInfo->height);
    }
    else
    {
        create_window_fscr(xwmii, sht_img, &WindowHandle->WindowNodePtr);
    }

    return true;
}

int do_xapi_SetWindowTitle(uint64_t handle, uint64_t str)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);
    char          *WindowTitle  = (char *)(str);
    change_title(xwmii, sht_img, WindowHandle->WindowNodePtr, WindowTitle);

    return true;
}

int do_xapi_CloseWindow(uint64_t handle)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);
    delete_window(xwmii, sht_img, WindowHandle->WindowNodePtr);

    return true;
}

int do_xapi_SetIcon(uint64_t handle, uint64_t path)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);
    char          *WindowTitle  = (char *)(path);
    // 你也给我等着

    return true;
}

int do_xapi_GetWindowSize(uint64_t handle, uint64_t w, uint64_t h)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);
    uint64_t      *width        = (uint64_t *)(w);
    uint64_t      *height       = (uint64_t *)(h);

    *width  = WindowHandle->WindowNodePtr->width;
    *height = WindowHandle->WindowNodePtr->height;

    return true;
}

int do_xapi_DrawPoint(uint64_t handle, uint32_t x, uint32_t y, uint32_t color)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);
    if (WindowHandle->WindowNodePtr->type == XWIN_NORMAL)
    {
        draw_point(sht_img, &(WindowHandle->WindowNodePtr->w_sheet), x + 12, y + 27,
                   {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF),
                    (uint8_t)((color) & 0xFF)});
    }
    else
    {
        draw_point(sht_img, &(WindowHandle->WindowNodePtr->w_sheet), x, y,
                   {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF),
                    (uint8_t)((color) & 0xFF)});
    }

    return true;
}

int do_xapi_DrawLine(uint64_t handle, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t color)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);
    if (WindowHandle->WindowNodePtr->type == XWIN_NORMAL)
    {
        draw_line(sht_img, &(WindowHandle->WindowNodePtr->w_sheet), x1 + 12, y1 + 27, x2 + 12, y2 + 27,
                  {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF),
                   (uint8_t)((color) & 0xFF)});
    }
    else
    {
        draw_line(sht_img, &(WindowHandle->WindowNodePtr->w_sheet), x1, y1, x2, y2,
                  {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF),
                   (uint8_t)((color) & 0xFF)});
    }

    return true;
}

int do_xapi_DrawRect_fill(uint64_t handle, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t color)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);
    if (WindowHandle->WindowNodePtr->type == XWIN_NORMAL)
    {
        draw_rect(sht_img, &(WindowHandle->WindowNodePtr->w_sheet), x1 + 12, y1 + 27, x2 + 12, y2 + 27,
                  {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF),
                   (uint8_t)((color) & 0xFF)});
    }
    else
    {
        draw_rect(sht_img, &(WindowHandle->WindowNodePtr->w_sheet), x1, y1, x2, y2,
                  {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF),
                   (uint8_t)((color) & 0xFF)});
    }

    return true;
}

int do_xapi_DrawRect(uint64_t handle, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t color)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);

    if (WindowHandle->WindowNodePtr->type == XWIN_NORMAL)
    {
        draw_line(sht_img, &(WindowHandle->WindowNodePtr->w_sheet), x1 + 12, y1 + 27, x2 + 12, y1 + 27,
                  {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF),
                   (uint8_t)((color) & 0xFF)});
        draw_line(sht_img, &(WindowHandle->WindowNodePtr->w_sheet), x1 + 12, y2 + 27, x2 + 12, y2 + 27,
                  {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF),
                   (uint8_t)((color) & 0xFF)});

        draw_line(sht_img, &(WindowHandle->WindowNodePtr->w_sheet), x1 + 12, y1 + 27, x1 + 12, y2 + 27,
                  {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF),
                   (uint8_t)((color) & 0xFF)});
        draw_line(sht_img, &(WindowHandle->WindowNodePtr->w_sheet), x2 + 12, y1 + 27, x2 + 12, y2 + 27,
                  {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF),
                   (uint8_t)((color) & 0xFF)});
    }
    else
    {
        draw_line(sht_img, &(WindowHandle->WindowNodePtr->w_sheet), x1, y1, x2, y1,
                  {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF),
                   (uint8_t)((color) & 0xFF)});
        draw_line(sht_img, &(WindowHandle->WindowNodePtr->w_sheet), x1, y2, x2, y2,
                  {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF),
                   (uint8_t)((color) & 0xFF)});

        draw_line(sht_img, &(WindowHandle->WindowNodePtr->w_sheet), x1, y1, x1, y2,
                  {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF),
                   (uint8_t)((color) & 0xFF)});
        draw_line(sht_img, &(WindowHandle->WindowNodePtr->w_sheet), x2, y1, x2, y2,
                  {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF),
                   (uint8_t)((color) & 0xFF)});
    }

    return true;
}

// int do_xapi_DrawCircle_fill(uint64_t handle, uint32_t x, uint32_t y, uint32_t radius, uint32_t color) {
//     WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);

//     draw_circle(sht_img, &(WindowHandle->WindowNodePtr->w_sheet), x + 5, y + 22, radius, true, {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF), (uint8_t)((color) & 0xFF)});
// }

// int do_xapi_DrawCircle(uint64_t handle, uint32_t x, uint32_t y, uint32_t radius, uint32_t color) {
//     WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);

//     draw_circle(sht_img, &(WindowHandle->WindowNodePtr->w_sheet), x + 5, y + 22, radius, false, {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF), (uint8_t)((color) & 0xFF)});
// }

int do_xapi_DrawText(uint64_t handle, uint32_t x, uint32_t y, uint64_t str, uint32_t size, uint32_t color)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);
    char          *TextString   = (char *)(str);
    if (WindowHandle->WindowNodePtr->type == XWIN_NORMAL)
    {
        draw_text(xwmii, sht_img, WindowHandle->WindowNodePtr, TextString, x + 12, y + 27, size,
                  {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF),
                   (uint8_t)((color) & 0xFF)});
    }
    else
    {
        draw_text(xwmii, sht_img, WindowHandle->WindowNodePtr, TextString, x, y, size,
                  {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF),
                   (uint8_t)((color) & 0xFF)});
    }
    return true;
}

int do_xapi_DrawTextl(uint64_t handle, uint64_t place, uint64_t str, uint32_t size, uint32_t color, uint64_t i_width)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);
    char          *TextString   = (char *)(str);
    uint32_t      *input_width  = (uint32_t *)(i_width);
    uint32_t       x            = place & 0xffffffff00000000;
    uint32_t       y            = place & 0x00000000ffffffff;
    if (WindowHandle->WindowNodePtr->type == XWIN_NORMAL)
    {
        draw_textl(xwmii, sht_img, WindowHandle->WindowNodePtr, TextString, x + 12, y + 27, size,
                   {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF),
                    (uint8_t)((color) & 0xFF)},
                   input_width);
    }
    else
    {
        draw_textl(xwmii, sht_img, WindowHandle->WindowNodePtr, TextString, x, y, size,
                   {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF),
                    (uint8_t)((color) & 0xFF)},
                   input_width);
    }
    return true;
}

int do_xapi_DrawSWText(uint64_t handle, uint32_t x, uint32_t y, uint64_t str, uint32_t size, uint32_t color)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);
    char          *TextString   = (char *)(str);
    if (WindowHandle->WindowNodePtr->type == XWIN_NORMAL)
    {
        draw_text_sw(xwmii, sht_img, WindowHandle->WindowNodePtr, TextString, x + 12, y + 27, size,
                     {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF),
                      (uint8_t)((color) & 0xFF)});
    }
    else
    {
        draw_text_sw(xwmii, sht_img, WindowHandle->WindowNodePtr, TextString, x, y, size,
                     {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF),
                      (uint8_t)((color) & 0xFF)});
    }
    return true;
}

void do_xapi_ReadBuffer(uint64_t handle, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint64_t buffer)
{
    WINDOW_HANDLE    *WindowHandle = (WINDOW_HANDLE *)(handle);
    SHEET_BUFFER_WOA *buf          = (SHEET_BUFFER_WOA *)buffer;
    SHEET_BUFFER     *src          = (SHEET_BUFFER *)get_sheet_buffer(sht_img, &(WindowHandle->WindowNodePtr->w_sheet));
    int               xsize        = getXsize(sht_img, &(WindowHandle->WindowNodePtr->w_sheet));
    int               ysize        = getYsize(sht_img, &(WindowHandle->WindowNodePtr->w_sheet));
    if (WindowHandle->WindowNodePtr->type == XWIN_NORMAL)
    {
        for (int wy = 0; wy < height; wy++)
        {
            for (int wx = 0; wx < width; wx++)
            {
                buf[wy * width + wx].r = src[(wy + y + 27) * xsize + (wx + x + 12)].r;
                buf[wy * width + wx].g = src[(wy + y + 27) * xsize + (wx + x + 12)].g;
                buf[wy * width + wx].b = src[(wy + y + 27) * xsize + (wx + x + 12)].b;
            }
        }
    }
    else
    {
        for (int wy = 0; wy < height; wy++)
        {
            for (int wx = 0; wx < width; wx++)
            {
                buf[wy * width + wx].r = src[(wy + y) * xsize + (wx + x)].r;
                buf[wy * width + wx].g = src[(wy + y) * xsize + (wx + x)].g;
                buf[wy * width + wx].b = src[(wy + y) * xsize + (wx + x)].b;
            }
        }
    }
}

void do_xapi_WriteBuffer(uint64_t handle, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint64_t buffer)
{
    WINDOW_HANDLE    *WindowHandle = (WINDOW_HANDLE *)(handle);
    SHEET_BUFFER_WOA *buf          = (SHEET_BUFFER_WOA *)buffer;
    SHEET_BUFFER     *src          = (SHEET_BUFFER *)get_sheet_buffer(sht_img, &(WindowHandle->WindowNodePtr->w_sheet));
    int               xsize        = getXsize(sht_img, &(WindowHandle->WindowNodePtr->w_sheet));
    int               ysize        = getYsize(sht_img, &(WindowHandle->WindowNodePtr->w_sheet));
    if (WindowHandle->WindowNodePtr->type == XWIN_NORMAL)
    {
        for (int wy = 0; wy < height; wy++)
        {
            for (int wx = 0; wx < width; wx++)
            {
                src[(wy + y + 27) * xsize + (wx + x + 12)].r = buf[wy * width + wx].r;
                src[(wy + y + 27) * xsize + (wx + x + 12)].g = buf[wy * width + wx].g;
                src[(wy + y + 27) * xsize + (wx + x + 12)].b = buf[wy * width + wx].b;
                src[(wy + y + 27) * xsize + (wx + x + 12)].a = 255;
            }
        }
    }
    else
    {
        for (int wy = 0; wy < height; wy++)
        {
            for (int wx = 0; wx < width; wx++)
            {
                src[(wy + y) * xsize + (wx + x)].r = buf[wy * width + wx].r;
                src[(wy + y) * xsize + (wx + x)].g = buf[wy * width + wx].g;
                src[(wy + y) * xsize + (wx + x)].b = buf[wy * width + wx].b;
                src[(wy + y) * xsize + (wx + x)].a = 255;
            }
        }
    }
}

void do_xapi_ReadBufferA(uint64_t handle, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint64_t buffer)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);
    SHEET_BUFFER  *buf          = (SHEET_BUFFER *)buffer;
    SHEET_BUFFER  *src          = (SHEET_BUFFER *)get_sheet_buffer(sht_img, &(WindowHandle->WindowNodePtr->w_sheet));
    int            xsize        = getXsize(sht_img, &(WindowHandle->WindowNodePtr->w_sheet));
    int            ysize        = getYsize(sht_img, &(WindowHandle->WindowNodePtr->w_sheet));
    if (WindowHandle->WindowNodePtr->type == XWIN_NORMAL)
    {
        for (int wy = 0; wy < height; wy++)
        {
            for (int wx = 0; wx < width; wx++)
            {
                buf[wy * width + wx] = src[(wy + y + 27) * xsize + (wx + x + 12)];
            }
        }
    }
    else
    {
        for (int wy = 0; wy < height; wy++)
        {
            for (int wx = 0; wx < width; wx++)
            {
                buf[wy * width + wx] = src[(wy + y) * xsize + (wx + x)];
            }
        }
    }
}

void do_xapi_WriteBufferA(uint64_t handle, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint64_t buffer)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);
    SHEET_BUFFER  *buf          = (SHEET_BUFFER *)buffer;
    SHEET_BUFFER  *src          = (SHEET_BUFFER *)get_sheet_buffer(sht_img, &(WindowHandle->WindowNodePtr->w_sheet));
    int            xsize        = getXsize(sht_img, &(WindowHandle->WindowNodePtr->w_sheet));
    int            ysize        = getYsize(sht_img, &(WindowHandle->WindowNodePtr->w_sheet));
    if (WindowHandle->WindowNodePtr->type == XWIN_NORMAL)
    {
        for (int wy = 0; wy < height; wy++)
        {
            for (int wx = 0; wx < width; wx++)
            {
                src[(wy + y + 27) * xsize + (wx + x + 12)] = buf[wy * width + wx];
            }
        }
    }
    else
    {
        for (int wy = 0; wy < height; wy++)
        {
            for (int wx = 0; wx < width; wx++)
            {
                src[(wy + y) * xsize + (wx + x)] = buf[wy * width + wx];
            }
        }
    }
}

int do_xapi_RefreshWindow(uint64_t handle)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);
    int            bx           = getBX(sht_img, &(WindowHandle->WindowNodePtr->w_sheet));
    int            by           = getBY(sht_img, &(WindowHandle->WindowNodePtr->w_sheet));
    int            ex           = bx + getXsize(sht_img, &(WindowHandle->WindowNodePtr->w_sheet));
    int            ey           = by + getYsize(sht_img, &(WindowHandle->WindowNodePtr->w_sheet));
    refresh_part_sheet(sht_img, bx, by, ex, ey);

    return true;
}

int do_xapi_SetMsgProc(uint64_t handle, uint64_t func)
{
    WINDOW_HANDLE *WindowHandle         = (WINDOW_HANDLE *)(handle);
    WindowHandle->WindowNodePtr->WinMPf = (MsgPrcor)(func);

    return true;
}

void do_xapi_DrawBMP(uint64_t handle, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint64_t path)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);
    char          *filepath     = (char *)(path);

    if (WindowHandle->WindowNodePtr->type == XWIN_NORMAL)
    {
        PrintPicture_blend(sht_img, &(WindowHandle->WindowNodePtr->w_sheet), x + 12, y + 27, width, height, filepath);
    }
    else
    {
        PrintPicture_blend(sht_img, &(WindowHandle->WindowNodePtr->w_sheet), x, y, width, height, filepath);
    }
}

void do_xapi_DrawPNG(uint64_t handle, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint64_t path)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);
    char          *filepath     = (char *)(path);

    if (WindowHandle->WindowNodePtr->type == XWIN_NORMAL)
    {
        PrintPicture_blend(sht_img, &(WindowHandle->WindowNodePtr->w_sheet), x + 12, y + 27, width, height, filepath);
    }
    else
    {
        PrintPicture_blend(sht_img, &(WindowHandle->WindowNodePtr->w_sheet), x, y, width, height, filepath);
    }
}

void do_xapi_DrawPicture(uint64_t handle, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint64_t path)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);
    char          *filepath     = (char *)(path);

    if (WindowHandle->WindowNodePtr->type == XWIN_NORMAL)
    {
        PrintPicture_blend(sht_img, &(WindowHandle->WindowNodePtr->w_sheet), x + 12, y + 27, width, height, filepath);
    }
    else
    {
        PrintPicture_blend(sht_img, &(WindowHandle->WindowNodePtr->w_sheet), x, y, width, height, filepath);
    }
}
