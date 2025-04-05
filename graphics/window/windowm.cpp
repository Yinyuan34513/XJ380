/*
 *
 *      XJ380 Window Manager
 *      Copyright(C) XINGJI Interactive Software 2017-2025 All rights reserved.
 * 
 */
#include "../../include/include.h"
#include "./wbuttons.h"

#define WINDOW_ALPHA_VALUE 0x5d

void init_xwm(XWM_INFO *xwmi) {
    write_serial_string("Initializing XWM...\n");
    xwmi->start = NULL;
    write_serial_string("XWM Initializing Success.\n");
}

void create_window(XWM_INFO *xwmi, SHEET_INFO *sht, UINT16 window_id, CHAR8 title, UINT32 width, UINT32 height) {
    WINDOWLS *new_sheet = (WINDOWLS *)(malloc(sizeof(WINDOWLS))); // 开辟内存空间
    xmemset(new_sheet, 0, sizeof(WINDOWLS));

    WINDOWLS *front_p = xwmi->start;
    if (front_p == NULL) {
        // 哦亲爱的孩子我不说了你知道这是干什么的
        xwmi->start = new_sheet;
        create_sheet(sht, 72, 72, width, height, MovableSheetType, sht->sheet_num + 1, &(new_sheet->w_sheet));
        draw_rect(sht, &(new_sheet->w_sheet), 0, 0, width - 1, height - 1, {0x00, 0xa2, 0xe8, WINDOW_ALPHA_VALUE});

        for (int y = 0; y < 9; y++) {
            for (int x = 0; x < 9; x++) {
                if (wbutton_c[y][x] == 'a') {
                    draw_point(sht, &(new_sheet->w_sheet), x, y, {0, 0, 0, 0});
                } else if (wbutton_c[y][x] == '.') {
                    // do nothing
                } else if (wbutton_c[y][x] == '@') {
                    draw_point(sht, &(new_sheet->w_sheet), x, y, {0, 0, 0, WINDOW_ALPHA_VALUE});
                }
            }
        }

        for (int y = 0; y < 9; y++) {
            for (int x = 0; x < 9; x++) {
                if (wbutton_close[y][x] == '.') {
                    draw_point(sht, &(new_sheet->w_sheet), x + width - 16, y, {0xed, 0x1c, 0x24, WINDOW_ALPHA_VALUE});
                } else if (wbutton_close[y][x] == 'a') {
                    draw_point(sht, &(new_sheet->w_sheet), x + width - 16, y, {0, 0, 0, 0});
                } else if (wbutton_close[y][x] == '@') {
                    draw_point(sht, &(new_sheet->w_sheet), x + width - 16, y, {0, 0, 0, WINDOW_ALPHA_VALUE});
                }
            }
        }

        for (int y = 0; y < 9; y++) {
            for (int x = 0; x < 9; x++) {
                if (wbutton_close[y][x] == '.') {
                    // do nothing
                } else if (wbutton_close[y][x] == 'a') {
                    draw_point(sht, &(new_sheet->w_sheet), x + width - 31, y, {0, 0, 0, 0});
                } else if (wbutton_close[y][x] == '@') {
                    draw_point(sht, &(new_sheet->w_sheet), x + width - 31, y, {0, 0, 0, WINDOW_ALPHA_VALUE});
                }
            }
        }
    }
}
