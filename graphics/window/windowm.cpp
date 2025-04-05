/*
 *
 *      XJ380 Window Manager
 *      Copyright(C) XINGJI Interactive Software 2017-2025 All rights reserved.
 * 
 */
#include "../../include/include.h"
#include "./wbuttons.h"

#define WINDOW_ALPHA_VALUE 0xaa

void init_xwm(XWM_INFO *xwmi) {
    write_serial_string("Initializing XWM...\n");
    xwmi->start = NULL;
    write_serial_string("XWM Initializing Success.\n");
}

void create_window(XWM_INFO *xwmi, SHEET_INFO *sht, UINT16 *window_id, CHAR8 *title, UINT32 width, UINT32 height) {
    WINDOWLS *new_window = (WINDOWLS *)(malloc(sizeof(WINDOWLS))); // 开辟内存空间
    xmemset(new_window, 0, sizeof(WINDOWLS));

    WINDOWLS *front_p = xwmi->start;
    if (front_p == NULL) {
        // 哦亲爱的孩子我不说了你知道这是干什么的
        xwmi->start = new_window;
    } else {
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

    create_sheet(sht, 72, 72, width, height, MovableSheetType, -1, &(new_window->w_sheet));
    draw_rect(sht, &(new_window->w_sheet), 0,  0, width - 1, 20, {0x00, 0xa2, 0xe8, WINDOW_ALPHA_VALUE});
    draw_rect(sht, &(new_window->w_sheet), 0, 20, width - 1, height - 1, {0xff, 0xff, 0xff, 0xff});

    for (int y = 0; y < 9; y++) {
        for (int x = 0; x < 9; x++) {
            if (wbutton_c[y][x] == 'a') {
                draw_point(sht, &(new_window->w_sheet), x, y, {0, 0, 0, 0});
            } else if (wbutton_c[y][x] == '.') {
                // do nothing
            } else if (wbutton_c[y][x] == '@') {
                draw_point(sht, &(new_window->w_sheet), x, y, {0, 0, 0, WINDOW_ALPHA_VALUE});
            }
        }
    }

    for (int y = 0; y < 17; y++) {
        for (int x = 0; x < 32; x++) {
            if (wbutton_close[y][x] == '.') {
                draw_point(sht, &(new_window->w_sheet), x + width - 32, y, {0xed, 0x1c, 0x24, WINDOW_ALPHA_VALUE});
            } else if (wbutton_close[y][x] == 'a') {
                draw_point(sht, &(new_window->w_sheet), x + width - 32, y, {0, 0, 0, 0});
            } else if (wbutton_close[y][x] == '@') {
                draw_point(sht, &(new_window->w_sheet), x + width - 32, y, {0, 0, 0, WINDOW_ALPHA_VALUE});
            }
        }
    }

    for (int y = 0; y < 17; y++) {
        for (int x = 0; x < 32; x++) {
            if (wbutton_little[y][x] == '@') {
                draw_point(sht, &(new_window->w_sheet), x + width - 63, y, {0, 0, 0, WINDOW_ALPHA_VALUE});
            }
        }
    }

    refresh_part_sheet(sht, 72, 72, width, height);
}
