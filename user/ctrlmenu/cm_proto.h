#pragma once

#include <stdint.h>

extern HDLE handle;

extern UINT64 win_width;
extern UINT64 win_height;

extern int cindex;

void draw_background();
void process_left_key(int x, int y);
void draw_mainpage();
