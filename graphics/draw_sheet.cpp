#include "../include/include.h"

void draw_point(struct SHEET_INFO &sht, UINT16* sheet_id, int x, int y, const SHEET_BUFFER &color) {
    SHEET_BUFFER *SheetBuffer = (SHEET_BUFFER *)(sht.sheet[*sheet_id].buffer);
    SheetBuffer += (sht.scrx * y + x);
    if (color.a != 255){
        sht.sheet[*sheet_id].col_type = NeedAlpha;
    }
    SheetBuffer->r = color.r;
    SheetBuffer->g = color.g;
    SheetBuffer->b = color.b;
    SheetBuffer->a = color.a;
}
