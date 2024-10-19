#include "../include/include.h"
#include "xj380_logo_rect_72x64px.hpp"
#include "xingji_logo_16px.hpp"

//初始化桌面
void init_desktop(SHEET_INFO *sht, UINT16* sheet_id)
{
    int scrx = sht->scrx;
    int scry = sht->scry;
    //#136dff
    draw_rect(sht, sheet_id, 0, 0, scrx, scry, WIN_BLUE);
    init_dock(sht, sheet_id);
    init_shortcut_dock(sht, sheet_id);
}

void init_dock(SHEET_INFO *sht, UINT16* sheet_id)
{
    int scrx = sht->scrx;
    int scry = sht->scry;
    int tmpx = scrx  * 0.75;            
    int tmpy = scry - 48;                // 75%
    draw_rect(sht, sheet_id, 0, scry - 48, tmpx, scry, GRAY);

    for (int y = 0; y <= 47; y++) {
        for (int x = 0; x <= y; x++) {
            draw_point(sht, sheet_id, x + tmpx, y + tmpy, GRAY);
        }
    }

    draw_logo(sht, sheet_id, 24, scry - 24 - 63);
}

void init_shortcut_dock(SHEET_INFO *sht, UINT16* sheet_id)
{
    int x1 = sht->scrx * 0.1875;
    int y1 = 0;
    int x2 = sht->scrx * 0.8125;
    int y2 = 24;
    draw_rect(sht, sheet_id, x1, y1, x2, y2, GRAY);

    draw_studio_logo(sht, sheet_id, x1 + 4, 4);

    PrintString(sht, sheet_id, x1 + 32, 4, "Files", BLACK); 
    PrintString(sht, sheet_id, x1 + 32 + 40 + 8, 4, "Settings", BLACK); 
    PrintString(sht, sheet_id, x1 + 32 + 40 + 8 + 64 + 8, 4, "Terminal", BLACK); 
    PrintString(sht, sheet_id, x1 + 32 + 40 + 8 + 64 + 8 + 64 + 8, 4, "Graphics", BLACK); 
    PrintString(sht, sheet_id, x1 + 32 + 40 + 8 + 64 + 8 + 64 + 8 + 64 + 8, 4, "About", BLACK); 
}

void draw_logo(SHEET_INFO *sht, UINT16* sheet_id, int xi, int yi)
{
    for (int y = 0; y < 63; y++) {
        for (int x = 0; x < 73; x++) {
            if (xj380_logo_rect_72x64px[y][x] == '.')
            {
                // pass
            }
            else if (xj380_logo_rect_72x64px[y][x] == '#')
            {
                draw_point(sht, sheet_id, x + xi, y + yi, XINGJI_BLUE);
            }
            else if (xj380_logo_rect_72x64px[y][x] == '@')
            {
                draw_point(sht, sheet_id, x + xi, y + yi, XINGJI_YELLOW);
            }
        }
    }
}

void draw_studio_logo(SHEET_INFO *sht, UINT16* sheet_id, int xi, int yi)
{
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            if (xingji_logo_16px[y][x] == '.')
            {
                // pass
            }
            else if (xingji_logo_16px[y][x] == '@')
            {
                draw_point(sht, sheet_id, x + xi, y + yi, XINGJI_BLUE);
            }
            else if (xingji_logo_16px[y][x] == '#')
            {
                draw_point(sht, sheet_id, x + xi, y + yi, XINGJI_YELLOW);
            }
        }
    }
}