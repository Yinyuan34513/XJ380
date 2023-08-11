#include "include.h"

#define CONS_WIN_X	320
#define CONS_WIN_Y	180

#define COL_BLACK	0x000000

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
	win_x = CONS_WIN_X;
	win_y = CONS_WIN_Y;
	
	//光标颜色
	u32 cur_col = 0x000000;
	
	//窗口
    sheet_setbuf(sht_cons, buf_cons, win_x, win_y, 99);
    
    make_window(buf_cons, win_x, win_y, "Terminal", 99);
    putstr_atbuf(buf_cons, win_x, 0, 20, 0x000000, ">");
    
    int t;
    char key_in[1];
    int key_in_m;
    char keys_in[64];
    
    for (;;)
    {
    	if (fifo_status(&decoded_key) > 0)
    	{
    		//输入
    		key_in[0] = fifo_get(&decoded_key);
    		keys_in[key_in_m] = key_in[0];
    		if (key_in == 13 || key_in == 10)
    		{
    			input_pgm(keys_in, ccx, ccy);
    		}
    		putstr_atbuf(buf_cons, win_x, ccx, ccy, 0x000000, key_in);
    		if (ccx + 8 >= win_x - 7)
    		{
    			if (ccy + 16 >= win_y - 15)
    			{
    				//滚动
    				//以后再写
    			}
    			else
    			{
        			ccy += 16;
    			}
    			ccx = 8;
    		}
    		else
    		{
    			ccx += 8;
    		}
    		key_in_m++;
    	}
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
		}
		sheet_refresh(sht_cons, 0, 0, win_x, win_y);
    }
}

PUBLIC int input_pgm(char *str, int ccx, int ccy)
{
	int black = COL_BLACK;
	//strcmp()（XINGJI版）说明：1 = 一样， 0 = 不一样
	if (strcmp(str, "info") == 1)
	{
		output("XJ380 BetaVersion", black, ccx, ccy);
		output("Copyright(C) XINGJI 2017-2023", black, ccx, ccy);
	}
	return ccx, ccy;
}

PUBLIC int output(char *str, int c, int x, int y)
{
	putstr_atbuf(buf_cons, CONS_WIN_X, x, y, c, str);
	x += (strlen(str) << 3);
	y += 16;
	return x, y;
}
