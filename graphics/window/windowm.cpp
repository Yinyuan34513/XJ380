/*
 *
 *      XJ380 Window Manager
 *      Copyright(C) XINGJI Interactive Software 2017-2025 All rights reserved.
 *
 */
#include "../../include/include.h"
#include "./wbuttons.h"

#define WINDOW_ALPHA_VALUE 0xaa

bool WindowIsCreating = false;

void init_xwm(XWM_INFO *xwmi)
{
    write_serial_string("Initializing XWM...\n");
    xwmi->start = NULL;
    xwmi->win_num = 0;
    write_serial_string("XWM Initializing Success.\n");
}

void create_window(XWM_INFO *xwmi, SHEET_INFO *sht, WINDOWLS **windowls, CHAR8 *title, UINT32 width, UINT32 height)
{
    WindowIsCreating = true;

    WINDOWLS *new_window = (WINDOWLS *)(malloc(sizeof(WINDOWLS))); // 开辟内存空间
    xmemset(new_window, 0, sizeof(WINDOWLS));

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

    create_sheet(sht, 72, 72, width + 10, height + 27, MovableSheetType, -1, &(new_window->w_sheet));
    draw_rect(sht, &(new_window->w_sheet), 0, 0, width + 9, height + 26, {0x00, 0xa2, 0xe8, WINDOW_ALPHA_VALUE});

    for (int y = 0; y < height + 27; y++)
    {
        for (int x = width / 3; x < width / 3 + 114; x++)
        {
            draw_point(sht, &(new_window->w_sheet), (x + y) % (width + 10), y, {0x0f, 0xb1, 0xf7, WINDOW_ALPHA_VALUE});
        }
    }

    for (int y = 0; y < height + 27; y++)
    {
        for (int x = width / 3 + 256; x < width / 3 + 48 + 256; x++)
        {
            draw_point(sht, &(new_window->w_sheet), (x + y) % (width + 10), y, {0x0f, 0xb1, 0xf7, WINDOW_ALPHA_VALUE});
        }
    }

    draw_rect(sht, &(new_window->w_sheet), 5, 22, width + 3, height + 21, {0xff, 0xff, 0xff, 0xff});

    for (int y = 0; y < 9; y++)
    {
        for (int x = 0; x < 9; x++)
        {
            if (wbutton_c[y][x] == 'a')
            {
                draw_point(sht, &(new_window->w_sheet), x, y, {0, 0, 0, 0});
            }
            else if (wbutton_c[y][x] == '.')
            {
                // do nothing
            }
            else if (wbutton_c[y][x] == '@')
            {
                draw_point(sht, &(new_window->w_sheet), x, y, {0, 0, 0, WINDOW_ALPHA_VALUE});
            }
        }
    }

    for (int y = 0; y < 17; y++)
    {
        for (int x = 0; x < 32; x++)
        {
            if (wbutton_close[y][x] == '.')
            {
                draw_point(sht, &(new_window->w_sheet), x + width - 22, y, {0xed, 0x1c, 0x24, WINDOW_ALPHA_VALUE});
            }
            else if (wbutton_close[y][x] == 'a')
            {
                draw_point(sht, &(new_window->w_sheet), x + width - 22, y, {0, 0, 0, 0});
            }
            else if (wbutton_close[y][x] == '@')
            {
                draw_point(sht, &(new_window->w_sheet), x + width - 22, y, {0, 0, 0, WINDOW_ALPHA_VALUE});
            }
        }
    }

    for (int y = 0; y < 17; y++)
    {
        for (int x = 0; x < 32; x++)
        {
            if (wbutton_little[y][x] == '@')
            {
                draw_point(sht, &(new_window->w_sheet), x + width - 53, y, {0, 0, 0, WINDOW_ALPHA_VALUE});
            }
        }
    }

    for (int y = 0; y < 4; y++)
    {
        for (int x = 0; x < 4; x++)
        {
            if (wbutton_cdl[y][x] == 'a')
            {
                draw_point(sht, &(new_window->w_sheet), x, y + height + 23, {0, 0, 0, 0});
            }
        }
    }

    for (int y = 0; y < 4; y++)
    {
        for (int x = 0; x < 4; x++)
        {
            if (wbutton_cdr[y][x] == 'a')
            {
                draw_point(sht, &(new_window->w_sheet), x + width + 6, y + height + 23, {0, 0, 0, 0});
            }
        }
    }

    print_box_ttf(sht, &(new_window->w_sheet), title, WHITE, 22, 0, 11);

    refresh_part_sheet(sht, 72, 72, width + 10 + 72, height + 27 + 72);
    WindowIsCreating = false;
}

void change_title(XWM_INFO *xwmi, SHEET_INFO *sht, WINDOWLS *windowls, CHAR8 *title)
{
    draw_rect(sht, &(windowls->w_sheet), 22, 0, getXsize(sht, &(windowls->w_sheet)) - 81, 20, {0x00, 0xa2, 0xe8, WINDOW_ALPHA_VALUE});
    print_box_ttf(sht, &(windowls->w_sheet), title, WHITE, 22, 0, 11);
    refresh_part_sheet(sht, getBX(sht, &(windowls->w_sheet)), getBY(sht, &(windowls->w_sheet)),
                       getBX(sht, &(windowls->w_sheet)) + getXsize(sht, &(windowls->w_sheet)) + 10,
                       getBY(sht, &(windowls->w_sheet)) + 22);
}

void draw_text(XWM_INFO *xwmi, SHEET_INFO *sht, WINDOWLS *windowls, CHAR8 *title, int x, int y, int size, SHEET_BUFFER color)
{
    print_box_ttf(sht, &(windowls->w_sheet), title, color, x, y, 10);
}

void delete_window(XWM_INFO *xwmi, SHEET_INFO *sht, WINDOWLS *windowls) {
    while (true) {
        if (!WindowIsCreating) {
            break;
        }
    }

    WindowIsCreating = true;
    WINDOWLS *front_p = xwmi->start;
    WINDOWLS *front_p2;
    SHEET    *front_p3 = sht->start;
    int bx, by, ex, ey;

    while (true)
    {
        if (front_p3->index == windowls->w_sheet)
        {
            bx = front_p3->bx;
            by = front_p3->by;
            ex = bx + front_p3->width;
            ey = by + front_p3->height;
        }
        else if (front_p3->next == NULL)
        {
            write_serial_string("Window Delete Faild. (Cannot found sheet) SheetID: ");
            write_serial_dec(windowls->w_sheet);
            write_serial_string("\n");
            return;
        }
        front_p3 = front_p3->next;
    }

    while (true)
    {
        if (front_p->number == windowls->number)
        {
            if (!front_p2)
                front_p2->next = front_p->next;
            else
                xwmi->start = front_p->next;
            delete_sheet(sht, &(windowls->w_sheet));
            xmemset(windowls, 0, sizeof(SHEET));
            free((void *)(windowls));
            // xwmi->win_num--;
            refresh_part_sheet(sht_img, bx, by, ex, ey);
            return;
        }
        else if (front_p->next == NULL)
        {
            write_serial_string("Window Delete Faild. (Cannot found window) WindowID: ");
            return;
        }
        front_p2 = front_p;
        front_p = front_p->next;
    }

    WindowIsCreating = false;
}

WINDOWLS* shtid_found_win(XWM_INFO *xwmi, SHEET_INFO *sht, UINT16 *sheet_id) {
    WINDOWLS *front_p = xwmi->start;
    while (true)
    {
        SHEET *sheet = get_sheet(sht, &(front_p->w_sheet));
        if (sheet->index == *sheet_id)
        {
            return front_p;
        }
        else if (front_p->next == NULL)
        {
            return NULL;
        }
        front_p = front_p->next;
    }
}

WINDOWLS* sht_found_win(XWM_INFO *xwmi, SHEET_INFO *sht, SHEET *csheet) {
    WINDOWLS *front_p = xwmi->start;
    while (true)
    {
        SHEET *sheet = get_sheet(sht, &(front_p->w_sheet));
        if (sheet->index == csheet->index)
        {
            return front_p;
        }
        else if (front_p->next == NULL)
        {
            return NULL;
        }
        front_p = front_p->next;
    }
}
