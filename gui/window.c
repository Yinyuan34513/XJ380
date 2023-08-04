#include "include.h"

//创建窗口

PUBLIC void make_window(u32 *buf, int xsize, int ysize, char *title, int bc)
{
    //绘制窗口
	boxfill8(buf, xsize, 0x00a2e8, 0, 0, xsize, ysize);		//标题栏，颜色#00A2E8
    boxfill8(buf, xsize, 0xffffff, 0, 20, xsize, ysize);	//窗口主体，白色

    //定义关闭按钮样式

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
    //定义缩小按钮样式
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
    putstr_atbuf(buf, xsize, 8, 4, 0xffffff, title);	//绘制标题（白色）
    //将样式转换为颜色列表
    for (y = 0; y < 18; y++)
    {
        for (x = 0; x < 32; x++)
        {
            c = closebtn[y][x];
            if (c == '@') 
            {
                c = 0x000000;	//黑
            } 
            else if (c == 'Q') 
            {
                c = 0xc42b1c;	//Windows关闭按钮的红（#C42B1C）
            }
            buf[(y + 2) * xsize + (xsize - 37 + x)] = c;
            c = minimize[y][x];
            if (c == '@')
            {
                c = 0x000000;	//黑
            }
            else if (c == 'Q')
            {
                c = 0x00a2e8;	//星际集团主题色——浅蓝（#00A2E8）
            }
            buf[(y + 2) * xsize + (xsize - 68 + x)] = c;
        }
    }
    //硬核圆角
    buf[0 * xsize + 0] = bc;
    buf[0 * xsize + 1] = bc;
    buf[0 * xsize + 2] = bc;
    buf[0 * xsize + 3] = bc;
    buf[1 * xsize + 0] = bc;
    buf[1 * xsize + 1] = bc;
    buf[2 * xsize + 0] = bc;
    buf[3 * xsize + 0] = bc;

    buf[0 * xsize + xsize - 1 - 0] = bc;
    buf[0 * xsize + xsize - 1 - 1] = bc;
    buf[0 * xsize + xsize - 1 - 2] = bc;
    buf[0 * xsize + xsize - 1 - 3] = bc;
    buf[1 * xsize + xsize - 1 - 0] = bc;
    buf[1 * xsize + xsize - 1 - 1] = bc;
    buf[2 * xsize + xsize - 1 - 0] = bc;
    buf[3 * xsize + xsize - 1 - 0] = bc;

    buf[(ysize - 0) * xsize + 0] = bc;
    buf[(ysize - 0) * xsize + 1] = bc;
    buf[(ysize - 0) * xsize + 2] = bc;
    buf[(ysize - 0) * xsize + 3] = bc;
    buf[(ysize - 1) * xsize + 0] = bc;
    buf[(ysize - 1) * xsize + 1] = bc;
    buf[(ysize - 2) * xsize + 0] = bc;
    buf[(ysize - 3) * xsize + 0] = bc;

    buf[(ysize - 0) * xsize + xsize - 1 - 0] = bc;
    buf[(ysize - 0) * xsize + xsize - 1 - 1] = bc;
    buf[(ysize - 0) * xsize + xsize - 1 - 2] = bc;
    buf[(ysize - 0) * xsize + xsize - 1 - 3] = bc;
    buf[(ysize - 1) * xsize + xsize - 1 - 0] = bc;
    buf[(ysize - 1) * xsize + xsize - 1 - 1] = bc;
    buf[(ysize - 2) * xsize + xsize - 1 - 0] = bc;
    buf[(ysize - 3) * xsize + xsize - 1 - 0] = bc;

    //返回
    return;
}

PUBLIC void make_console_window(u32 *buf, int xsize, int ysize, char *title, int bc)
{
    //绘制命令行
	boxfill8(buf, xsize, 0x00a2e8, 0, 0, xsize, ysize);		//标题栏，颜色#00A2E8
    boxfill8(buf, xsize, 0x000000, 0, 20, xsize, ysize);	//命令行，全黑

    //定义关闭按钮样式

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
    //定义缩小按钮样式
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
    putstr_atbuf(buf, xsize, 8, 4, 0xffffff, title);	//绘制标题（白色）
    //将样式转换为颜色列表
    for (y = 0; y < 18; y++)
    {
        for (x = 0; x < 32; x++)
        {
            c = closebtn[y][x];
            if (c == '@') 
            {
                c = 0x000000;	//黑
            } 
            else if (c == 'Q') 
            {
                c = 0xc42b1c;	//Windows关闭按钮的红（#C42B1C）
            }
            buf[(y + 2) * xsize + (xsize - 37 + x)] = c;
            c = minimize[y][x];
            if (c == '@')
            {
                c = 0x000000;	//黑
            }
            else if (c == 'Q')
            {
                c = 0x00a2e8;	//星际集团主题色——浅蓝（#00A2E8）
            }
            buf[(y + 2) * xsize + (xsize - 68 + x)] = c;
        }
    }
    //硬核圆角
    buf[0 * xsize + 0] = bc;
    buf[0 * xsize + 1] = bc;
    buf[0 * xsize + 2] = bc;
    buf[0 * xsize + 3] = bc;
    buf[1 * xsize + 0] = bc;
    buf[1 * xsize + 1] = bc;
    buf[2 * xsize + 0] = bc;
    buf[3 * xsize + 0] = bc;

    buf[0 * xsize + xsize - 1 - 0] = bc;
    buf[0 * xsize + xsize - 1 - 1] = bc;
    buf[0 * xsize + xsize - 1 - 2] = bc;
    buf[0 * xsize + xsize - 1 - 3] = bc;
    buf[1 * xsize + xsize - 1 - 0] = bc;
    buf[1 * xsize + xsize - 1 - 1] = bc;
    buf[2 * xsize + xsize - 1 - 0] = bc;
    buf[3 * xsize + xsize - 1 - 0] = bc;

    buf[(ysize - 0) * xsize + 0] = bc;
    buf[(ysize - 0) * xsize + 1] = bc;
    buf[(ysize - 0) * xsize + 2] = bc;
    buf[(ysize - 0) * xsize + 3] = bc;
    buf[(ysize - 1) * xsize + 0] = bc;
    buf[(ysize - 1) * xsize + 1] = bc;
    buf[(ysize - 2) * xsize + 0] = bc;
    buf[(ysize - 3) * xsize + 0] = bc;

    buf[(ysize - 0) * xsize + xsize - 1 - 0] = bc;
    buf[(ysize - 0) * xsize + xsize - 1 - 1] = bc;
    buf[(ysize - 0) * xsize + xsize - 1 - 2] = bc;
    buf[(ysize - 0) * xsize + xsize - 1 - 3] = bc;
    buf[(ysize - 1) * xsize + xsize - 1 - 0] = bc;
    buf[(ysize - 1) * xsize + xsize - 1 - 1] = bc;
    buf[(ysize - 2) * xsize + xsize - 1 - 0] = bc;
    buf[(ysize - 3) * xsize + xsize - 1 - 0] = bc;

    //返回
    return;
}