#include "../include/include.h"
#include "../include/efi.h"

void init_sheet(const FrameBufferConfig &fbc, SHEET_INFO *sht) {
    sht->fbc = &fbc;
    sht->scrx = fbc.horizontal_resolution;
    sht->scry = fbc.vertical_resolution;
    //sht->temp_buffer = 0; // 测试WSOD
    sht->temp_buffer = malloc((sht->scrx * sht->scry * 4) + 4); // 多分配4字节防止溢出啥的
    if (sht->temp_buffer == NULL) {
        // 没分配成，去世。
        WSOD_Cannot_Alloc_Memory(fbc);  // 死了啦，都是你害的啦
    }
    sht->sheet_num = -1;
    SHEET_BUFFER *SheetBuffer = (SHEET_BUFFER *)(sht->temp_buffer);
    for (int y = 0; y < sht->scry; y++) {
        for (int x = 0; x < sht->scrx; x++) {
            SheetBuffer->r = 0x00;
            SheetBuffer->g = 0x00;
            SheetBuffer->b = 0x00;
            SheetBuffer->a = 0xff;
            SheetBuffer++;
        }
    }
    for (int i = 0; i < MAX_SHEET - 1; i++) {
        sht->sheet[i].flag = 0;
        sht->sheet[i].col_type = NoAlpha;
    }
}

void create_sheet(SHEET_INFO *sht, UINT32 bx, UINT32 by, UINT32 x, UINT32 y, UINT32 type, INT16 index, UINT16* sheet_id) {
    if (sht->sheet_num >= 253) {
        // 图层满了，直接去世
        return;
    }
    if (index >= sht->sheet_num) {
        // 图层高度 > 已有最高图层
        // 直接分配下一个
        UINT16 i = sht->sheet_num + 1;
        *sheet_id = i;

        sht->sheet[i].flag = 1;
        sht->sheet[i].bx = bx;
        sht->sheet[i].by = by;
        sht->sheet[i].width = x;
        sht->sheet[i].height = y;
        sht->sheet[i].type = type;
        sht->sheet[i].number = sheet_id;
        sht->sheet[i].buffer = malloc((x * y * 4) + 4); // 多分配4字节防止溢出啥的

        sht->sheet_num++;
    } else if (index >= 0) {
        // 指定图层高度
        for (int i = MAX_SHEET - 4; i >= sht->sheet_num; i--) {
            // 把所有图层位移一下
            if (sht->sheet[i].flag == 1) {
                sht->sheet[i + 1].buffer = sht->sheet[i].buffer;
                sht->sheet[i + 1].bx = sht->sheet[i].bx;
                sht->sheet[i + 1].by = sht->sheet[i].by;
                sht->sheet[i + 1].col_type = sht->sheet[i].col_type;
                sht->sheet[i + 1].height = sht->sheet[i].height;
                sht->sheet[i + 1].number = sht->sheet[i].number;
                sht->sheet[i + 1].type = sht->sheet[i].type;
                sht->sheet[i + 1].width = sht->sheet[i].width;

                sht->sheet[i + 1].flag = 1;
                sht->sheet[i].flag = 0;
            }
        }
        // 分配
        UINT16 i = index;
        *sheet_id = i;

        sht->sheet[i].flag = 1;
        sht->sheet[i].bx = bx;
        sht->sheet[i].by = by;
        sht->sheet[i].width = x;
        sht->sheet[i].height = y;
        sht->sheet[i].type = type;
        sht->sheet[i].number = sheet_id;
        sht->sheet[i].buffer = malloc((x * y * 4) + 4); // 多分配4字节防止溢出啥的

        sht->sheet_num++;
    } else {
        // 随便分配
        // 直接分配下一个
        UINT16 i = sht->sheet_num + 1;
        *sheet_id = i;

        sht->sheet[i].flag = 1;
        sht->sheet[i].bx = bx;
        sht->sheet[i].by = by;
        sht->sheet[i].width = x;
        sht->sheet[i].height = y;
        sht->sheet[i].type = type;
        sht->sheet[i].number = sheet_id;
        sht->sheet[i].buffer = malloc((x * y * 4) + 4); // 多分配4字节防止溢出啥的

        sht->sheet_num++;
    }
}

void delete_sheet(SHEET_INFO *sht, UINT16* sheet_id) {
    UINT16 i = *sheet_id;
    sht->sheet[i].flag = 0;
    free(sht->sheet[i].buffer);
    for (int j = i; j < MAX_SHEET - 4; j++) {
        sht->sheet[j].buffer = sht->sheet[j + 1].buffer;
        sht->sheet[j].bx = sht->sheet[j + 1].bx;
        sht->sheet[j].by = sht->sheet[j + 1].by;
        sht->sheet[j].col_type = sht->sheet[j + 1].col_type;
        sht->sheet[j].height = sht->sheet[j + 1].height;
        sht->sheet[j].number = sht->sheet[j + 1].number;
        sht->sheet[j].type = sht->sheet[j + 1].type;
        sht->sheet[j].width = sht->sheet[j + 1].width;

        sht->sheet[j].flag = 1;
        sht->sheet[j + 1].flag = 0;

        if (sht->sheet[j + 2].flag == 0) {
            break;
        }
    }
}

/*
void refresh_sheet(struct SHEET_INFO &sht) {
    SHEET_BUFFER *TempBuffer = (SHEET_BUFFER *)(sht.temp_buffer);
    static char pixel_writer_buf[sizeof(RGBRWriter) > sizeof(BGRRWriter) ? sizeof(RGBRWriter) : sizeof(BGRRWriter)];  
    static PixelWriter* pixel_writer = nullptr;  
  
    switch (sht.fbc->pixel_format) {  
        case PixelFormat::kRGBR:  
            pixel_writer = new(pixel_writer_buf) RGBRWriter(*sht.fbc); // 使用 placement new  
            break;  
        case PixelFormat::kBGRR:  
            pixel_writer = new(pixel_writer_buf) BGRRWriter(*sht.fbc);  
            break;  
        default:  
            break;  
    }

    for (int i = 0; i < sht.sheet_num; i++) {
        if (sht.sheet[i].col_type == NoAlpha) {
            SHEET_BUFFER *SheetBuffer = (SHEET_BUFFER *)(sht.sheet[i].buffer);
            for (int y = sht.sheet[i].by; y < sht.sheet[i].by + sht.sheet[i].height; y++) {
                for (int x = sht.sheet[i].bx; x < sht.sheet[i].bx + sht.sheet[i].width; x++) {
                    TempBuffer->r = SheetBuffer->r;
                    TempBuffer->g = SheetBuffer->g;
                    TempBuffer->b = SheetBuffer->b;
                    SheetBuffer++;
                }
            }
        } else {
            SHEET_BUFFER *SheetBuffer = (SHEET_BUFFER *)(sht.sheet[i].buffer);
            for (int y = sht.sheet[i].by; y < sht.sheet[i].by + sht.sheet[i].height; y++) {
                for (int x = sht.sheet[i].bx; x < sht.sheet[i].bx + sht.sheet[i].width; x++) {
                    //显示颜色 = 源像素颜色 * alpha / 255 + 背景颜色 * (255 - alpha) / 255
                    TempBuffer->r = SheetBuffer->r * SheetBuffer->a / 255 + TempBuffer->r * (255 - SheetBuffer->a) / 255;
                    TempBuffer->g = SheetBuffer->g * SheetBuffer->a / 255 + TempBuffer->g * (255 - SheetBuffer->a) / 255;
                    TempBuffer->b = SheetBuffer->b * SheetBuffer->a / 255 + TempBuffer->b * (255 - SheetBuffer->a) / 255;
                    SheetBuffer++;
                }
            }
        }
    }

    // 刷新
    for (int y = 0; y < sht.scry; y++) {
        for (int x = 0; x < sht.scrx; x++) {
            pixel_writer->Write(x, y, {TempBuffer->r, TempBuffer->g, TempBuffer->b});
        }
    }
}
*/
