#include "../include/include.h"
#include "./mouse.hpp"

void draw_mouse(SHEET_INFO *sht, UINT16* sheet_id) {
    for (int y = 0; y < 21; y++) {
        for (int x = 0; x < 10; x++) {
            if (mouse[y][x] == '.')
            {
                draw_point(sht, sheet_id, x, y, {0, 0, 0, 0});
            }
            else if (mouse[y][x] == 'w')
            {
                draw_point(sht, sheet_id, x, y, WHITE);
            }
            else if (mouse[y][x] == '@')
            {
                draw_point(sht, sheet_id, x, y, BLACK);
            }
        }
    }
}

void move_mouse(SHEET_INFO *sht, UINT16* sheet_id, int px, int py) {
    sht->sheet[*sheet_id].bx = px;
    sht->sheet[*sheet_id].by = py;
}