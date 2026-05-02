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
#include <ps2/mouse.h>
#include <ttf.h>
#include <syscall/pxapi.h>

bool WindowIsCreating = false;
extern mouse_dec ms_dec;

int window_create_offest_xy = 32;

static bool is_valid_gui_ptr(const void *ptr)
{
    return ptr != NULL && (uint64_t)ptr >= get_physical_memory_offset();
}

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
    int refresh_bx = getBX(sht, new_window->w_sheet);
    int refresh_by = getBY(sht, new_window->w_sheet);

    window_create_offest_xy %= sht_img->fbc->vertical_resolution / 2;
    window_create_offest_xy += 32;

    refresh_part_sheet(sht, refresh_bx, refresh_by, refresh_bx + width + 24, refresh_by + height + 47);
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

    strncpy(new_window->title, title, sizeof(new_window->title) - 1);
    new_window->title[sizeof(new_window->title) - 1] = '\0';

    new_window->w_task = get_current_task();
    get_current_task()->window_count++;

    create_sheet(sht, window_create_offest_xy, window_create_offest_xy, 
                 width + 24, height + 47, MovableSheetType, -1, &new_window->w_sheet);
    draw_rect(sht, new_window->w_sheet, 12, 27, width + 12, height + 26, {0xff, 0xff, 0xff, 0xff});
    int refresh_bx = getBX(sht, new_window->w_sheet);
    int refresh_by = getBY(sht, new_window->w_sheet);

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

    refresh_part_sheet(sht, refresh_bx, refresh_by, refresh_bx + width + 24, refresh_by + height + 47);

    register_task_dock(new_window);

    WindowIsCreating = false;
}

void change_title(XWM_INFO *xwmi, SHEET_INFO *sht, WINDOWLS *windowls, CHAR8 *title)
{
    SHEET_BUFFER *top = (SHEET_BUFFER *)malloc((windowls->width + 24) * 27 * 4);
    resize_theme_width(top, xwmi->theme.top, windowls->width + 24, 336, 27);
    copy_buffer_by_id(sht, windowls->w_sheet, top, 0, 0, windowls->width + 24, 27);
    print_box_ttf(sht, windowls->w_sheet, title, WHITE, 22, 0, 11);
    free(top);
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
    print_box_ttf_c(sht, windowls->w_sheet, title, color, x, y, size);
}


void delete_window(XWM_INFO *xwmi, SHEET_INFO *sht, WINDOWLS *windowls)
{
    while (true)
    {
        if (!WindowIsCreating) { break; }
    }

    if (!is_valid_gui_ptr(xwmi) || !is_valid_gui_ptr(windowls)) return;
    if (!is_valid_gui_ptr(windowls->w_sheet) && windowls->type != XWIN_FULL_SCR) return;

    if (ms_dec.sht_now == windowls->w_sheet)
    {
        ms_dec.sht_now       = NULL;
        ms_dec.win_move_lock = false;
    }
    if (ms_dec.left_release_window == windowls)
    {
        ms_dec.left_release_window  = NULL;
        ms_dec.left_release_pending = false;
    }

    if (windowls->type == XWIN_FULL_SCR)
    {
        have_full_screen_app = false;
        SHEET *fs_sheet = found_sheet_byid(sht_img, windowls->w_sheet);
        if (is_valid_gui_ptr(fs_sheet) && is_valid_gui_ptr(fs_sheet->buffer))
        {
            fs_sheet->type = FixedSheetType;
            memset(fs_sheet->buffer, 0, sizeof(SHEET_BUFFER) * fs_sheet->width * fs_sheet->height);
            init_dock(sht_img, windowls->w_sheet);
            init_shortcut_dock(sht_img, windowls->w_sheet);
        }
    }

    WindowIsCreating  = true;
    SHEET    *front_p3 = sht->start;
    int       bx = 0, by = 0, ex = 0, ey = 0;
    bool      need_refresh = false;

    unregister_right_rb_button_menu_components(windowls);
    unregister_task_dock(windowls);

    while (front_p3 != NULL)
    {
        if (front_p3 == windowls->w_sheet)
        {
            bx = front_p3->bx;
            by = front_p3->by;
            ex = bx + front_p3->width;
            ey = by + front_p3->height;
            need_refresh = true;
            break;
        }
        front_p3 = front_p3->next;
    }

    WINDOWLS *front_p  = xwmi->start;
    WINDOWLS *front_p2 = NULL;
    while (front_p != NULL)
    {
        if (front_p == windowls)
        {
            if (front_p2 == NULL) xwmi->start = front_p->next;
            else front_p2->next = front_p->next;

            if (windowls->w_task != NULL && windowls->w_task->window_count > 0) windowls->w_task->window_count--;
            if (xwmi->win_num > 0) xwmi->win_num--;

            if (windowls->type != XWIN_FULL_SCR)
            {
                delete_sheet(sht, windowls->w_sheet);
            }

            memset(windowls, 0, sizeof(WINDOWLS));
            free((void *)(windowls));
            break;
        }
        front_p2 = front_p;
        front_p  = front_p->next;
    }

    WindowIsCreating = false;

    if (front_p == NULL)
    {
        write_serial_string("Window Delete Faild. (Cannot found window)\n");
        return;
    }

    if (need_refresh) refresh_part_sheet(sht_img, bx, by, ex, ey);
}

void delete_process_windows(XWM_INFO *xwmi, SHEET_INFO *sht, pcb_t process)
{
    if (!is_valid_gui_ptr(xwmi) || sht == NULL || process == NULL) return;

    while (true)
    {
        WINDOWLS *target = NULL;
        WINDOWLS *front_p = xwmi->start;
        while (front_p != NULL)
        {
            if (front_p->w_task != NULL && front_p->w_task->parent_group == process)
            {
                target = front_p;
                break;
            }
            front_p = front_p->next;
        }

        if (target == NULL) break;
        delete_window(xwmi, sht, target);
    }
}

WINDOWLS *sht_found_win(XWM_INFO *xwmi, SHEET_INFO *sht, SHEET *csheet)
{
    if (!is_valid_gui_ptr(xwmi) || csheet == NULL) { return NULL; }

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
    if (!is_valid_gui_ptr(xwmi) || thread == NULL) { return NULL; }
    WINDOWLS *front_p = xwmi->start;
    if (front_p == NULL) { return NULL; }
    while (true)
    {
        if (front_p->w_task == thread) { return front_p; }
        else if (front_p->next == NULL) { return NULL; }
        front_p = front_p->next;
    }
}

WINDOWLS *mpf_found_win(MsgPrcor mpf)
{
    if (mpf == NULL) { return NULL; }

    WINDOWLS *front_p = xwmii->start;
    if (front_p == NULL) { return NULL; }
    while (true)
    {
        if (front_p->WinMPf == mpf) { return front_p; }
        else if (front_p->next == NULL) { return NULL; }
        front_p = front_p->next;
    }
}
