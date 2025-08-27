#include <graphics/sheet.h>
#include <proto.hpp>
void draw_point(SHEET_INFO *sht, uint16_t *sheet_id, int x, int y, const SHEET_BUFFER &color)
{
    SHEET *front_p = sht->start;
    while (true)
    {
        if (front_p->index == *sheet_id) { break; }
        else if (front_p->next == NULL) { return; }
        front_p = front_p->next;
    }
    SHEET_BUFFER *SheetBuffer  = (SHEET_BUFFER *)(front_p->buffer);
    SheetBuffer               += (front_p->width * y + x);
    SheetBuffer->r             = color.r;
    SheetBuffer->g             = color.g;
    SheetBuffer->b             = color.b;
    SheetBuffer->a             = color.a;
}

void draw_rect(SHEET_INFO *sht, uint16_t *sheet_id, int x1, int y1, int x2, int y2, const SHEET_BUFFER &color)
{
    SHEET *front_p = sht->start;
    while (true)
    {
        if (front_p->index == *sheet_id) { break; }
        else if (front_p->next == NULL) { return; }
        front_p = front_p->next;
    }
    SHEET_BUFFER *SheetBuffer = (SHEET_BUFFER *)(front_p->buffer);
    for (int y = y1; y <= y2; y++)
    {
        for (int x = x1; x <= x2; x++)
        {
            SheetBuffer[front_p->width * y + x].r = color.r;
            SheetBuffer[front_p->width * y + x].g = color.g;
            SheetBuffer[front_p->width * y + x].b = color.b;
            SheetBuffer[front_p->width * y + x].a = color.a;
        }
    }
}

void draw_line(SHEET_INFO *sht, uint16_t *sheet_id, int x0, int y0, int x1, int y1, const SHEET_BUFFER &color)
{
    SHEET *front_p = sht->start;
    while (true)
    {
        if (front_p->index == *sheet_id) { break; }
        else if (front_p->next == NULL) { return; }
        front_p = front_p->next;
    }
    SHEET_BUFFER *SheetBuffer = (SHEET_BUFFER *)(front_p->buffer);

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

    for (i = 0; i < len; i++)
    {
        SheetBuffer[front_p->width * (y >> 10) + (x >> 10)].r  = color.r;
        SheetBuffer[front_p->width * (y >> 10) + (x >> 10)].g  = color.g;
        SheetBuffer[front_p->width * (y >> 10) + (x >> 10)].b  = color.b;
        SheetBuffer[front_p->width * (y >> 10) + (x >> 10)].a  = color.a;
        x                                                     += dx;
        y                                                     += dy;
    }
}
