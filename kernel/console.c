#include "include.h"

PUBLIC u32 buf_cons[320 * 180];

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
	win_x = 320;
	win_y = 180;
	
	//光标颜色
	u32 cur_col = 0x000000;
	
	//窗口
    sheet_setbuf(sht_cons, buf_cons, win_x, win_y, 99);
    
    make_window(buf_cons, win_x, win_y, "Terminal", 99);
    putstr_atbuf(buf_cons, win_x, 0, 20, 0x000000, ">");
    
    int t;
	int refresh_right = 1;//是否刷新，0 = 不刷新，其他 = 刷新
    
    for (;;)
    {
		t = get_ticks();
		putstr_atbuf(buf_cons, win_x, ccx, ccy, cur_col, " ");
		if (((get_ticks() - t) * 100 / HZ) >= 500)
		{
			if (cur_col == 0x000000)
			{
				cur_col = 0xffffff;
			}
			else if (cur_col == 0xffffff)
			{
				cur_col = 0x000000;
			}
			refresh_right = 1;
		}
		if (ccx == (win_x - 20))
		{
			ccx = 8;
			ccy += 16;
			refresh_right = 1;
		}
		if (refresh_right ^ 0)
		{
			sheet_refresh(sht_cons, 0, 0, win_x, win_y);
			refresh_right = 0;
		}
    }
    
    return;
}