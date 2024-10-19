#include "../include/include.h"
void draw_point(SHEET_INFO *sht, UINT16* sheet_id, int x, int y, const SHEET_BUFFER &color) {
    SHEET_BUFFER *SheetBuffer = (SHEET_BUFFER *)(sht->sheet[*sheet_id].buffer);
    SheetBuffer += (sht->scrx * y + x);
    if (color.a != 255){
        sht->sheet[*sheet_id].col_type = NeedAlpha;
    }
    SheetBuffer->r = color.r;
    SheetBuffer->g = color.g;
    SheetBuffer->b = color.b;
    SheetBuffer->a = color.a;
}

void draw_rect(SHEET_INFO *sht, UINT16* sheet_id, int x1, int y1, int x2, int y2, const SHEET_BUFFER &color) {
    for (int y = y1; y < y2; y++) {
        for (int x = x1; x < x2; x++) {
            draw_point(sht, sheet_id, x, y, color);
        }
    }
}

void draw_line(SHEET_INFO *sht, UINT16* sheet_id, int x0, int y0, int x1, int y1, const SHEET_BUFFER &color) {
    int dx = x1 - x0;  
    int dy = y1 - y0;  
    int k;  
    int xIncrement;  
    int err;  
  
    // 确定迭代方向和步数  
    if (dx < 0) {  
        dx = -dx;  
        xIncrement = -1;  
    } else {  
        xIncrement = 1;  
    }  
  
    // 初始化斜率（2*dy，因为我们后面会乘以2来避免浮点运算）  
    k = 2 * dy;  
  
    // 初始化误差项和y的增量  
    err = k - dx;  
    int y = y0; 
  
    // 绘制直线上的点  
    for (int x = x0; x != x1 + xIncrement; x += xIncrement) {  
        // 这里可以添加代码来绘制点(x, y)  
        draw_point(sht, sheet_id, x, y, color);
  
        // 更新误差项  
        if (err < 0) {  
            err += 2 * dy;  
        } else {  
            y += (y1 > y0) ? 1 : -1;  
            err += 2 * (dy - dx);  
        }  
    }  
}
