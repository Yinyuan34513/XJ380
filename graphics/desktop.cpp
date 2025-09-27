#include "./image/stbi.h"
#include "./image/stbir.h"
#include "graphics/xingji_logo_16px.hpp"
#include "graphics/xj380_logo_rect_72x64px.hpp"
#include <global_color.h>
#include <graphics/GOP.hpp>
#include <graphics/sheet.h>
#include <proto.hpp>
#include <ttf.h>

// 初始化桌面
void init_desktop(SHEET_INFO *sht, uint16_t *sheet_id, uint16_t *dock_sheet_id)
{
    int           scrx = sht->scrx;
    int           scry = sht->scry;
    // #136dff
    // draw_rect(sht, sheet_id, 0, 0, scrx - 1, scry - 1, WIN_BLUE);
    int           w, h, bpp;
    stbi_uc      *b = stbi_load("/BackGround.jpeg", &w, &h, &bpp, 4);
    uint8_t      *b1 = (uint8_t *)(malloc(scrx * scry * 4));
    stbir_resize_uint8(b, w, h, 0, b1, scrx, scry, 0, 4);
    SHEET_BUFFER *buff = (SHEET_BUFFER *)b1;

    SHEET *front_p = sht->start;
    while (true)
    {
        if (front_p->index == *sheet_id) { break; }
        else if (front_p->next == NULL) { return; }
        front_p = front_p->next;
    }

    SHEET_BUFFER *SheetBuffer = (SHEET_BUFFER *)(front_p->buffer);
    const int     width       = front_p->width;

    // 检查内存对齐情况
    int is_src_aligned  = ((uintptr_t)buff & 0xF) == 0;
    int is_dest_aligned = ((uintptr_t)SheetBuffer & 0xF) == 0;

    for (int i = 0; i < scry; i++)
    {
        SHEET_BUFFER       *dest_row = &SheetBuffer[i * width];
        const SHEET_BUFFER *src_row  = &buff[i * scrx];

        int j = 0;

        // 根据对齐情况选择最优复制方式
        if (is_src_aligned && is_dest_aligned)
        {
            // 双方都对齐的情况 - 使用 movaps
            for (; j <= scrx - 16; j += 16)
            {
                __asm__ volatile("movaps (%1), %%xmm0\n\t"
                                 "movaps 16(%1), %%xmm1\n\t"
                                 "movaps 32(%1), %%xmm2\n\t"
                                 "movaps 48(%1), %%xmm3\n\t"
                                 "movaps %%xmm0, (%0)\n\t"
                                 "movaps %%xmm1, 16(%0)\n\t"
                                 "movaps %%xmm2, 32(%0)\n\t"
                                 "movaps %%xmm3, 48(%0)\n\t"
                                 :
                                 : "r"(dest_row + j), "r"(src_row + j)
                                 : "xmm0", "xmm1", "xmm2", "xmm3", "memory");
            }
        }
        else
        {
            // 至少一方未对齐的情况 - 使用 movups
            for (; j <= scrx - 16; j += 16)
            {
                __asm__ volatile("movups (%1), %%xmm0\n\t"
                                 "movups 16(%1), %%xmm1\n\t"
                                 "movups 32(%1), %%xmm2\n\t"
                                 "movups 48(%1), %%xmm3\n\t"
                                 "movups %%xmm0, (%0)\n\t"
                                 "movups %%xmm1, 16(%0)\n\t"
                                 "movups %%xmm2, 32(%0)\n\t"
                                 "movups %%xmm3, 48(%0)\n\t"
                                 :
                                 : "r"(dest_row + j), "r"(src_row + j)
                                 : "xmm0", "xmm1", "xmm2", "xmm3", "memory");
            }
        }

        // 处理剩余不足 16 个的元素
        for (; j < scrx; j++)
        {
            dest_row[j] = src_row[j];
        }
    }
    init_dock(sht, dock_sheet_id);
    init_shortcut_dock(sht, dock_sheet_id);
}

void init_dock(SHEET_INFO *sht, uint16_t *sheet_id)
{
    int scrx = sht->scrx;
    int scry = sht->scry;
    int tmpx = scrx * 0.75; // 75%
    int tmpy = scry - 48;
    draw_rect(sht, sheet_id, 0, scry - 48, tmpx - 1, scry - 1, DOCK_COL);

    for (int y = 0; y <= 47; y++)
    {
        for (int x = 0; x <= y; x++)
        {
            draw_point(sht, sheet_id, x + tmpx, y + tmpy, {0xe6, 0xe6, 0xe6, 0xaa});
        }
    }

    draw_logo(sht, sheet_id, 24, scry - 24 - 63);
}

void init_shortcut_dock(SHEET_INFO *sht, uint16_t *sheet_id)
{
    int x1 = sht->scrx * 0.1875;
    int y1 = 0;
    int x2 = sht->scrx * 0.8125;
    int y2 = 24;
    draw_rect(sht, sheet_id, x1, y1, x2 - 1, y2 - 1, DOCK_COL);

    draw_studio_logo(sht, sheet_id, x1 + 8, 4);

    print_box_ttf(sht, sheet_id, (char *)"文件", BLACK, x1 + 32, 1, 10);
    print_box_ttf(sht, sheet_id, (char *)"设置", BLACK, x1 + 32 + 25 + 8, 1, 10);
    print_box_ttf(sht, sheet_id, (char *)"终端", BLACK, x1 + 32 + 25 + 8 + 25 + 9, 1, 10);
    print_box_ttf(sht, sheet_id, (char *)"图形", BLACK, x1 + 32 + 25 + 8 + 25 + 9 + 25 + 9, 1, 10);
    print_box_ttf(sht, sheet_id, (char *)"关于", BLACK, x1 + 32 + 25 + 8 + 25 + 9 + 25 + 9 + 25 + 8, 1, 10);
    // PrintString(sht, sheet_id, x1 + 32, 4, "Files", BLACK);
    // PrintString(sht, sheet_id, x1 + 32 + 40 + 8, 4, "Settings", BLACK);
    // PrintString(sht, sheet_id, x1 + 32 + 40 + 8 + 64 + 8, 4, "Terminal", BLACK);
    // PrintString(sht, sheet_id, x1 + 32 + 40 + 8 + 64 + 8 + 64 + 8, 4, "Graphics", BLACK);
    // PrintString(sht, sheet_id, x1 + 32 + 40 + 8 + 64 + 8 + 64 + 8 + 64 + 8, 4, "About", BLACK);
}

void draw_logo(SHEET_INFO *sht, uint16_t *sheet_id, int xi, int yi)
{
    for (int y = 0; y < 63; y++)
    {
        for (int x = 0; x < 73; x++)
        {
            if (xj380_logo_rect_72x64px[y][x] == '.')
            {
                // pass
            }
            else if (xj380_logo_rect_72x64px[y][x] == '#') { draw_point(sht, sheet_id, x + xi, y + yi, XINGJI_BLUE); }
            else if (xj380_logo_rect_72x64px[y][x] == '@') { draw_point(sht, sheet_id, x + xi, y + yi, XINGJI_YELLOW); }
        }
    }
}

void draw_studio_logo(SHEET_INFO *sht, uint16_t *sheet_id, int xi, int yi)
{
    for (int y = 0; y < 16; y++)
    {
        for (int x = 0; x < 16; x++)
        {
            if (xingji_logo_16px[y][x] == '.')
            {
                // pass
            }
            else if (xingji_logo_16px[y][x] == '@') { draw_point(sht, sheet_id, x + xi, y + yi, XINGJI_BLUE); }
            else if (xingji_logo_16px[y][x] == '#') { draw_point(sht, sheet_id, x + xi, y + yi, XINGJI_YELLOW); }
        }
    }
}