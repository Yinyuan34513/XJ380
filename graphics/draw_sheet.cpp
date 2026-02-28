#include <graphics/sheet.h>
#include <proto.hpp>
void draw_point(SHEET_INFO *sht, SHEET *csheet, int x, int y, const SHEET_BUFFER &color)
{
    SHEET_BUFFER *SheetBuffer = (SHEET_BUFFER *)(csheet->buffer);
    uint32_t *pixel = (uint32_t *)&SheetBuffer[csheet->width * y + x];
    *pixel = *(uint32_t*)&color;  // 一次性写入 4 字节
}

void draw_rect(SHEET_INFO *sht, SHEET *csheet, int x1, int y1, int x2, int y2, const SHEET_BUFFER &color)
{
    if (x1 > x2) { int tmp = x1; x1 = x2; x2 = tmp; }
    if (y1 > y2) { int tmp = y1; y1 = y2; y2 = tmp; }
    
    SHEET_BUFFER *SheetBuffer = (SHEET_BUFFER *)(csheet->buffer);
    int width = x2 - x1 + 1;
    
    // 预组合颜色值（如果 SHEET_BUFFER 是 4 字节结构）
    uint32_t color_value = *(uint32_t*)&color;
    
    for (int y = y1; y <= y2; y++)
    {
        SHEET_BUFFER *row_start = &SheetBuffer[csheet->width * y + x1];
        
        // 使用 32 位批量写入
        uint32_t *row_ptr = (uint32_t *)row_start;
        for (int x = 0; x < width; x++)
        {
            row_ptr[x] = color_value;
        }
    }
}

void draw_line(SHEET_INFO *sht, SHEET *csheet, int x0, int y0, int x1, int y1, const SHEET_BUFFER &color)
{
    SHEET_BUFFER *SheetBuffer = (SHEET_BUFFER *)(csheet->buffer);
    uint32_t color_value = *(uint32_t*)&color;  // 预组合颜色值
    
    int i, x, y, len, dx, dy;

    dx = x1 - x0;
    dy = y1 - y0;
    x  = x0 << 10;
    y  = y0 << 10;
    if (dx < 0) { dx = -dx; }
    if (dy < 0) { dy = -dy; }
    if (dx >= dy)
    {
        len = dx + 1;
        if (x0 > x1) { dx = -1024; }
        else { dx = 1024; }
        if (y0 <= y1) { dy = ((y1 - y0 + 1) << 10) / len; }
        else { dy = ((y1 - y0 - 1) << 10) / len; }
    }
    else
    {
        len = dy + 1;
        if (y0 > y1) { dy = -1024; }
        else { dy = 1024; }
        if (x0 <= x1) { dx = ((x1 - x0 + 1) << 10) / len; }
        else { dx = ((x1 - x0 - 1) << 10) / len; }
    }

    uint32_t *pixels = (uint32_t *)SheetBuffer;
    for (i = 0; i < len; i++)
    {
        int pixel_y = y >> 10;
        int pixel_x = x >> 10;
        pixels[csheet->width * pixel_y + pixel_x] = color_value;
        x += dx;
        y += dy;
    }
}
