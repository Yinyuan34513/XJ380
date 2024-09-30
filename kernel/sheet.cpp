#include "../include/include.h"

void init_sheet(const FrameBufferConfig &fbc, struct SHEET_INFO &sht) {
    sht.fbc = &fbc;
    sht.scrx = fbc.horizontal_resolution;
    sht.scry = fbc.vertical_resolution;
    for (int i = 0; i < MAX_SHEET - 1; i++) {
        sht.sheet[i].flag = 0;
    }
}

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
    /*
    static char pixel_writer_buf[sizeof(RGBRWriter) > sizeof(BGRRWriter) ? sizeof(RGBRWriter) : sizeof(BGRRWriter)];  
    static PixelWriter* pixel_writer = nullptr;  
  
    switch (sht.fbc->pixel_format) {  
        case PixelFormat::kRGBR:  
            pixel_writer = new(pixel_writer_buf) RGBRWriter(sht.fbc); // 使用 placement new  
            break;  
        case PixelFormat::kBGRR:  
            pixel_writer = new(pixel_writer_buf) BGRRWriter(sht.fbc);  
            break;  
        default:  
            break;  
    }  
    */

    for (int i = 0; i < MAX_SHEET - 1; i++) {
        SHEET_BUFFER *EfiMemory = (SHEET_BUFFER *)(sht.sheet[i].buffer);
    }
   
    //pixel_writer->Write(x, y, c);
}
