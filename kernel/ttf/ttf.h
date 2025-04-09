#pragma once
#include "../../include/include.h"
void TTF_Draw_Char(SHEET_INFO *sht, UINT16 *sheet_id,const FrameBufferConfig &fbc,int x, int y, int ch, unsigned int bc, unsigned fc);
void TTF_Print(SHEET_INFO *sht, UINT16 *sheet_id,const FrameBufferConfig &fbc,char *buf);
void set_size(int s1);
void init_ttf(SHEET_INFO *sht, UINT16 *sheet_id,const FrameBufferConfig &fbc);