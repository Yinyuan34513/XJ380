#include "include.h"

/*
PUBLIC void make_window(u32 *buf, int xsize, int ysize, char *title)
{
    PRIVATE char closebtn[14][16] = {
        "OOOOOOOOOOOOOOO@",
        "OQQQQQQQQQQQQQ$@",
        "OQQQQQQQQQQQQQ$@",
        "OQQQ@@QQQQ@@QQ$@",
        "OQQQQ@@QQ@@QQQ$@",
        "OQQQQQ@@@@QQQQ$@",
        "OQQQQQQ@@QQQQQ$@",
        "OQQQQQ@@@@QQQQ$@",
        "OQQQQ@@QQ@@QQQ$@",
        "OQQQ@@QQQQ@@QQ$@",
        "OQQQQQQQQQQQQQ$@",
        "OQQQQQQQQQQQQQ$@",
        "O$$$$$$$$$$$$$$@",
        "@@@@@@@@@@@@@@@@"
    };

    int x, y, c;
    
    boxfill8(buf, xsize, 0xc6c6c6, 0, 0, xsize - 1, 0);
    boxfill8(buf, xsize, 0xffffff, 1, 1, xsize - 2, 1);
    boxfill8(buf, xsize, 0xc6c6c6, 0, 0, 0, ysize - 1);
    boxfill8(buf, xsize, 0xffffff, 1, 1, 1, ysize - 2);
    boxfill8(buf, xsize, 0x848484, xsize - 2, 1, xsize - 2, ysize - 2);
    boxfill8(buf, xsize, 0x000000, xsize - 1, 0, xsize - 1, ysize - 1);
    boxfill8(buf, xsize, 0x848484, 1, ysize - 2, xsize - 2, ysize - 2);
    boxfill8(buf, xsize, 0x000000, 0, ysize - 1, xsize - 1, ysize - 1);
    boxfill8(buf, xsize, 0xc6c6c6, 2, 2, xsize - 3, ysize - 3);
    boxfill8(buf, xsize, 0x000084, 3, 3, xsize - 4, 20);
    putstr_atbuf(buf, xsize, 24, 4, 0xffffff, title);

    for (y = 0; y < 14; y++) {
        for (x = 0; x < 16; x++) {
            c = closebtn[y][x];
            if (c == '@') c = 0;
            else if (c == '$') c = 0x848484;
            else if (c == 'Q') c = 0xc6c6c6;
            else c = 0xffffff;
            buf[(5 + y) * xsize + (xsize - 21 + x)] = c;
        }
    }
}
*/

PUBLIC void make_window(u32 *buf, int xsize, int ysize, char *title, int bc)
{
    //绘制窗口
	boxfill8(buf, xsize, 0xffffff, 0, 0, xsize, ysize);
    boxfill8(buf, xsize, 0xc6c6c6, 8, 28, xsize - 8, ysize - 8);

    static char closebtn[18][32] = {
        "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
        "@QQQQQQQQQQQQQQQQQQQQQQQQQQQQQQ@",
        "@QQQQQQQQQQQQQQQQQQQQQQQQQQQQQQ@",
        "@QQQQQQQQQQQQQQQQQQQQQQQQQQQQQQ@",
        "@QQQQQQQQQQQQQQQQQQQQQQQQQQQQQQ@",
        "@QQQQQQQQQQQ@@QQQQ@@QQQQQQQQQQQ@",
        "@QQQQQQQQQQQQ@@QQ@@QQQQQQQQQQQQ@",
        "@QQQQQQQQQQQQQ@@@@QQQQQQQQQQQQQ@",
        "@QQQQQQQQQQQQQQ@@QQQQQQQQQQQQQQ@",
        "@QQQQQQQQQQQQQQ@@QQQQQQQQQQQQQQ@",
        "@QQQQQQQQQQQQQ@@@@QQQQQQQQQQQQQ@",
        "@QQQQQQQQQQQQ@@QQ@@QQQQQQQQQQQQ@",
        "@QQQQQQQQQQQ@@QQQQ@@QQQQQQQQQQQ@",
        "@QQQQQQQQQQQQQQQQQQQQQQQQQQQQQQ@",
        "@QQQQQQQQQQQQQQQQQQQQQQQQQQQQQQ@",
        "@QQQQQQQQQQQQQQQQQQQQQQQQQQQQQQ@",
        "@QQQQQQQQQQQQQQQQQQQQQQQQQQQQQQ@",
        "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
    };
    static char minimize[18][32] = {
        "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
        "@QQQQQQQQQQQQQQQQQQQQQQQQQQQQQQ@",
        "@QQQQQQQQQQQQQQQQQQQQQQQQQQQQQQ@",
        "@QQQQQQQQQQQQQQQQQQQQQQQQQQQQQQ@",
        "@QQQQQQQQQQQQQQQQQQQQQQQQQQQQQQ@",
        "@QQQQQQQQQQQQQQQ@@@QQQQQQQQQQQQ@",
        "@QQQQQQQQQQQQQQ@@@QQQQQQQQQQQQQ@",
        "@QQQQQQQQQQQQQ@@@QQQQQQQQQQQQQQ@",
        "@QQQQQQQQQQ@Q@@@QQQQQQQQQQQQQQQ@",
        "@QQQQQQQQQQ@@@@QQQQQQQQQQQQQQQQ@",
        "@QQQQQQQQQQ@@@QQQQQQQQQQQQQQQQQ@",
        "@QQQQQQQQQQ@@@@QQQQQQQQQQQQQQQQ@",
        "@QQQQQQQQQQQQQQQQQQQQQQQQQQQQQQ@",
        "@QQQQQQQQ@@@@@@@@@@@@@@QQQQQQQQ@",
        "@QQQQQQQQ@@@@@@@@@@@@@@QQQQQQQQ@",
        "@QQQQQQQQQQQQQQQQQQQQQQQQQQQQQQ@",
        "@QQQQQQQQQQQQQQQQQQQQQQQQQQQQQQ@",
        "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@"
    };
    //标题栏
    int x, y, c;
    //显示颜色 = 源像素颜色 X alpha / 255 + 背景颜色 X (255 - alpha) / 255
    int alpha;
    alpha = 0;
    int win_col;
    win_col = 0x91c4e2;
    int back_col;
    back_col = bc;
    int obj_col;
    obj_col = (win_col * alpha / 255) + (back_col * (255 - alpha) / 255);
	boxfill8(buf, xsize, obj_col, 0, 0, xsize, 28);
    putstr_atbuf(buf, xsize, (xsize - 64 - strlen(title) * 8) / 2, 4, 0x000000, title);
    for (y = 0; y < 18; y++)
    {
        for (x = 0; x < 32; x++)
        {
            c = closebtn[y][x];
            if (c == '@') 
            {
                c = 0x000000;
            } 
            else if (c == 'Q') 
            {
                c = 0xc42b1c;
            }
            buf[y * xsize + (xsize - 37 + x)] = c;
            c = minimize[y][x];
            if (c == '@')
            {
                c = 0x000000;
            }
            else if (c == 'Q')
            {
                c = 0x91c4e2;
            }
            buf[y * xsize + (xsize - 68 + x)] = c;
        }
    }
    return;
}