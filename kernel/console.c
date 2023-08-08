#include "include.h"

PUBLIC u32 buf_cons[480 * 290];

PUBLIC void start_console(SHEET *sht_cons)
{
	//光标坐标
	int ccx;
	int ccy;
	//初生化
	ccx = 8;
	ccy = 20;
	
	//窗口大小
	int win_x;
	int win_y;
	//初生化
	win_x = 480;
	win_y = 290;
	
	//光标颜色
	u32 cur_col = 0x000000;
	
	//窗口
    sheet_setbuf(sht_cons, buf_cons, win_x, win_y, 99);
    
    make_window(buf_cons, win_x, win_y, "Console", 99);
    putstr_atbuf(buf_cons, win_x, 0, 20, 0x000000, ">");
    int t;
    t = get_ticks();
    putstr_atbuf(buf_cons, 480, ccx, ccy, cur_col, " ");
    if (((get_ticks() - t) * 100 / HZ) < 500)
    {
    	if (cur_col == 0x000000)
        {
        	cur_col = 0xffffff;
        }
        else if (cur_col == 0xffffff)
        {
        	cur_col = 0x000000;
        }
    }
    if (ccx == (win_x - 20))
    {
    	ccx = 8;
    	ccy += 16;
    }
    
    return;
}