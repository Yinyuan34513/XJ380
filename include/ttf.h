#pragma once
#include <proto.hpp>
// font/ttf/ttf.cpp
void print_box_ttf(SHEET_INFO *sht, uint16_t *sheet_id, char *buf, SHEET_BUFFER fc, unsigned x, unsigned y, int size);
void print_fmt_box_ttf(SHEET_INFO *sht, uint16_t *sheet_id, SHEET_BUFFER fc, int x, int y, int size, const char *fmt,
                       ...);
void init_ttf();