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
        //WSOD_Cannot_Alloc_Memory(fbc);  // 死了啦，都是你害的啦
    }
    sht->sheet_num = -1;
    SHEET_BUFFER *SheetBuffer = (SHEET_BUFFER *)(sht->temp_buffer);
    for (int y = 0; y < sht->scry; y++) {
        for (int x = 0; x < sht->scrx; x++) {
            SheetBuffer->r = 0x00;
            SheetBuffer->g = 0x00;
            SheetBuffer->b = 0x00;
            SheetBuffer->a = 0xff;
            if ((sht->scrx * y + x) % 2 != 0) {
                SheetBuffer->r = 0xff;
                SheetBuffer->g = 0xff;
                SheetBuffer->b = 0xff;
                SheetBuffer->a = 0xff;
            }
            SheetBuffer++;
        }
    }
    for (int i = 0; i < MAX_SHEET - 1; i++) {
        sht->sheet[i].flag = 0;
    }
}

void create_sheet(SHEET_INFO *sht, UINT32 bx, UINT32 by, UINT32 x, UINT32 y, UINT32 type, INT16 index, UINT16* sheet_id) {
    if (sht->sheet_num >= 253) {
        // 图层满了，直接去世
        return;
    }
    if (sht->sheet[index].flag == 0) {
        // 指定图层高度为空
        // 非必要不要用
        // 直接分配下一个
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
    } else if (index >= 0) {
        // 指定图层高度
        for (int i = MAX_SHEET - 4; i >= sht->sheet_num; i--) {
            // 把所有图层位移一下
            if (sht->sheet[i].flag == 1) {
                sht->sheet[i + 1].buffer = sht->sheet[i].buffer;
                sht->sheet[i + 1].bx = sht->sheet[i].bx;
                sht->sheet[i + 1].by = sht->sheet[i].by;
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

    int tx = sht->sheet[*sheet_id].width;
    int ty = sht->sheet[*sheet_id].height;
    SHEET_BUFFER *SheetBuffer = (SHEET_BUFFER *)(sht->sheet[*sheet_id].buffer);
    for (int y = 0; y < tx; y++) {
        for (int x = 0; x < ty; x++) {
            SheetBuffer->r = 0x00;
            SheetBuffer->g = 0x00;
            SheetBuffer->b = 0x00;
            SheetBuffer->a = 0x00;
            SheetBuffer++;
        }
    }
}

void delete_sheet(SHEET_INFO *sht, UINT16* sheet_id) {
    UINT16 i = *sheet_id;
    sht->sheet[i].flag = 0;
    free(sht->sheet[i].buffer);
    for (int j = i; j < MAX_SHEET - 4; j++) {
        if (sht->sheet[j + 2].flag == 0) {
            break;
        }
        sht->sheet[j].buffer = sht->sheet[j + 1].buffer;
        sht->sheet[j].bx = sht->sheet[j + 1].bx;
        sht->sheet[j].by = sht->sheet[j + 1].by;
        sht->sheet[j].height = sht->sheet[j + 1].height;
        sht->sheet[j].number = sht->sheet[j + 1].number;
        sht->sheet[j].type = sht->sheet[j + 1].type;
        sht->sheet[j].width = sht->sheet[j + 1].width;

        sht->sheet[j].flag = 1;
        sht->sheet[j + 1].flag = 0;
    }
    sht->sheet_num--;
}

void refresh_sheet(SHEET_INFO *sht) {
    static char pixel_writer_buf[sizeof(RGBRWriter) > sizeof(BGRRWriter) ? sizeof(RGBRWriter) : sizeof(BGRRWriter)];  
    static PixelWriter* pixel_writer = nullptr;  
  
    switch (sht->fbc->pixel_format) {  
        case PixelFormat::kRGBR:  
            pixel_writer = new(pixel_writer_buf) RGBRWriter(*sht->fbc); // 使用 placement new  
            break;  
        case PixelFormat::kBGRR:  
            pixel_writer = new(pixel_writer_buf) BGRRWriter(*sht->fbc);  
            break;  
        default:  
            break;  
    }

    for (int i = 0; i < MAX_SHEET; i++) {
        if (sht->sheet[i].flag == 1) {
            SHEET_BUFFER *TempBuffer = (SHEET_BUFFER *)(sht->temp_buffer);
            SHEET_BUFFER *SheetBuffer = (SHEET_BUFFER *)(sht->sheet[i].buffer);
            TempBuffer += (sht->scrx * sht->sheet[i].by + sht->sheet[i].bx);
            for (int y = 0; y < sht->sheet[i].height; y++) {
                for (int x = 0; x < sht->sheet[i].width; x++) {
                    //显示颜色 = 源像素颜色 * alpha / 255 + 背景颜色 * (255 - alpha) / 255
                    if (SheetBuffer->a == 255) {
                        TempBuffer->r = SheetBuffer->r;
                        TempBuffer->g = SheetBuffer->g;
                        TempBuffer->b = SheetBuffer->b;
                    } else if (SheetBuffer->a == 0) {
                        // pass
                    } else {
                        TempBuffer->r = SheetBuffer->r * SheetBuffer->a / 255 + TempBuffer->r * (255 - SheetBuffer->a) / 255;
                        TempBuffer->g = SheetBuffer->g * SheetBuffer->a / 255 + TempBuffer->g * (255 - SheetBuffer->a) / 255;
                        TempBuffer->b = SheetBuffer->b * SheetBuffer->a / 255 + TempBuffer->b * (255 - SheetBuffer->a) / 255;
                    }
                    TempBuffer++;
                    SheetBuffer++;
                }
                TempBuffer += (sht->scrx - sht->sheet[i].width);
            }
        }
    }

    // 刷新
    SHEET_BUFFER *OutputBuffer = (SHEET_BUFFER *)(sht->temp_buffer);
    for (int y = 0; y < sht->scry; y++) {
        for (int x = 0; x < sht->scrx; x++) {
            pixel_writer->Write(x, y, {OutputBuffer->r, OutputBuffer->g, OutputBuffer->b});
            OutputBuffer++;
        }
    }
}

void refresh_part_sheet(SHEET_INFO *sht, int bex1, int bey1, int bex2, int bey2) {
    static char pixel_writer_buf[sizeof(RGBRWriter) > sizeof(BGRRWriter) ? sizeof(RGBRWriter) : sizeof(BGRRWriter)];  
    static PixelWriter* pixel_writer = nullptr;  
  
    switch (sht->fbc->pixel_format) {  
        case PixelFormat::kRGBR:  
            pixel_writer = new(pixel_writer_buf) RGBRWriter(*sht->fbc); // 使用 placement new  
            break;  
        case PixelFormat::kBGRR:  
            pixel_writer = new(pixel_writer_buf) BGRRWriter(*sht->fbc);  
            break;  
        default:  
            break;  
    }

    int vx, vy;

    for (int i = 0; i < MAX_SHEET; i++) {
        if (sht->sheet[i].flag == 1) {
            SHEET_BUFFER *TempBuffer = (SHEET_BUFFER *)(sht->temp_buffer);
            SHEET_BUFFER *SheetBuffer = (SHEET_BUFFER *)(sht->sheet[i].buffer);
            TempBuffer += (sht->scrx * sht->sheet[i].by + sht->sheet[i].bx);
            for (int y = 0; y < sht->sheet[i].height; y++) {
                vy = sht->sheet[i].by + y;
                for (int x = 0; x < sht->sheet[i].width; x++) {
                    vx = sht->sheet[i].bx + x;
                    if (bex1 <= vx && vx < bex2 && bey1 <= vy && vy < bey2) {
                        //显示颜色 = 源像素颜色 * alpha / 255 + 背景颜色 * (255 - alpha) / 255
                        if (SheetBuffer->a == 255) {
                            TempBuffer->r = SheetBuffer->r;
                            TempBuffer->g = SheetBuffer->g;
                            TempBuffer->b = SheetBuffer->b;
                        } else if (SheetBuffer->a == 0) {
                            // pass
                        } else {
                            TempBuffer->r = SheetBuffer->r * SheetBuffer->a / 255 + TempBuffer->r * (255 - SheetBuffer->a) / 255;
                            TempBuffer->g = SheetBuffer->g * SheetBuffer->a / 255 + TempBuffer->g * (255 - SheetBuffer->a) / 255;
                            TempBuffer->b = SheetBuffer->b * SheetBuffer->a / 255 + TempBuffer->b * (255 - SheetBuffer->a) / 255;
                        }
                    }
                    TempBuffer++;
                    SheetBuffer++;
                }
                TempBuffer += (sht->scrx - sht->sheet[i].width);
            }
        }
    }

    // 刷新
    SHEET_BUFFER *OutputBuffer = (SHEET_BUFFER *)(sht->temp_buffer);
    OutputBuffer += (sht->scrx * bey1 + bex1);
    for (int y = bey1; y < bey2; y++) {
        for (int x = bex1; x < bex2; x++) {
            pixel_writer->Write(x, y, {OutputBuffer->r, OutputBuffer->g, OutputBuffer->b});
            OutputBuffer++;
        }
        OutputBuffer += (sht->scrx - bex2 + bex1);
    }
}
