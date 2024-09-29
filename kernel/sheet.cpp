#include "../include/include.h"
void init_sheet(const FrameBufferConfig &fbc, struct SHEET_INFO &sht) {
    sht.fbc = &fbc;
    sht.scrx = fbc.horizontal_resolution;
    sht.scry = fbc.vertical_resolution;
    for (int i = 0; i < MAX_SHEET; i++) {
        sht.sheet[i].flag = 0;
    }
}

void create_sheet(struct SHEET_INFO &sht, UINT32 bx, UINT32 by, UINT32 x, UINT32 y, UINT32 type, INT16 index, UINT16* sheet_id) {
    if (index == -1) // 随便分配
    {
        for (int i = 253; i > 1; i--) // 0 1 254 255 保留使用
        {
            if (sht.sheet[i].flag == 0) // free
            {
                sht.sheet[i].width = x;
                sht.sheet[i].height = y;
                sht.sheet[i].buffer = malloc(x * y * 4);
                sht.sheet[i].bx = bx;
                sht.sheet[i].by = by;
                sht.sheet[i].type = type;
                sht.sheet[i].flag = 1;
                sht.sheet_num++;
                *sheet_id = i;
                break;
            }
        }
    }
    else if (index != -1 && index <= 255)
    {
        sht.sheet[index].width = x;
        sht.sheet[index].height = y;
        sht.sheet[index].buffer = malloc(x * y * 4);
        sht.sheet[index].bx = bx;
        sht.sheet[index].by = by;
        sht.sheet[index].type = type;
        sht.sheet[index].flag = 1;
        sht.sheet_num++;
        *sheet_id = index;
    }
}
