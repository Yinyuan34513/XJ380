#include "include.h"

u32 buf_con[480 * 270];

PUBLIC void start_console()
{
	//光标坐标
	int ccx;
	int ccy;
	//初生化
	ccx = 0;
	ccy = 0;
	
	//光标颜色
	u32 cur_col = 0xfffff;
	
	//窗口
    make_console_window(buf_con, 480, 68, "Console", 99);
    putstr_atbuf(buf_con, 480, 0, 20, 0xffffff, ">");
    putstr_atbuf(buf_con, 480, 0, 40, cur_col, " ");
    
    return;
}