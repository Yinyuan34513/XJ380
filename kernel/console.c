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
    int ref_right = 1;
	t = get_ticks();
    
    for (;;)
    {
    	if (fifo_status(&decoded_key) > 0 && ((get_ticks() - t) * 100 / HZ) >= 25)
    	{
    		//输入
    		key_in[0] = fifo_get(&decoded_key);
    		keys_in[key_in_m] = key_in[0];
    		if (key_in[0] == '\n' || key_in[0] == '\r')
    		{
    			ccx = 8;
    			ccy += 8;
    			//识别指令
    			if (strcmp(keys_in, "info") == 0)
    			{
    				putstr_atbuf(buf_cons, win_x, ccx, ccy, 0x000000, "XJ380 BetaVersion");
    				ccx += (17 << 3);
    			}
    			else
    			{
    				putstr_atbuf(buf_cons, win_x, ccx, ccy, 0x000000, "Command not found.");
    				ccx += (18 << 3);
    			}
    			ccx = 8;
    			ccy += 8;
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
    		ref_right = 1;
    		key_in_m++;
    	}
		putstr_atbuf(buf_cons, win_x, ccx, ccy, cur_col, " ");
		if (((get_ticks() - t) * 100 / HZ) >= 500)
		{
			t = get_ticks();
			if (cur_col == 0x000000)
			{
				cur_col = 0xffffff;
			}
			else if (cur_col == 0xffffff)
			{
				cur_col = 0x000000;
			}
			ref_right = 1;
		}
		if (ref_right == 1)
		{
			sheet_refresh(sht_cons, 0, 0, win_x, win_y);
			ref_right = 0;
		}
    }
}
