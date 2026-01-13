/*
 *
 *      XJ380 Window Manager
 *      Copyright(C) XINGJI Interactive Software 2017-2026 All rights reserved.
 *
 */
#include "./wbuttons.h"
#include <global_color.h>
#include <graphics/window/window.h>
#include <proto.hpp>
#include <ttf.h>
#include <syscall/pxapi.h>

bool WindowIsCreating = false;

int window_create_offest_xy = 32;

void init_xwm(XWM_INFO *xwmi)
{
    write_serial_string("Initializing XWM...\n");
    WINDOWLS *tmp_window = (WINDOWLS *)(malloc(sizeof(WINDOWLS))); // 开辟内存空间
    memset(tmp_window, 0, sizeof(WINDOWLS));
    tmp_window->number = 30000;
    xwmi->start   = tmp_window;
    xwmi->win_num = 0;
    write_serial_string("Reading Theme...\n");
    LoadPictureOgM(&xwmi->theme.top,      "/system/theme/skyglass/top.png");
    LoadPictureOgM(&xwmi->theme.bottom,   "/system/theme/skyglass/bottom.png");
    LoadPictureOgM(&xwmi->theme.left,     "/system/theme/skyglass/left.png");
    LoadPictureOgM(&xwmi->theme.right,    "/system/theme/skyglass/right.png");
    LoadPictureOgM(&xwmi->theme.close,    "/system/theme/skyglass/close.png");
    LoadPictureOgM(&xwmi->theme.min,      "/system/theme/skyglass/min.png");
    write_serial_string("XWM Initializing Success.\n");
}

extern SHEET   *dock_ct_sheet;
extern bool have_full_screen_app;

void create_window_fscr(XWM_INFO *xwmi, SHEET_INFO *sht, WINDOWLS **windowls)
{
    WindowIsCreating = true;
    have_full_screen_app = true;

    WINDOWLS *new_window = (WINDOWLS *)(malloc(sizeof(WINDOWLS))); // 开辟内存空间
    memset(new_window, 0, sizeof(WINDOWLS));

    WINDOWLS *front_p = xwmi->start;
    if (front_p == NULL)
    {
        // 哦亲爱的孩子我不说了你知道这是干什么的
        xwmi->start = new_window;
    }
    else
    {
        WINDOWLS *front_p = xwmi->start;
        while (true)
        {
            if (front_p->next == NULL)
            {
                // 链表.exe
                front_p->next = new_window;
                break;
            }
            front_p = front_p->next;
        }
    }

    *windowls = new_window;
    xwmi->win_num++;
    new_window->WinMPf = NULL;
    new_window->type   = XWIN_FULL_SCR;
    get_current_task()->hasfscr = true;
    get_current_task()->winnum++;

    new_window->width  = sht_img->fbc->horizontal_resolution;
    new_window->height = sht_img->fbc->vertical_resolution;

    new_window->w_task = get_current_task();
    get_current_task()->window_count++;

    // create_sheet(sht, 0, 0, 
    //              sht_img->fbc->horizontal_resolution, sht_img->fbc->vertical_resolution, FixedSheetType, -1, new_window->w_sheet);
    new_window->w_sheet = dock_ct_sheet;
    change_sheet_type(sht, new_window->w_sheet, TopWindowSheetType);
    draw_rect(sht, new_window->w_sheet, 0, 0, sht_img->fbc->horizontal_resolution - 1, sht_img->fbc->vertical_resolution - 1, {0xff, 0xff, 0xff, 0xff});

    refresh_sheet(sht);
    WindowIsCreating = false;
}


void create_window_fmoff(XWM_INFO *xwmi, SHEET_INFO *sht, WINDOWLS **windowls, uint32_t width, uint32_t height)
{
    WindowIsCreating = true;

    WINDOWLS *new_window = (WINDOWLS *)(malloc(sizeof(WINDOWLS))); // 开辟内存空间
    memset(new_window, 0, sizeof(WINDOWLS));

    WINDOWLS *front_p = xwmi->start;
    if (front_p == NULL)
    {
        // 哦亲爱的孩子我不说了你知道这是干什么的
        xwmi->start = new_window;
    }
    else
    {
        WINDOWLS *front_p = xwmi->start;
        while (true)
        {
            if (front_p->next == NULL)
            {
                // 链表.exe
                front_p->next = new_window;
                break;
            }
            front_p = front_p->next;
        }
    }

    *windowls = new_window;
    xwmi->win_num++;
    new_window->WinMPf = NULL;
    new_window->type   = XWIN_FRAME_OFF;

    new_window->width  = width;
    new_window->height = height;

    new_window->w_task = get_current_task();
    get_current_task()->window_count++;

    create_sheet(sht, window_create_offest_xy, window_create_offest_xy, 
                 width + 24, height + 47, NoEdgeWindowSheetType, -1, &new_window->w_sheet);
    draw_rect(sht, new_window->w_sheet, 12, 27, width + 12, height + 26, {0xff, 0xff, 0xff, 0xff});

    window_create_offest_xy %= sht_img->fbc->vertical_resolution / 2;
    window_create_offest_xy += 32;

    refresh_part_sheet(sht, window_create_offest_xy, window_create_offest_xy, 
        width + 24 + window_create_offest_xy, height + 52 + window_create_offest_xy);
    WindowIsCreating = false;
}


void create_window(XWM_INFO *xwmi, SHEET_INFO *sht, WINDOWLS **windowls, CHAR8 *title, uint32_t width, uint32_t height)
{
    WindowIsCreating = true;

    WINDOWLS *new_window = (WINDOWLS *)(malloc(sizeof(WINDOWLS))); // 开辟内存空间
    memset(new_window, 0, sizeof(WINDOWLS));

    WINDOWLS *front_p = xwmi->start;
    if (front_p == NULL)
    {
        // 哦亲爱的孩子我不说了你知道这是干什么的
        xwmi->start = new_window;
    }
    else
    {
        WINDOWLS *front_p = xwmi->start;
        while (true)
        {
            if (front_p->next == NULL)
            {
                // 链表.exe
                front_p->next = new_window;
                break;
            }
            front_p = front_p->next;
        }
    }

    *windowls = new_window;
    xwmi->win_num++;
    new_window->WinMPf = NULL;
    new_window->type   = XWIN_NORMAL;

    new_window->width  = width;
    new_window->height = height;

    strcpy(new_window->title, title);

    new_window->w_task = get_current_task();
    get_current_task()->window_count++;

    create_sheet(sht, window_create_offest_xy, window_create_offest_xy, 
                 width + 24, height + 47, MovableSheetType, -1, &new_window->w_sheet);
    draw_rect(sht, new_window->w_sheet, 12, 27, width + 12, height + 26, {0xff, 0xff, 0xff, 0xff});

    window_create_offest_xy %= sht_img->fbc->vertical_resolution / 2;
    window_create_offest_xy += 32;

    SHEET_BUFFER *top = (SHEET_BUFFER *)malloc((width + 24) * 27 * 4);
    SHEET_BUFFER *lft = (SHEET_BUFFER *)malloc(height * 12 * 4);

    // 标题栏
    resize_theme_width(top, xwmi->theme.top, width + 24, 336, 27);
    copy_buffer_by_id(sht, new_window->w_sheet, top, 0, 0, width + 24, 27);
    // 底部
    resize_theme_width(top, xwmi->theme.bottom, width + 24, 181, 20);
    copy_buffer_by_id(sht, new_window->w_sheet, top, 0, height + 27, width + 24, 20);
    // 左侧
    resize_theme_height(lft, xwmi->theme.left, 12, height, 600);
    copy_buffer_by_id(sht, new_window->w_sheet, lft, 0, 27, 12, height);
    // 右侧
    resize_theme_height(lft, xwmi->theme.right, 12, height, 600);
    copy_buffer_by_id(sht, new_window->w_sheet, lft, width + 12, 27, 12, height);
    // 关闭 & 最小化按钮
    copy_buffer_by_id_without_alpha(sht, new_window->w_sheet, xwmi->theme.close, width - 21, 5, 41, 18);
    copy_buffer_by_id_without_alpha(sht, new_window->w_sheet, xwmi->theme.min, width - 48, 5, 26, 18);

    // print_box_ttf(sht, new_window->w_sheet, title, {0xd7, 0xd7, 0xd7, 0xff}, 11, 3, 12);
    print_box_ttf(sht, new_window->w_sheet, title, BLACK, 12, 4, 11);

    free(top);
    free(lft);

    refresh_part_sheet(sht, window_create_offest_xy, window_create_offest_xy, 
        width + 24 + window_create_offest_xy, height + 52 + window_create_offest_xy);

    register_task_dock(new_window);

    WindowIsCreating = false;
}

void change_title(XWM_INFO *xwmi, SHEET_INFO *sht, WINDOWLS *windowls, CHAR8 *title)
{
    SHEET_BUFFER *top = (SHEET_BUFFER *)malloc((windowls->width + 24) * 27 * 4);
    resize_theme_width(top, xwmi->theme.top, windowls->width + 24, 336, 27);
    copy_buffer_by_id(sht, windowls->w_sheet, top, 0, 0, windowls->width + 24, 27);
    print_box_ttf(sht, windowls->w_sheet, title, WHITE, 22, 0, 11);
    refresh_part_sheet(sht, getBX(sht, windowls->w_sheet), getBY(sht, windowls->w_sheet),
                       getBX(sht, windowls->w_sheet) + getXsize(sht, windowls->w_sheet) + 10,
                       getBY(sht, windowls->w_sheet) + 22);
}

void draw_text(XWM_INFO *xwmi, SHEET_INFO *sht, WINDOWLS *windowls, CHAR8 *title, int x, int y, int size,
               SHEET_BUFFER color)
{
    print_box_ttf(sht, windowls->w_sheet, title, color, x, y, size);
}

void draw_textl(XWM_INFO *xwmi, SHEET_INFO *sht, WINDOWLS *windowls, CHAR8 *title, int x, int y, int size,
               SHEET_BUFFER color, uint32_t *i_width)
{
    print_box_ttfl(sht, windowls->w_sheet, title, color, x, y, size, i_width);
}

void draw_text_sw(XWM_INFO *xwmi, SHEET_INFO *sht, WINDOWLS *windowls, CHAR8 *title, int x, int y, int size,
               SHEET_BUFFER color)
{
    print_box_ttf_c(sht, windowls->w_sheet, title, color, x, y, 10);
}


void delete_window(XWM_INFO *xwmi, SHEET_INFO *sht, WINDOWLS *windowls)
{
    while (true)
    {
        if (!WindowIsCreating) { break; }
    }

    if (windowls->type == XWIN_FULL_SCR)
    {
        have_full_screen_app = false;
        found_sheet_byid(sht_img, windowls->w_sheet)->type = FixedSheetType;
        memset(found_sheet_byid(sht_img, windowls->w_sheet)->buffer, 
                0, sizeof(SHEET_BUFFER) * 
                found_sheet_byid(sht_img, windowls->w_sheet)->width * 
                found_sheet_byid(sht_img, windowls->w_sheet)->height);
        init_dock(sht_img, windowls->w_sheet);
        init_shortcut_dock(sht_img, windowls->w_sheet);
    }

    WindowIsCreating  = true;
    SHEET    *front_p3 = sht->start;
    int       bx, by, ex, ey;

    unregister_task_dock(windowls);

    while (true)
    {
        if (front_p3 == windowls->w_sheet)
        {
            bx = front_p3->bx;
            by = front_p3->by;
            ex = bx + front_p3->width;
            ey = by + front_p3->height;
            break;
        }
        else if (front_p3->next == NULL)
        {
            write_serial_fmt("Window Delete Faild. (Cannot found sheet) SheetAddress: 0x%x\n", (uint64_t)windowls->w_sheet);
            return;
        }
        front_p3 = front_p3->next;
    }

    WINDOWLS *front_p = xwmi->start;
    WINDOWLS *front_p2;

    while (true)
    {
        if (front_p == windowls)
        {
            front_p2->next = front_p->next;
            windowls->w_task->window_count--;
            if (windowls->type != XWIN_FULL_SCR) 
                delete_sheet(sht, windowls->w_sheet);
            memset(windowls, 0, sizeof(SHEET));
            free((void *)(windowls));
            // xwmi->win_num--;
            refresh_part_sheet(sht_img, bx, by, ex, ey);
            break;
        }
        else if (front_p->next == NULL)
        {
            write_serial_string("Window Delete Faild. (Cannot found window) WindowID: ");
            return;
        }
        front_p2 = front_p;
        front_p  = front_p->next;
    }

    WindowIsCreating = false;
}

WINDOWLS *sht_found_win(XWM_INFO *xwmi, SHEET_INFO *sht, SHEET *csheet)
{
    if (csheet == NULL) { return NULL; }

    WINDOWLS *front_p = xwmi->start;
    if (front_p == NULL) { return NULL; }
    while (true)
    {
        if (front_p->w_sheet == csheet) { return front_p; }
        else if (front_p->next == NULL) { return NULL; }
        front_p = front_p->next;
    }
}

WINDOWLS *thread_found_win(XWM_INFO *xwmi, tcb_t thread)
{
    WINDOWLS *front_p = xwmi->start;
    if (front_p == NULL) { return NULL; }
    while (true)
    {
        if (front_p->w_task == thread) { return front_p; }
        else if (front_p->next == NULL) { return NULL; }
        front_p = front_p->next;
    }
}
