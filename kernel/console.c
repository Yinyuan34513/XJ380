#include "include.h"

u32 buf_cons[480 * 290];

PUBLIC void start_console(SHEET *sht_cons)
{
	//光标坐标
	int ccx;
	int ccy;
	//初生化
	ccx = 0;
	ccy = 0;
	
	//光标颜色
	u32 cur_col = 0x000000;
	
	//窗口
    sheet_setbuf(sht_cons, buf_cons, 480, 290, 99);
    
    make_window(buf_cons, 480, 68, "Console", 99);
    putstr_atbuf(buf_cons, 480, 0, 20, 0x000000, ">");
    putstr_atbuf(buf_cons, 480, ccx * 8 + 8, ccy * 8 + 20, cur_col, " ");
    
    return;
}