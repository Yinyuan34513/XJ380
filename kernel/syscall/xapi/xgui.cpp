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
#include <graphics/svg.h>
#include <graphics/window/window.h>
#include <krlibc.h>
#include <mm/frame.h>
#include <pci/pci.h>
#include <proto.hpp>
#include <rtc.h>
#include <syscall/pxapi.h>
#include <task/pcb.h>
#include <ttf.h>
#include <graphics/components/button.h>

bool winRD_lock = false;

static bool clip_window_region(WINDOW_HANDLE *handle, uint32_t *x, uint32_t *y, uint32_t *width, uint32_t *height,
                               int *base_x, int *base_y, int *xsize_out)
{
    if (handle == NULL || handle->WindowNodePtr == NULL || handle->WindowNodePtr->w_sheet == NULL) return false;

    int xsize = getXsize(sht_img, handle->WindowNodePtr->w_sheet);
    int ysize = getYsize(sht_img, handle->WindowNodePtr->w_sheet);
    int content_x = 0;
    int content_y = 0;
    int content_w = xsize;
    int content_h = ysize;

    if (handle->WindowNodePtr->type == XWIN_NORMAL)
    {
        content_x = 12;
        content_y = 27;
        content_w -= 24;
        content_h -= 47;
    }

    if (content_w <= 0 || content_h <= 0) return false;
    if (*x >= (uint32_t)content_w || *y >= (uint32_t)content_h) return false;

    if (*width > (uint32_t)(content_w - (int)*x)) *width = (uint32_t)(content_w - (int)*x);
    if (*height > (uint32_t)(content_h - (int)*y)) *height = (uint32_t)(content_h - (int)*y);
    if (*width == 0 || *height == 0) return false;

    *base_x = content_x;
    *base_y = content_y;
    *xsize_out = xsize;
    return true;
}

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
        scheduler_yield();
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
    if (WindowHandle == NULL || WindowHandle->WindowNodePtr == NULL) return false;
    delete_window(xwmii, sht_img, WindowHandle->WindowNodePtr);
    WindowHandle->WindowNodePtr = NULL;

    return true;
}

int do_xapi_SetIcon(uint64_t handle, uint64_t path)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);
    char          *NewPath      = vfs_cwd_path_build((char *)path);
    
    change_task_dock_icon(WindowHandle->WindowNodePtr, NewPath);

    free(NewPath);

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
        x %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->width - 12;
        y %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->height - 27;
        draw_point(sht_img, WindowHandle->WindowNodePtr->w_sheet, x + 12, y + 27,
                   {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF),
                    (uint8_t)((color) & 0xFF)});
    }
    else
    {
        x %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->width;
        y %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->height;
        draw_point(sht_img, WindowHandle->WindowNodePtr->w_sheet, x, y,
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
        x1 %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->width - 12;
        x2 %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->width - 12;
        y1 %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->height - 27;
        y2 %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->height - 27;
        draw_line(sht_img, WindowHandle->WindowNodePtr->w_sheet, x1 + 12, y1 + 27, x2 + 12, y2 + 27,
                  {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF),
                   (uint8_t)((color) & 0xFF)});
    }
    else
    {
        x1 %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->width;
        x2 %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->width;
        y1 %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->height;
        y2 %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->height;
        draw_line(sht_img, WindowHandle->WindowNodePtr->w_sheet, x1, y1, x2, y2,
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
        x1 %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->width - 12;
        x2 %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->width - 12;
        y1 %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->height - 27;
        y2 %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->height - 27;
        draw_rect(sht_img, WindowHandle->WindowNodePtr->w_sheet, x1 + 12, y1 + 27, x2 + 12, y2 + 27,
                  {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF),
                   (uint8_t)((color) & 0xFF)});
    }
    else
    {
        x1 %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->width;
        x2 %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->width;
        y1 %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->height;
        y2 %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->height;
        draw_rect(sht_img, WindowHandle->WindowNodePtr->w_sheet, x1, y1, x2, y2,
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
        x1 %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->width - 12;
        x2 %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->width - 12;
        y1 %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->height - 27;
        y2 %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->height - 27;
        draw_line(sht_img, WindowHandle->WindowNodePtr->w_sheet, x1 + 12, y1 + 27, x2 + 12, y1 + 27,
                  {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF),
                   (uint8_t)((color) & 0xFF)});
        draw_line(sht_img, WindowHandle->WindowNodePtr->w_sheet, x1 + 12, y2 + 27, x2 + 12, y2 + 27,
                  {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF),
                   (uint8_t)((color) & 0xFF)});

        draw_line(sht_img, WindowHandle->WindowNodePtr->w_sheet, x1 + 12, y1 + 27, x1 + 12, y2 + 27,
                  {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF),
                   (uint8_t)((color) & 0xFF)});
        draw_line(sht_img, WindowHandle->WindowNodePtr->w_sheet, x2 + 12, y1 + 27, x2 + 12, y2 + 27,
                  {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF),
                   (uint8_t)((color) & 0xFF)});
    }
    else
    {
        x1 %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->width;
        x2 %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->width;
        y1 %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->height;
        y2 %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->height;
        draw_line(sht_img, WindowHandle->WindowNodePtr->w_sheet, x1, y1, x2, y1,
                  {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF),
                   (uint8_t)((color) & 0xFF)});
        draw_line(sht_img, WindowHandle->WindowNodePtr->w_sheet, x1, y2, x2, y2,
                  {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF),
                   (uint8_t)((color) & 0xFF)});

        draw_line(sht_img, WindowHandle->WindowNodePtr->w_sheet, x1, y1, x1, y2,
                  {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF),
                   (uint8_t)((color) & 0xFF)});
        draw_line(sht_img, WindowHandle->WindowNodePtr->w_sheet, x2, y1, x2, y2,
                  {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF),
                   (uint8_t)((color) & 0xFF)});
    }

    return true;
}

// int do_xapi_DrawCircle_fill(uint64_t handle, uint32_t x, uint32_t y, uint32_t radius, uint32_t color) {
//     WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);

//     draw_circle(sht_img, WindowHandle->WindowNodePtr->w_sheet, x + 5, y + 22, radius, true, {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF), (uint8_t)((color) & 0xFF)});
// }

// int do_xapi_DrawCircle(uint64_t handle, uint32_t x, uint32_t y, uint32_t radius, uint32_t color) {
//     WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);

//     draw_circle(sht_img, WindowHandle->WindowNodePtr->w_sheet, x + 5, y + 22, radius, false, {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF), (uint8_t)((color) & 0xFF)});
// }

int do_xapi_DrawText(uint64_t handle, uint32_t x, uint32_t y, uint64_t str, uint32_t size, uint32_t color)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);
    char          *TextString   = (char *)(str);
    if (WindowHandle->WindowNodePtr->type == XWIN_NORMAL)
    {
        x %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->width - 12;
        y %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->height - 27;
        draw_text(xwmii, sht_img, WindowHandle->WindowNodePtr, TextString, x + 12, y + 27, size,
                  {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF),
                   (uint8_t)((color) & 0xFF)});
    }
    else
    {
        x %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->width;
        y %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->height;
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
        x %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->width - 12;
        y %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->height - 27;
        draw_textl(xwmii, sht_img, WindowHandle->WindowNodePtr, TextString, x + 12, y + 27, size,
                   {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF),
                    (uint8_t)((color) & 0xFF)},
                   input_width);
    }
    else
    {
        x %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->width;
        y %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->height;
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
        x %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->width - 12;
        y %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->height - 27;
        draw_text_sw(xwmii, sht_img, WindowHandle->WindowNodePtr, TextString, x + 12, y + 27, size,
                     {(uint8_t)((color >> 24) & 0xFF), (uint8_t)((color >> 16) & 0xFF), (uint8_t)((color >> 8) & 0xFF),
                      (uint8_t)((color) & 0xFF)});
    }
    else
    {
        x %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->width;
        y %= found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet)->height;
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
    SHEET_BUFFER     *src          = (SHEET_BUFFER *)get_sheet_buffer(sht_img, WindowHandle->WindowNodePtr->w_sheet);
    int               xsize        = 0;
    int               base_x       = 0;
    int               base_y       = 0;
    if (!clip_window_region(WindowHandle, &x, &y, &width, &height, &base_x, &base_y, &xsize)) return;
    for (int wy = 0; wy < (int)height; wy++)
    {
        SHEET_BUFFER_WOA *dst_row = &buf[wy * width];
        SHEET_BUFFER *src_row = &src[(wy + y + base_y) * xsize + (x + base_x)];
        for (int wx = 0; wx < (int)width; wx++)
        {
            dst_row[wx].r = src_row[wx].r;
            dst_row[wx].g = src_row[wx].g;
            dst_row[wx].b = src_row[wx].b;
        }
    }
}

void do_xapi_WriteBuffer(uint64_t handle, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint64_t buffer)
{
    WINDOW_HANDLE    *WindowHandle = (WINDOW_HANDLE *)(handle);
    SHEET_BUFFER_WOA *buf          = (SHEET_BUFFER_WOA *)buffer;
    SHEET_BUFFER     *src          = (SHEET_BUFFER *)get_sheet_buffer(sht_img, WindowHandle->WindowNodePtr->w_sheet);
    int               xsize        = 0;
    int               base_x       = 0;
    int               base_y       = 0;
    if (!clip_window_region(WindowHandle, &x, &y, &width, &height, &base_x, &base_y, &xsize)) return;
    for (int wy = 0; wy < (int)height; wy++)
    {
        SHEET_BUFFER_WOA *src_row = &buf[wy * width];
        SHEET_BUFFER *dst_row = &src[(wy + y + base_y) * xsize + (x + base_x)];
        for (int wx = 0; wx < (int)width; wx++)
        {
            dst_row[wx].r = src_row[wx].r;
            dst_row[wx].g = src_row[wx].g;
            dst_row[wx].b = src_row[wx].b;
            dst_row[wx].a = 255;
        }
    }
}

void do_xapi_ReadBufferA(uint64_t handle, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint64_t buffer)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);
    SHEET_BUFFER  *buf          = (SHEET_BUFFER *)buffer;
    SHEET_BUFFER  *src          = (SHEET_BUFFER *)get_sheet_buffer(sht_img, WindowHandle->WindowNodePtr->w_sheet);
    int            xsize        = 0;
    int            base_x       = 0;
    int            base_y       = 0;
    if (!clip_window_region(WindowHandle, &x, &y, &width, &height, &base_x, &base_y, &xsize)) return;
    for (int wy = 0; wy < (int)height; wy++)
    {
        SHEET_BUFFER *dst_row = &buf[wy * width];
        SHEET_BUFFER *src_row = &src[(wy + y + base_y) * xsize + (x + base_x)];
        memcpy(dst_row, src_row, (size_t)width * sizeof(SHEET_BUFFER));
    }
}

void do_xapi_WriteBufferA(uint64_t handle, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint64_t buffer)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);
    SHEET_BUFFER  *buf          = (SHEET_BUFFER *)buffer;
    SHEET_BUFFER  *src          = (SHEET_BUFFER *)get_sheet_buffer(sht_img, WindowHandle->WindowNodePtr->w_sheet);
    int            xsize        = 0;
    int            base_x       = 0;
    int            base_y       = 0;
    if (!clip_window_region(WindowHandle, &x, &y, &width, &height, &base_x, &base_y, &xsize)) return;
    for (int wy = 0; wy < (int)height; wy++)
    {
        SHEET_BUFFER *dst_row = &src[(wy + y + base_y) * xsize + (x + base_x)];
        SHEET_BUFFER *src_row = &buf[wy * width];
        memcpy(dst_row, src_row, (size_t)width * sizeof(SHEET_BUFFER));
    }
}

int do_xapi_RefreshWindow(uint64_t handle)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);
    int            bx           = getBX(sht_img, WindowHandle->WindowNodePtr->w_sheet);
    int            by           = getBY(sht_img, WindowHandle->WindowNodePtr->w_sheet);
    int            ex           = bx + getXsize(sht_img, WindowHandle->WindowNodePtr->w_sheet);
    int            ey           = by + getYsize(sht_img, WindowHandle->WindowNodePtr->w_sheet);
    refresh_part_sheet(sht_img, bx, by, ex, ey);

    return true;
}

int do_xapi_RefreshPartWindow(uint64_t handle, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);
    int            bx           = getBX(sht_img, WindowHandle->WindowNodePtr->w_sheet) + x1;
    int            by           = getBY(sht_img, WindowHandle->WindowNodePtr->w_sheet) + y1;
    int            ex           = getBX(sht_img, WindowHandle->WindowNodePtr->w_sheet) + x2;
    int            ey           = getBY(sht_img, WindowHandle->WindowNodePtr->w_sheet) + y2;
    refresh_part_sheet(sht_img, bx, by, ex, ey);

    return true;
}

int xapi_doDrawSvg(uint64_t handle, uint32_t x, uint32_t y, uint32_t width, uint64_t svg_text, uint64_t enable_trans)
{
    WINDOW_HANDLE *window_handle = (WINDOW_HANDLE *)(handle);
    const char    *svg_string    = (const char *)svg_text;
    if (!window_handle || !window_handle->WindowNodePtr || !window_handle->WindowNodePtr->w_sheet || !svg_string)
    {
        return -1;
    }
    if (width == 0) { return -1; }

    SHEET *sheet = found_sheet_byid(sht_img, window_handle->WindowNodePtr->w_sheet);
    if (!sheet) { return -1; }

    int content_x = 0;
    int content_y = 0;
    int content_w = (int)sheet->width;
    int content_h = (int)sheet->height;

    if (window_handle->WindowNodePtr->type == XWIN_NORMAL)
    {
        content_x = 12;
        content_y = 27;
        content_w -= 24;
        content_h -= 47;
    }

    if (content_w <= 0 || content_h <= 0) { return -1; }
    if (x >= (uint32_t)content_w || y >= (uint32_t)content_h) { return -1; }

    uint32_t max_width = (uint32_t)(content_w - (int)x);
    if (width > max_width) { width = max_width; }
    if (width == 0) { return -1; }

    return xapi_drawSvgBySheet(sht_img, window_handle->WindowNodePtr->w_sheet, (int)x + content_x, (int)y + content_y,
                              (int)width, svg_string, enable_trans != 0);
}

int xapi_doDrawFA(uint64_t handle, uint32_t x, uint32_t y, uint32_t width, uint64_t name, uint64_t enable_trans)
{
    WINDOW_HANDLE *window_handle = (WINDOW_HANDLE *)(handle);
    const char    *fa_name       = (const char *)name;
    if (!window_handle || !window_handle->WindowNodePtr || !window_handle->WindowNodePtr->w_sheet || !fa_name)
    {
        return -1;
    }
    if (width == 0) { return -1; }

    SHEET *sheet = found_sheet_byid(sht_img, window_handle->WindowNodePtr->w_sheet);
    if (!sheet) { return -1; }

    int content_x = 0;
    int content_y = 0;
    int content_w = (int)sheet->width;
    int content_h = (int)sheet->height;

    if (window_handle->WindowNodePtr->type == XWIN_NORMAL)
    {
        content_x = 12;
        content_y = 27;
        content_w -= 24;
        content_h -= 47;
    }

    if (content_w <= 0 || content_h <= 0) { return -1; }
    if (x >= (uint32_t)content_w || y >= (uint32_t)content_h) { return -1; }

    uint32_t max_width = (uint32_t)(content_w - (int)x);
    if (width > max_width) { width = max_width; }
    if (width == 0) { return -1; }

    return xapi_drawFABySheet(sht_img, window_handle->WindowNodePtr->w_sheet, (int)x + content_x, (int)y + content_y,
                              (int)width, fa_name, enable_trans != 0);
}

int do_xapi_SetMsgProc(uint64_t handle, uint64_t func)
{
    WINDOW_HANDLE *WindowHandle         = (WINDOW_HANDLE *)(handle);
    init_window_message(WindowHandle->WindowNodePtr, (MsgPrcor)(func));

    return true;
}

void do_xapi_DrawBMP(uint64_t handle, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint64_t path)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);
    char          *filepath     = vfs_cwd_path_build((char *)path);

    if (WindowHandle->WindowNodePtr->type == XWIN_NORMAL)
    {
        PrintPicture_blend(sht_img, WindowHandle->WindowNodePtr->w_sheet, x + 12, y + 27, width, height, filepath);
    }
    else
    {
        PrintPicture_blend(sht_img, WindowHandle->WindowNodePtr->w_sheet, x, y, width, height, filepath);
    }

    free(filepath);
}

void do_xapi_DrawPNG(uint64_t handle, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint64_t path)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);
    char          *filepath     = vfs_cwd_path_build((char *)path);

    if (WindowHandle->WindowNodePtr->type == XWIN_NORMAL)
    {
        PrintPicture_blend(sht_img, WindowHandle->WindowNodePtr->w_sheet, x + 12, y + 27, width, height, filepath);
    }
    else
    {
        PrintPicture_blend(sht_img, WindowHandle->WindowNodePtr->w_sheet, x, y, width, height, filepath);
    }

    free(filepath);
}

void do_xapi_DrawPicture(uint64_t handle, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint64_t path)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);
    char          *filepath     = vfs_cwd_path_build((char *)path);

    if (WindowHandle->WindowNodePtr->type == XWIN_NORMAL)
    {
        PrintPicture_blend(sht_img, WindowHandle->WindowNodePtr->w_sheet, x + 12, y + 27, width, height, filepath);
    }
    else
    {
        PrintPicture_blend(sht_img, WindowHandle->WindowNodePtr->w_sheet, x, y, width, height, filepath);
    }

    free(filepath);
}

void do_xapi_Button(uint64_t handle, uint64_t x, uint64_t y, uint64_t CRLid, uint64_t text)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);
    char          *button_text  = (char *)(text);

    if (WindowHandle->WindowNodePtr->type == XWIN_NORMAL)
    {
        put_button_theme(WindowHandle->WindowNodePtr, x + 12, y + 27, button_text, CRLid, false);
    }
    else
    {
        put_button_theme(WindowHandle->WindowNodePtr, x, y, button_text, CRLid, false);
    }
}

void do_xapi_ButtonEmp(uint64_t handle, uint64_t x, uint64_t y, uint64_t CRLid, uint64_t text)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);
    char          *button_text  = (char *)(text);

    if (WindowHandle->WindowNodePtr->type == XWIN_NORMAL)
    {
        put_button_theme(WindowHandle->WindowNodePtr, x + 12, y + 27, button_text, CRLid, true);
    }
    else
    {
        put_button_theme(WindowHandle->WindowNodePtr, x, y, button_text, CRLid, true);
    }
}

void do_xapi_DeleteButton(uint64_t handle, uint64_t CRLid)
{
    WINDOW_HANDLE *WindowHandle = (WINDOW_HANDLE *)(handle);
    unregister_button_components(found_sheet_byid(sht_img, WindowHandle->WindowNodePtr->w_sheet),
                                 CRLid);
}

void do_xapi_Broken(char *info)
{
    WINDOWLS *BrokenNoticeWindow;
    create_window(xwmii, sht_img, &BrokenNoticeWindow, "Oops!", 400, 100);
    char title[64];
    memset(title, 0, sizeof(char) * 64);
    strcat(title, get_current_task()->name);
    strcat(title, " 崩溃了！");
    char einfo[64];
    memset(einfo, 0, sizeof(char) * 64);
    strcat(einfo, "崩溃信息：");
    strcat(einfo, info);
    print_box_ttf(sht_img, BrokenNoticeWindow->w_sheet, title, BLACK, 20, 0 + 27, 16);
    print_box_ttf(sht_img, BrokenNoticeWindow->w_sheet, einfo, BGRAY, 20, 32 + 27, 10);
    print_box_ttf(sht_img, BrokenNoticeWindow->w_sheet, "您可以尝试向应用程序开发者提供本窗口的信息进行求助。", BGRAY, 20, 48 + 27, 10);

    draw_rect(sht_img, BrokenNoticeWindow->w_sheet, 12, 70 + 27, 411, 126, GRAY);
}

void do_xapi_RegisterRightButtonMenu(uint64_t handle, uint64_t items, uint64_t count)
{
    WINDOW_HANDLE       *WindowHandle = (WINDOW_HANDLE *)(handle);
    RightMenuItem_user  *MenuItems = (RightMenuItem_user *)(items);

    register_right_rb_button_menu(WindowHandle->WindowNodePtr, MenuItems, count);
}

void do_xapi_DeleteRightButtonMenu(uint64_t handle) 
{
    WINDOW_HANDLE       *WindowHandle = (WINDOW_HANDLE *)(handle);

    unregister_right_rb_button_menu_components(WindowHandle->WindowNodePtr);
}
