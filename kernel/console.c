#include "include.h"

#define CONS_WIN_X	320
#define CONS_WIN_Y	180

#define COL_BLACK	0x000000

PUBLIC u32 buf_cons[320 * 180];

//光标坐标
PRIVATE int ccx = 8;
PRIVATE int ccy = 20;

PRIVATE int cls_right = 0;

PUBLIC void start_console(SHEET *sht_cons)
{
	//初生化
	//ccx = 8;
	//ccy = 20;
	
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
    int key_in_m = 0;
    char keys_in[256];
    int ref_right = 1;
    char key;
    
	t = get_ticks();
	
    for (;;)
    {
    	if (fifo_status(&decoded_key) > 0 && ((get_ticks() - t) * 100 / HZ) >= 25)
    	{
			//输入
			key = fifo_get(&decoded_key);
			key_in[0] = key;
    		if (key_in[0] == '\n' || key_in[0] == '\r')
    		{
    			check_command(keys_in);
    			
    			//清空keys_in
    			memset(keys_in, 0, sizeof keys_in);
    			strcpy(keys_in, "");
    			for (int i = 0; i < strlen(keys_in); i++) { keys_in[i] = '\0'; }
    			key_in_m = 0;
    			
    			if (ccy + 32 >= win_y - 31)
    			{
    				console_scroll();
    				ccy += 16;
    			}
    			else if (cls_right ^ 1)
    			{
    				ccy += 32;
    			}
				ccx = 0;
    		    putstr_atbuf(buf_cons, win_x, 0, ccy, 0x000000, ">");
    		}
    		else if (key_in[0] == BACKSPACE)	//BUG里写了个退格
    		{
    			if (ccx > 8)
    			{
    				ccx -= 8;
    			}
    			else if (ccx <= 8)
    			{
    				ccx = win_x - 168;
    				ccy -= 8;
    			}
    			//keys_in删除一个字符
    			key_in_m--;
    			keys_in[key_in_m] = '\0';
				putstr_atbuf(buf_cons, win_x, ccx, ccy, 0xffffff, " ");
    		}
    		else
    		{
				keys_in[key_in_m] = key_in[0];
				key_in_m++;
				key_in[1] = '\0';
				putstr_atbuf(buf_cons, win_x, ccx, ccy, 0x000000, key_in);
    		}
    		if (ccx + 8 >= win_x - 7)
    		{
    			if (ccy + 16 >= win_y - 15)
    			{
    				console_scroll();
    				putstr_atbuf(buf_cons, win_x, 0, ccy, 0x000000, ">");
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

PUBLIC void console_scroll()
{
	int win_x = CONS_WIN_X;
	int win_y = CONS_WIN_Y;
	
	//滚动
	for (int i = 20; i < win_y; i++) 
	{
		for (int j = 0; j < win_x; j++) 
		{
		    buf_cons[j + i * win_x] = buf_cons[j + (i + 16) * win_x];
		}
	}
	for (int i = win_y - 20; i < win_y; i++) 
	{
		for (int j = 0; j < win_x; j++) 
		{
	    	buf_cons[j + i * win_x] = 0xffffff;
		}
	}
	
	yuanjiao(buf_cons);
}

PUBLIC void check_command(char *str)
{
	ccx = 8;
	ccy += 16;
	//识别指令
	if (strcmp(str, "info") == 0) { command_info(); }
	else if (strcmp(str, "cls") == 0) { command_cls(); }
	else if (strcmp(str, "") == 0) { ccy -= 32; }
	else { printstr("Command or program not found."); }
}
PUBLIC void command_cls()
{
	for (int i = 20; i < CONS_WIN_Y; i++) 
	{
		for (int j = 0; j < CONS_WIN_X; j++) 
		{
		    buf_cons[j + i * CONS_WIN_X] = 0xffffff;
		}
	}
	
	ccx = 8;
	ccy = 20;
	cls_right = 1;
	putstr_atbuf(buf_cons, CONS_WIN_X, 0, 20, 0x000000, ">");
	
	yuanjiao(buf_cons);
}

PUBLIC void command_info()
{
	printstr("XJ380 BetaVersion");
	endline();
	printstr("Copyright(C) XINGJI Studios 2017-2023");
}


PUBLIC void printstr(char *str)
{
	putstr_atbuf(buf_cons, CONS_WIN_X, ccx, ccy, 0x000000, str);
	//纪念一下找了一整天不知道问题出在哪里的代码
	//putstr_atbuf(buf_cons, CONS_WIN_X, ccx, ccy, 0xffffff, str);
	ccx += (strlen(str) << 3);
}

PUBLIC void endline()
{
	ccx = 8;
	ccy += 16;
}

PUBLIC void yuanjiao(u32 *buf)
{
	int xsize = CONS_WIN_X;
	int ysize = CONS_WIN_Y;
	int bc = 99;
	
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
}
