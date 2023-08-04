#include "include.h"
//显示光标
PUBLIC void init_mouse_cursor(int *mouse, int bc)
{
    //从XJ380重置前移植过来的丑光标
    static char cursor[16][16] = {
        "*...............",
		"**..............",
		"**..............",
		"*O*.............",
		"*OO*............",
		"*OOO*...........",
		"*OOOO*..........",
		"*OOOOO*.........",
		"*OOOOOO*........",
		"*OOOOOOO*.......",
		"*OOOO****.......",
		"*O*OO*..........",
		"**.*O*..........",
		"*..*OO*.........",
		"....*O*.........",
		".....**.........",
    };
    
    int x, y;
    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            if (cursor[y][x] == '*') {
                mouse[y * 16 + x] = 0;	//如果是*则填充黑色
            }
            if (cursor[y][x] == 'O') {
                mouse[y * 16 + x] = 0xffffff;	//如果是O则填充白色
            }
            if (cursor[y][x] == '.') {
                mouse[y * 16 + x] = bc;		//如果是.则填充透明色
            }
        }
    }
    return;
}