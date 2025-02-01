#include "../../include/include.h"

void init_sheet(const FrameBufferConfig &fbc, SHEET_INFO *sht)
{
    write_serial_string("Initializing Sheet Manager...\n");
    sht->fbc = &fbc;
    sht->scrx = fbc.horizontal_resolution;
    sht->scry = fbc.vertical_resolution;
    sht->start = NULL;
    sht->temp_buffer = malloc((sht->scrx * sht->scry * 4)); // 多分配4字节防止溢出啥的
    if (sht->temp_buffer == NULL)
    {
        // 没分配成，去世。
        // WSOD_Cannot_Alloc_Memory(fbc);  // 死了啦，都是你害的啦
    }
    sht->sheet_num = 0;
    SHEET_BUFFER *SheetBuffer = (SHEET_BUFFER *)(sht->temp_buffer);
    for (int y = 0; y < sht->scry; y++)
    {
        for (int x = 0; x < sht->scrx; x++)
        {
            SheetBuffer->r = 0x00;
            SheetBuffer->g = 0x00;
            SheetBuffer->b = 0x00;
            SheetBuffer->a = 0xff;
            if ((sht->scrx * y + x) % 2 != 0)
            {
                SheetBuffer->r = 0xff;
                SheetBuffer->g = 0xff;
                SheetBuffer->b = 0xff;
                SheetBuffer->a = 0xff;
            }
            SheetBuffer++;
        }
    }
    write_serial_string("Sheet Manager Initialize Success.\n");
}

void create_sheet(SHEET_INFO *sht, UINT32 bx, UINT32 by, UINT32 x, UINT32 y, UINT32 type, INT16 index, UINT16 *sheet_id)
{
    SHEET *new_sheet = (SHEET *)(malloc(sizeof(SHEET))); // 开辟内存空间
    SHEET *front_p = sht->start;
    if (index > 0)
    {
        // 指定图层高度
        if (front_p != NULL)
        {
            SHEET *record = front_p;
            while (true)
            {
                if (front_p->next == NULL)
                {
                    break;
                }
                else if (front_p->next->index >= index)
                {
                    break;
                }
                else if (record == front_p->next || front_p == front_p->next)
                {
                    front_p->next = NULL;
                    break;
                }
                front_p = front_p->next;
            }

            if (front_p->next != NULL)
            {
                // 后继有人，给他踢出去换成自己
                new_sheet->next = front_p->next;  // 把自己的下一个标记为曾经的
                new_sheet->front = front_p;       // 把自己的上一个标记为上一个
                front_p->next->front = new_sheet; // 把曾经的上一个标记为自己
                front_p->next = new_sheet;        // 把自己的上一个的下一个标记为自己
                front_p = new_sheet->next;        // 指针指向自己的下一个（曾经的）
                record = front_p;
                // 给后面的全++
                while (true)
                {
                    front_p->index++;
                    if (front_p->next == NULL)
                    {
                        break;
                    }
                    else if (record == front_p->next || front_p == front_p->next)
                    {
                        front_p->next = NULL;
                        break;
                    }
                    front_p = front_p->next;
                }

                // 分配
                *sheet_id = index;

                new_sheet->index = index;
                new_sheet->bx = bx;
                new_sheet->by = by;
                new_sheet->width = x;
                new_sheet->height = y;
                new_sheet->type = type;
                new_sheet->buffer = malloc((x * y * 4));

                sht->sheet_num++;
            }
            else
            {
                // 哦亲爱的孩子你是第一个分配的
                new_sheet->front = front_p;
                front_p->next = new_sheet;

                // 分配
                *sheet_id = index;

                new_sheet->index = index;
                new_sheet->bx = bx;
                new_sheet->by = by;
                new_sheet->width = x;
                new_sheet->height = y;
                new_sheet->type = type;
                new_sheet->buffer = malloc((x * y * 4));

                sht->sheet_num++;
            }
        }
        else
        {
            sht->start = new_sheet;

            *sheet_id = 1;

            new_sheet->index = 0;
            new_sheet->bx = bx;
            new_sheet->by = by;
            new_sheet->width = x;
            new_sheet->height = y;
            new_sheet->type = type;
            new_sheet->buffer = malloc((x * y * 4));

            sht->sheet_num++;
        }
    }
    else
    {
        // 随便分配
        // 直接分配下一个
        if (front_p != NULL)
        {
            for (int i = 0; i < sht->sheet_num; i++)
            {
                if (front_p->next == NULL)
                {
                    // 链表.exe
                    front_p->next = new_sheet;
                    new_sheet->front = front_p;
                    break;
                }
                front_p = front_p->next;
            }

            *sheet_id = sht->sheet_num;

            new_sheet->index = front_p->index + 1;
            new_sheet->bx = bx;
            new_sheet->by = by;
            new_sheet->width = x;
            new_sheet->height = y;
            new_sheet->type = type;
            new_sheet->buffer = malloc((x * y * 4));

            sht->sheet_num++;
        }
        else
        {
            sht->start = new_sheet;

            *sheet_id = 0;

            new_sheet->index = 0;
            new_sheet->bx = bx;
            new_sheet->by = by;
            new_sheet->width = x;
            new_sheet->height = y;
            new_sheet->type = type;
            new_sheet->buffer = malloc((x * y * 4));

            sht->sheet_num++;
        }
    }
    write_serial_string("Sheet Create Success. SheetID: ");
    write_serial_dec(*sheet_id);
    write_serial_string("\n");
}

UINT32 getBX(SHEET_INFO *sht, UINT16 *sheet_id)
{
    SHEET *front_p = sht->start;
    while (true)
    {
        if (front_p->index == *sheet_id)
        {
            return front_p->bx;
        }
        else if (front_p->next == NULL)
        {
            return NULL;
        }
        front_p = front_p->next;
    }
}

UINT32 getBY(SHEET_INFO *sht, UINT16 *sheet_id)
{
    SHEET *front_p = sht->start;
    while (true)
    {
        if (front_p->index == *sheet_id)
        {
            return front_p->by;
        }
        else if (front_p->next == NULL)
        {
            return NULL;
        }
        front_p = front_p->next;
    }
}

UINT32 getXsize(SHEET_INFO *sht, UINT16 *sheet_id)
{
    SHEET *front_p = sht->start;
    while (true)
    {
        if (front_p->index == *sheet_id)
        {
            return front_p->width;
        }
        else if (front_p->next == NULL)
        {
            return NULL;
        }
        front_p = front_p->next;
    }
}

UINT32 getYsize(SHEET_INFO *sht, UINT16 *sheet_id)
{
    SHEET *front_p = sht->start;
    while (true)
    {
        if (front_p->index == *sheet_id)
        {
            return front_p->height;
        }
        else if (front_p->next == NULL)
        {
            return NULL;
        }
        front_p = front_p->next;
    }
}

void setBX(SHEET_INFO *sht, UINT16 *sheet_id, UINT32 num)
{
    SHEET *front_p = sht->start;
    while (true)
    {
        if (front_p->index == *sheet_id)
        {
            front_p->bx = num;
            return;
        }
        else if (front_p->next == NULL)
        {
            return;
        }
        front_p = front_p->next;
    }
}

void setBY(SHEET_INFO *sht, UINT16 *sheet_id, UINT32 num)
{
    SHEET *front_p = sht->start;
    while (true)
    {
        if (front_p->index == *sheet_id)
        {
            front_p->by = num;
            return;
        }
        else if (front_p->next == NULL)
        {
            return;
        }
        front_p = front_p->next;
    }
}

void delete_sheet(SHEET_INFO *sht, UINT16 *sheet_id)
{
    SHEET *front_p = sht->start;
    while (true)
    {
        if (front_p->index == *sheet_id)
        {
            front_p->front->next = front_p->next;
            front_p->next->front = front_p->front;
            free(front_p->buffer);
            sht->sheet_num--;
            write_serial_string("Sheet Delete Success. SheetID: ");
            write_serial_dec(*sheet_id);
            write_serial_string("\n");
        }
        else if (front_p->next == NULL)
        {
            write_serial_string("Sheet Delete Faild. SheetID: ");
            write_serial_dec(*sheet_id);
            write_serial_string("\n");
            return;
        }
        front_p = front_p->next;
    }
}

void refresh_sheet(SHEET_INFO *sht)
{
    static char pixel_writer_buf[sizeof(RGBRWriter) > sizeof(BGRRWriter) ? sizeof(RGBRWriter) : sizeof(BGRRWriter)];
    static PixelWriter *pixel_writer = nullptr;

    switch (sht->fbc->pixel_format)
    {
    case PixelFormat::kRGBR:
        pixel_writer = new (pixel_writer_buf) RGBRWriter(*sht->fbc); // 使用 placement new
        break;
    case PixelFormat::kBGRR:
        pixel_writer = new (pixel_writer_buf) BGRRWriter(*sht->fbc);
        break;
    default:
        break;
    }

    SHEET *front_p = sht->start;

    for (int i = 0; i < sht->sheet_num; i++)
    {
        write_serial_string("Refreshing Sheet Index ");
        write_serial_dec(front_p->index);
        write_serial_string("\n");
        SHEET_BUFFER *TempBuffer = (SHEET_BUFFER *)(sht->temp_buffer);
        SHEET_BUFFER *SheetBuffer = (SHEET_BUFFER *)(front_p->buffer);
        TempBuffer += (sht->scrx * front_p->by + front_p->bx);
        for (int y = 0; y < front_p->height; y++)
        {
            for (int x = 0; x < front_p->width; x++)
            {
                // 显示颜色 = 源像素颜色 * alpha / 255 + 背景颜色 * (255 - alpha) / 255
                if (SheetBuffer->a == 255)
                {
                    TempBuffer->r = SheetBuffer->r;
                    TempBuffer->g = SheetBuffer->g;
                    TempBuffer->b = SheetBuffer->b;
                }
                else if (SheetBuffer->a == 0)
                {
                    // pass
                }
                else
                {
                    TempBuffer->r = SheetBuffer->r * SheetBuffer->a / 255 + TempBuffer->r * (255 - SheetBuffer->a) / 255;
                    TempBuffer->g = SheetBuffer->g * SheetBuffer->a / 255 + TempBuffer->g * (255 - SheetBuffer->a) / 255;
                    TempBuffer->b = SheetBuffer->b * SheetBuffer->a / 255 + TempBuffer->b * (255 - SheetBuffer->a) / 255;
                }
                TempBuffer++;
                SheetBuffer++;
            }
            TempBuffer += (sht->scrx - front_p->width);
        }
        front_p = front_p->next;
    }

    // 刷新
    SHEET_BUFFER *OutputBuffer = (SHEET_BUFFER *)(sht->temp_buffer);
    for (int y = 0; y < sht->scry; y++)
    {
        for (int x = 0; x < sht->scrx; x++)
        {
            pixel_writer->Write(x, y, {OutputBuffer->r, OutputBuffer->g, OutputBuffer->b});
            OutputBuffer++;
        }
    }
}

void refresh_part_sheet(SHEET_INFO *sht, int bex1, int bey1, int bex2, int bey2)
{
    static char pixel_writer_buf[sizeof(RGBRWriter) > sizeof(BGRRWriter) ? sizeof(RGBRWriter) : sizeof(BGRRWriter)];
    static PixelWriter *pixel_writer = nullptr;

    switch (sht->fbc->pixel_format)
    {
    case PixelFormat::kRGBR:
        pixel_writer = new (pixel_writer_buf) RGBRWriter(*sht->fbc); // 使用 placement new
        break;
    case PixelFormat::kBGRR:
        pixel_writer = new (pixel_writer_buf) BGRRWriter(*sht->fbc);
        break;
    default:
        break;
    }

    if (bex1 < 0)
    {
        bex1 = 0;
    }
    if (bey1 < 0)
    {
        bey1 = 0;
    }
    if (bex2 > sht->scrx)
    {
        bex2 = sht->scrx;
    }
    if (bey2 > sht->scry)
    {
        bey2 = sht->scry;
    }

    SHEET *front_p = sht->start;
    SHEET_BUFFER *TempBuffer = (SHEET_BUFFER *)(sht->temp_buffer);

    int vx, vy;
    for (int i = 0; i < sht->sheet_num; i++)
    {
        SHEET_BUFFER *TempBuffer = (SHEET_BUFFER *)(sht->temp_buffer);
        SHEET_BUFFER *SheetBuffer = (SHEET_BUFFER *)(front_p->buffer);
        TempBuffer += (sht->scrx * front_p->by + front_p->bx);
        for (int y = 0; y < front_p->height; y++)
        {
            vy = front_p->by + y;
            for (int x = 0; x < front_p->width; x++)
            {
                vx = front_p->bx + x;
                if (bex1 <= vx && vx < bex2 && bey1 <= vy && vy < bey2)
                {
                    // 显示颜色 = 源像素颜色 * alpha / 255 + 背景颜色 * (255 - alpha) / 255
                    if (SheetBuffer->a == 255)
                    {
                        TempBuffer->r = SheetBuffer->r;
                        TempBuffer->g = SheetBuffer->g;
                        TempBuffer->b = SheetBuffer->b;
                    }
                    else if (SheetBuffer->a == 0)
                    {
                        // pass
                    }
                    else
                    {
                        TempBuffer->r = SheetBuffer->r * SheetBuffer->a / 255 + TempBuffer->r * (255 - SheetBuffer->a) / 255;
                        TempBuffer->g = SheetBuffer->g * SheetBuffer->a / 255 + TempBuffer->g * (255 - SheetBuffer->a) / 255;
                        TempBuffer->b = SheetBuffer->b * SheetBuffer->a / 255 + TempBuffer->b * (255 - SheetBuffer->a) / 255;
                    }
                }
                TempBuffer++;
                SheetBuffer++;
            }
            TempBuffer += (sht->scrx - front_p->width);
        }
        front_p = front_p->next;
    }

    // 刷新
    SHEET_BUFFER *OutputBuffer = (SHEET_BUFFER *)(sht->temp_buffer);
    OutputBuffer += (sht->scrx * bey1 + bex1);
    for (int y = bey1; y < bey2; y++)
    {
        for (int x = bex1; x < bex2; x++)
        {
            pixel_writer->Write(x, y, {OutputBuffer->r, OutputBuffer->g, OutputBuffer->b});
            OutputBuffer++;
        }
        OutputBuffer += (sht->scrx - bex2 + bex1);
    }
}

UINT32 get_hor()
{
    return fbc_addr->horizontal_resolution;
}

UINT32 get_ver()
{
    return fbc_addr->vertical_resolution;
}
