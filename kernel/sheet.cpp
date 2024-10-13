#include "../include/include.h"
#include "../include/efi.h"

void init_sheet(const FrameBufferConfig &fbc, SHEET_INFO *sht) {
    sht->fbc = &fbc;
    sht->scrx = fbc.horizontal_resolution;
    sht->scry = fbc.vertical_resolution;
    sht->temp_buffer = malloc(sht->scrx * sht->scry * 4);
    sht->sheet_num = -1;
    SHEET_BUFFER *SheetBuffer = (SHEET_BUFFER *)(sht->temp_buffer);
    //WriteHex(fbc, 440, 432, (UINT64)SheetBuffer, WHITE);
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

/*
void create_sheet(struct SHEET_INFO &sht, UINT32 bx, UINT32 by, UINT32 x, UINT32 y, UINT32 type, INT16 index, UINT16* sheet_id) {
    if (index == -1) // 随便分配
    {
        for (int i = MAX_SHEET - 3; i > 1; i--) // 0 1 254 255 保留使用
        {
            if (sht.sheet[i].flag == 1 && i < MAX_SHEET - 3) // free
            {
                sht.sheet[i + 1].width = x;
                sht.sheet[i + 1].height = y;
                sht.sheet[i + 1].buffer = malloc(x * y * 4);
                sht.sheet[i + 1].bx = bx;
                sht.sheet[i + 1].by = by;
                sht.sheet[i + 1].type = type;
                sht.sheet[i + 1].flag = 1;
                sht.sheet_num++;
                *sheet_id = i;
                sht.sheet[i].number = sheet_id;
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
        sht.sheet[index].number = sheet_id;
    }
}

void delete_sheet(struct SHEET_INFO &sht, UINT16* sheet_id) {
    free(sht.sheet[*sheet_id].buffer);
    sht.sheet[*sheet_id].flag = 0;
    sht.sheet_num--;
    sht.sheet[*sheet_id].number = 0;
}

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
