#include "include.h"

PUBLIC void init_mouse_cursor(int *mouse, int bc)
{
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
                mouse[y * 16 + x] = 0;
            }
            if (cursor[y][x] == 'O') {
                mouse[y * 16 + x] = 0xffffff;
            }
            if (cursor[y][x] == '.') {
                mouse[y * 16 + x] = bc;
            }
        }
    }
    return;
}