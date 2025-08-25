/*
 *
 *      XJ380 Window Manager
 *      Copyright(C) XINGJI Interactive Software 2017-2025 All rights reserved.
 *
 */
#include<proto.hpp>
#include <graphics/window/window.h>
#include <ttf.h>
#include <global_color.h>
#include "./wbuttons.h"

#define WINDOW_ALPHA_VALUE  0xaa
#define WINDOW_COLOR_R      0x00
#define WINDOW_COLOR_G      0xa2
#define WINDOW_COLOR_B      0xe8
#define WINDOW_COLOR        {WINDOW_COLOR_R, WINDOW_COLOR_G, WINDOW_COLOR_B, WINDOW_ALPHA_VALUE}
#define WINDOW_LIGHT_OFFEST 0x0f
// #define WINDOW_LIGHT_COLOR  {WINDOW_COLOR_R, WINDOW_COLOR_G, WINDOW_COLOR_B, (uint8_t)(WINDOW_ALPHA_VALUE + WINDOW_LIGHT_OFFEST)}
#define WINDOW_LIGHT_COLOR  {(uint8_t)(WINDOW_COLOR_R + WINDOW_LIGHT_OFFEST),      \
                                (uint8_t)(WINDOW_COLOR_G + WINDOW_LIGHT_OFFEST),   \
                                (uint8_t)(WINDOW_COLOR_B + WINDOW_LIGHT_OFFEST),   \
                                WINDOW_ALPHA_VALUE}

bool WindowIsCreating = false;

void init_xwm(XWM_INFO *xwmi)
{
    write_serial_string("Initializing XWM...\n");
    xwmi->start = NULL;
    xwmi->win_num = 0;
    write_serial_string("XWM Initializing Success.\n");
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

    create_sheet(sht, 72, 72, width + 10, height + 27, MovableSheetType, -1, &(new_window->w_sheet));
    draw_rect(sht, &(new_window->w_sheet), 0, 0, width + 9, height + 26, WINDOW_COLOR);

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
    draw_rect(sht, &(windowls->w_sheet), 22, 0, getXsize(sht, &(windowls->w_sheet)) - 81, 20, WINDOW_COLOR);
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
            memset(windowls, 0, sizeof(SHEET));
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

WINDOWLS* sht_found_win(XWM_INFO *xwmi, SHEET_INFO *sht, SHEET *csheet) {
    if (csheet == NULL) {
        return NULL;
    }

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
