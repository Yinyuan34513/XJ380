#pragma once
#include <proto.hpp>
// font/ttf/ttf.cpp
void print_box_ttf(SHEET_INFO *sht, UINT16 *sheet_id, char *buf, SHEET_BUFFER fc, unsigned x, unsigned y, int size);
void init_ttf();