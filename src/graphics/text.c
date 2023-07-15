#include "include.h"

PRIVATE int x = 0, y = 0;

PRIVATE void putfont8(u32 *buf, int xsize, int x, int y, int c, char *font)
{
    int i;
    int *p;
    char d;
    for (i = 0; i < 16; i++) {
        p = buf + (y + i) * xsize + x;
        d = font[i];
        if ((d & 0x80) != 0) { p[0] = c; }
        if ((d & 0x40) != 0) { p[1] = c; }
        if ((d & 0x20) != 0) { p[2] = c; }
        if ((d & 0x10) != 0) { p[3] = c; }
        if ((d & 0x08) != 0) { p[4] = c; }
        if ((d & 0x04) != 0) { p[5] = c; }
        if ((d & 0x02) != 0) { p[6] = c; }
        if ((d & 0x01) != 0) { p[7] = c; }
    }
}

PRIVATE void scroll()
{
    if (x >= binfo->scrnx) {
        y += 16;
        x = 0;
    }
    if (y == binfo->scrny) {
        y = binfo->scrny - 16;
        for (int i = 0; i < binfo->scrny; i++) {
            for (int j = 0; j < binfo->scrnx; j++) {
                buf_back[j + i * binfo->scrnx] = buf_back[j + (i + 16) * binfo->scrnx];
            }
        }
    }
    if (y == binfo->scrny - binfo->scrny % 16) {
        y = binfo->scrny - 16;
        for (int i = 0; i < binfo->scrny; i++) {
            for (int j = 0; j < binfo->scrnx; j++) {
                buf_back[j + i * binfo->scrnx] = buf_back[j + (i + 16 - binfo->scrny % 16) * binfo->scrnx];
            }
        }
    }
}

PUBLIC void put_char(char c)
{
    if (c == '\n') {
        y += 16;
        x = 0;
    } else if (c == '\b') {
        x -= 8;
        if (x == -8) {
            x = binfo->scrnx - 8;
            if (y != 0) y -= 16;
            if (y == -16) x = 0, y = 0;
        }
        draw_rect(x, y, x + 8, y + 16, 0x44cef6);
    } else {
        putfont8(buf_back, binfo->scrnx, (x += 8) - 8, y, 0, font + c * 16);
    }
    scroll();
}

PUBLIC void put_str(char *s)
{
    //disable_int();
    for (; *s; s++) put_char(*s);
    //enable_int();
}

PUBLIC void putstr_atbuf(u32 *buf, int xsize, int x, int y, int c, char *s)
{
    for (; *s; s++) {
        putfont8(buf, xsize, x, y, c, font + *s * 16);
        x += 8;
    }
    return;
}

PUBLIC void setX(int xval)
{
    x = xval;
}

PUBLIC int getX()
{
    return x;
}

PUBLIC void setY(int yval)
{
    y = yval;
}

PUBLIC int getY()
{
    return y;
}
