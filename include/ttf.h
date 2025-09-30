#pragma once
#include <proto.hpp>
// font/ttf/ttf.cpp
void print_box_ttf(SHEET_INFO *sht, uint16_t *sheet_id, char *buf, SHEET_BUFFER fc, unsigned x, unsigned y, int size);
void print_box_ttfl(SHEET_INFO *sht, uint16_t *sheet_id, char *buf, SHEET_BUFFER fc,
                   /*unsigned bc, */ unsigned x, unsigned y, int size, uint32_t *i_width);
void print_fmt_box_ttf(SHEET_INFO *sht, uint16_t *sheet_id, SHEET_BUFFER fc, int x, int y, int size, const char *fmt,
                       ...);
void print_fmt_box_ttf(SHEET_INFO *sht, uint16_t *sheet_id, SHEET_BUFFER fc, int x, int y, int size, uint32_t *i_width, const char *fmt,
                       ...);
void print_box_ttfc(SHEET_INFO *sht, uint16_t *sheet_id, char *buf, SHEET_BUFFER fc,
				   /*unsigned bc, */ unsigned x, unsigned y, int size);
void init_ttf();