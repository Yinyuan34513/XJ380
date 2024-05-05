#include "include.h"

#define CONS_WIN_X	640
#define CONS_WIN_Y	360

#define COL_BLACK	0x000000

PUBLIC u32 buf_cons[CONS_WIN_X * CONS_WIN_Y];
PUBLIC u32 buf_win[190 * 100];


//光标坐标
PRIVATE int ccx = 8;
PRIVATE int ccy = 36;

PRIVATE int cls_right = 0;

int adder = ADR_DISKIMG;

char p[3];
int DIRADR=ADR_DISKIMG;
struct FILEINFO *finfo;
int fileloop=0;
    
int t;
char key_in[1];
int key_in_m = 0;
char keys_in[256];
int ref_right = 1;
char key;

PUBLIC void start_console(SHEET *sht_cons)	//这个程序全是shit山代码！能别动就别动！否则全崩！！！！！！！！
{
	//初生化
	//ccx = 8;
	//ccy = 20;
	p[0] = (char*) adder;
	finfo = (struct FILEINFO *) (ADR_DISKIMG);
	
	//窗口大小
	int win_x;
	int win_y;
	//初生化
	win_x = CONS_WIN_X;
	win_y = CONS_WIN_Y;
	
	//光标颜色
	u32 cur_col = 0x000000;
	
	//窗口
	SHEET *sht_win  = sheet_alloc(&shtctl);
    sheet_setbuf(sht_cons, buf_cons, win_x, win_y, 99);
	sheet_setbuf(sht_win, buf_win, 190, 100, 99);
    
    make_window(buf_cons, win_x, win_y, "Terminal", 99);
	putstr_atbuf(buf_cons, win_x, 0, 20, 0x000000, "Copyright(C)XINGJI Studios 2017-2023");


    putstr_atbuf(buf_cons, win_x, 0, 36, 0x000000, ">");

	t = get_ticks();	

	memset(keys_in, 0, sizeof keys_in);
    strcpy(keys_in, "");

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
    			key_in_m = -1;
    			
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
    		else if (key_in[0] == 0x08)	//BUG里写了个退格
    		{
    			if (ccx > 8)
    			{
					putstr_atbuf(buf_cons, win_x, ccx-8, ccy, 0xffffff, keys_in[key_in_m]);
    				ccx -= 16;
					//keys_in删除一个字符
					key_in_m--;
					keys_in[key_in_m] = '\0';
    			}
    			//else if (ccx <= 8)
    			//{
    				//ccx = win_x - 168;
    				//ccy -= 8;
    			//}
				sheet_refresh(sht_cons, 0, 0, win_x, win_y);
				
    		}
    		else
    		{
				keys_in[key_in_m] = key_in[0];
				key_in[1]='\0';
				putstr_atbuf(buf_cons, win_x, ccx, ccy, 0x000000, key_in);
    		}
    		if (ccx + 8 >= win_x - 7)
    		{
    			if (ccy + 32 >= win_y - 15)
    			{
    				console_scroll();
    				putstr_atbuf(buf_cons, win_x, 0, ccy, 0x000000, ">");
    			}
    			else
    			{
    				ccy += 8;
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
	else if (strcmp(str, "cls") == 0) { command_cls(buf_cons); }
	else if (strcmp(str, "dir") == 0)  {command_dir(); }
	else if (strcmp(str, "") == 0) { ccy -= 32; }
	else if (strcmp(str, "explorer") == 0) { command_explorer(); }
	else if (part_strcmp(str, "open ", 5) == 0) { command_open(); }
	else { err_Command_or_program_not_found(); }
}

PUBLIC void command_open() {
	int win_x = CONS_WIN_X;
	int win_y = CONS_WIN_Y;
    char str[32];
	int tempccy=ccy;
    // Initialize
    memset(str, ' ', sizeof(str));
    int x, y;
    y = 0;
    for (x = 5; keys_in[x] != 0; x++) {
        if (keys_in[x] == '.' && y <= 8) {
            y = 8;
        } else {
            str[y] = keys_in[x];
            // Convert to uppercase//TODO
            if ('a' <= str[y] && str[y] <= 'z') {
                str[y] -= 32;
            }
            y++;
        }
    }
    // Find file
	
    for (x = 0; x < 224; x++) {
        if (finfo[x].name[0] == 0) {
            break;
        }
        if ((finfo[x].type & 0x18) == 0) {
            int match = 1;
            for (y = 0; y < 11; y++) {
                if (finfo[x].name[y] != str[y]) {
                    match = 0;
                    break;
                }
            }
            if (match) {
                // File found
				//printstr("File found.\n");//TODO
                int yy = 0;
                int size = finfo[x].size;
				//
                char *ptr = (char *) (finfo[x].clustno * 512 + 0x003e00 + 0x15000);
				char buf[16];
				//*ptr='A';
				//buf[0]=*ptr;
				//buf[1]='\0';
				
				//printstr(buf);
				ccy--;
                while (yy < size) {

                    char buf[39];
                    int xx = 0;
                    while (xx < 38 && ptr[yy] != '\0' && ptr[yy]!='\n') {
                        buf[xx] = ptr[yy++];
                        xx++;
                    }
					buf[xx] = '\0';

					

                    putstr_atbuf(buf_cons, CONS_WIN_X, 8, tempccy, 0x000000, buf);

					if (ccy + 32 >= win_y - 15)
					{
						console_scroll();
					}
                    if (yy % 80 == 79) {
                        console_scroll();
						//ccy+=16;
                    }
					
					tempccy+=16;
					ccy+=16;

                    yy++;
                }
                break;
            }
        }
    }
    if (x == 224) {
		// File not found
		printstr("File not found.");
    }
}




PUBLIC void command_cls()
{
	for (int loop1=0;loop1<320;loop1++)
	{
		for (int loop2=10;loop2<180;loop2++)
		{
			putstr_atbuf(buf_cons, CONS_WIN_X, loop1, loop2, 0xffffff, ".");
		}
	}
	
    ccy = -12;	
	putstr_atbuf(buf_cons, CONS_WIN_X, 0, 20, 0x000000, ">");
	
	yuanjiao(buf_cons);
}

PUBLIC void command_dir()
{
	ccx = 8;
	printstr("NAME    EXT");
	endline();
	while(finfo[fileloop].name[0] != 0x00)
	{
		putstr_atbuf(buf_cons, CONS_WIN_X, ccx, ccy + fileloop * 16, 0x000000, finfo[fileloop].name);
		fileloop++;
	}
	endline();
	endline();
}

PUBLIC void command_info()
{
	
	//writeMem(0xffffff,0x0a);
	ccx = 8;
	printstr("Copyright(C) XINGJI Studios 2017-2023");
	endline();
	// Copyright(C) XINGJI Studios 2017-2023
	return;

}	

PUBLIC void command_explorer()
{
	UpExplorer();
	return;
}

PUBLIC void printstr(char *str)
{
	putstr_atbuf(buf_cons, CONS_WIN_X, 8, ccy, 0x000000, str);
	ccx += (strlen(str) << 3);
}

PUBLIC void err_Command_or_program_not_found()
{
	printstr("Command or program not found.");
	endline();
}

PUBLIC void endline()
{
	ccx = 8;
	if (ccy + 16 < CONS_WIN_Y) { ccy += 16; }
	else if (ccy + 16 >= CONS_WIN_Y) { console_scroll(); }
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
/* test */
