#include "../include/include.h"

void init_sheet(const FrameBufferConfig &fbc, SHEET_INFO *sht)
{
    write_serial_string("Initializing Sheet Manager...\n");
    sht->fbc = &fbc;
    sht->scrx = fbc.horizontal_resolution;
    sht->scry = fbc.vertical_resolution;
    sht->start = NULL;
    sht->temp_buffer = malloc((sht->scrx * sht->scry * 4) + 4); // 多分配4字节防止溢出啥的
    if (sht->temp_buffer == NULL)
    {
        // 没分配成，去世。
        // WSOD_Cannot_Alloc_Memory(fbc);  // 死了啦，都是你害的啦
        while (1)
        {
            __asm__ __volatile__("hlt");
        }
    }
    sht->sheet_num = -1;
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
    xmemset(new_sheet, 0, sizeof(SHEET));

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
                new_sheet->is_change = true;
                xmemset(new_sheet->buffer, 0, x * y * 4);

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
                new_sheet->is_change = true;
                xmemset(new_sheet->buffer, 0, x * y * 4);

                sht->sheet_num++;
            }
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
            new_sheet->is_change = true;
            xmemset(new_sheet->buffer, 0, x * y * 4);

            sht->sheet_num++;
        }
    }
    else
    {
        // 随便分配
        // 直接分配下一个
        if (front_p != NULL)
        {
            for (int i = 0; i < sht->sheet_num - 2; i++)
            {
                front_p = front_p->next;
            }

            if (front_p->next == NULL)
            {
                front_p->next = new_sheet;
                new_sheet->front = front_p;
            }
            else
            {
                new_sheet->next = front_p->next;
                front_p->next->front = new_sheet;
                front_p->next = new_sheet;
                new_sheet->front = front_p;
            }

            *sheet_id = sht->sheet_num - 1;

            new_sheet->index = front_p->index + 1;
            new_sheet->bx = bx;
            new_sheet->by = by;
            new_sheet->width = x;
            new_sheet->height = y;
            new_sheet->type = type;
            new_sheet->buffer = malloc((x * y * 4));
            new_sheet->is_change = true;
            xmemset(new_sheet->buffer, 0, x * y * 4);

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
            new_sheet->is_change = true;
            xmemset(new_sheet->buffer, 0, x * y * 4);

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

UINT16 *found_sheet(SHEET_INFO *sht, UINT32 x, UINT32 y)
{
    SHEET *front_p = sht->start;
    while (true)
    {
        if (front_p->next == NULL)
        {
            while (true)
            {
                if (front_p->bx <= x && front_p->bx + front_p->width >= x &&
                    front_p->by <= y && front_p->by + front_p->height >= y)
                {
                    return &front_p->index;
                }
                else if (front_p->front == NULL)
                {
                    return NULL;
                }
                front_p = front_p->front;
            }
            return NULL;
        }
        front_p = front_p->next;
    }
}

UINT16 *found_sheetmb(SHEET_INFO *sht, UINT32 x, UINT32 y)
{
    SHEET *front_p = sht->start;
    while (true)
    {
        if (front_p->next == NULL)
        {
            while (true)
            {
                if (front_p->bx <= x && front_p->bx + front_p->width >= x &&
                    front_p->by <= y && front_p->by + front_p->height >= y &&
                    front_p->type == MovableSheetType)
                {
                    return &front_p->index;
                }
                else if (front_p->front == NULL)
                {
                    return NULL;
                }
                front_p = front_p->front;
            }
            return NULL;
        }
        front_p = front_p->next;
    }
}

UINT16 *found_sheet_movable(SHEET_INFO *sht, UINT32 x, UINT32 y)
{
    SHEET *front_p = sht->start;
    while (true)
    {
        if (front_p->next == NULL)
        {
            while (true)
            {
                if (front_p->bx + 22 <= x && front_p->bx + front_p->width - 32 >= x &&
                    front_p->by <= y && front_p->by + 22 >= y &&
                    front_p->type == MovableSheetType)
                {
                    return &front_p->index;
                }
                else if (front_p->front == NULL)
                {
                    return NULL;
                }
                front_p = front_p->front;
            }
            return NULL;
        }
        front_p = front_p->next;
    }
}

SHEET *get_sheet(SHEET_INFO *sht, UINT16 *sheet_id)
{
    SHEET *front_p = sht->start;
    while (true)
    {
        if (front_p->index == *sheet_id)
        {
            return front_p;
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

void lift_sheet(SHEET_INFO *sht, UINT16 *sheet_id)
{
    SHEET *front_p = sht->start;
    while (true)
    {
        if (front_p->index == *sheet_id)
        {
            front_p->next->front = front_p->front;
            front_p->front->next = front_p->next;
            SHEET *front_p2 = sht->start;
            for (int i = 0; i < sht->sheet_num - 3; i++)
            {
                front_p2 = front_p2->next; // 寻找最后一个
            }

            front_p->next = front_p2->next;
            front_p->next->front = front_p;
            front_p2->next = front_p;
            front_p->front = front_p2;
            return;
        }
        else if (front_p->next == NULL)
        {
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
    SHEET_BUFFER *TempBuffer = (SHEET_BUFFER *)(sht->temp_buffer);

    for (int i = 0; i < sht->sheet_num + 1; i++)
    {
        write_serial_string("Refreshing Sheet Index ");
        write_serial_dec(front_p->index);
        write_serial_string("\n");
        SHEET_BUFFER *SheetBuffer = (SHEET_BUFFER *)(front_p->buffer);
        UINT32 BufferOffest = sht->scrx * front_p->by + front_p->bx; // 起始位置
        for (int y = 0; y < front_p->height; y++)
        {
            for (int x = 0; x < front_p->width; x++)
            {
                // 起始位置 = sht->scrx * front_p->by + front_p->bx
                // 偏移量 = 起始位置 + y * sht->scrx + x
                // 即TempBuffer[起始位置 + y * sht->scrx + x] = SheetBuffer[y * front_p->width + x]
                // 显示颜色 = 源像素颜色 * alpha / 255 + 背景颜色 * (255 - alpha) / 255
                if (SheetBuffer[y * front_p->width + x].a == 255)
                {
                    TempBuffer[BufferOffest].r = SheetBuffer[y * front_p->width + x].r;
                    TempBuffer[BufferOffest].g = SheetBuffer[y * front_p->width + x].g;
                    TempBuffer[BufferOffest].b = SheetBuffer[y * front_p->width + x].b;
                }
                else if (SheetBuffer[y * front_p->width + x].a == 0)
                {
                    // pass
                }
                else
                {
                    TempBuffer[BufferOffest].r =
                        SheetBuffer[y * front_p->width + x].r * SheetBuffer[y * front_p->width + x].a / 255 + TempBuffer[BufferOffest].r * (255 - SheetBuffer[y * front_p->width + x].a) / 255;
                    TempBuffer[BufferOffest].g =
                        SheetBuffer[y * front_p->width + x].g * SheetBuffer[y * front_p->width + x].a / 255 + TempBuffer[BufferOffest].g * (255 - SheetBuffer[y * front_p->width + x].a) / 255;
                    TempBuffer[BufferOffest].b =
                        SheetBuffer[y * front_p->width + x].b * SheetBuffer[y * front_p->width + x].a / 255 + TempBuffer[BufferOffest].b * (255 - SheetBuffer[y * front_p->width + x].a) / 255;
                }
                BufferOffest++;
            }
            BufferOffest += (sht->scrx - front_p->width);
        }
        front_p = front_p->next;
    }

    // 刷新
    for (int y = 0; y < sht->scry; y++)
    {
        for (int x = 0; x < sht->scrx; x++)
        {
            pixel_writer->Write(x, y, {TempBuffer->r, TempBuffer->g, TempBuffer->b});
            TempBuffer++;
        }
    }
    write_serial_string("Refresh Success.\n");
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

    for (int i = 0; i < sht->sheet_num + 1; i++)
    {
        if (front_p->bx > bex2 || front_p->by > bey2 /*||
            front_p->bx + front_p->width < bex1 || front_p->by + front_p->height < bey1*/)  // 不知道为啥这段注释掉不仅bug没了，速度还变快了
        {
            // 不在刷新范围内，跳过
        }
        else
        {
            SHEET_BUFFER *SheetBuffer = (SHEET_BUFFER *)(front_p->buffer);
            int offest_x = bex1 - front_p->bx; // 刷新区域相较于图层的偏移量
            int offest_y = bey1 - front_p->by; // 同上
            if (offest_x < 0)
            {
                offest_x = 0;
            }
            if (offest_y < 0)
            {
                offest_y = 0;
            }
            int dlo_x = (front_p->bx + front_p->width) - bex2;  // 图层末尾相较于刷新区域末尾的偏移量
            int dlo_y = (front_p->by + front_p->height) - bey2; // 同上
            if (dlo_x < 0 || dlo_x > front_p->width)
            {
                dlo_x = 0;
            }
            if (dlo_y < 0 || dlo_y > front_p->height)
            {
                dlo_y = 0;
            }
            UINT32 BufferOffest = sht->scrx * (front_p->by + offest_y) + front_p->bx + offest_x; // 起始位置
            for (int y = offest_y; y < front_p->height - dlo_y; y++)
            {
                for (int x = offest_x; x < front_p->width - dlo_x; x++)
                {
                    // 显示颜色 = 源像素颜色 * alpha / 255 + 背景颜色 * (255 - alpha) / 255
                    if (SheetBuffer[y * front_p->width + x].a == 255)
                    {
                        TempBuffer[BufferOffest].r = SheetBuffer[y * front_p->width + x].r;
                        TempBuffer[BufferOffest].g = SheetBuffer[y * front_p->width + x].g;
                        TempBuffer[BufferOffest].b = SheetBuffer[y * front_p->width + x].b;
                    }
                    else if (SheetBuffer[y * front_p->width + x].a == 0)
                    {
                        // pass
                    }
                    else
                    {
                        TempBuffer[BufferOffest].r =
                            SheetBuffer[y * front_p->width + x].r * SheetBuffer[y * front_p->width + x].a / 255 + TempBuffer[BufferOffest].r * (255 - SheetBuffer[y * front_p->width + x].a) / 255;
                        TempBuffer[BufferOffest].g =
                            SheetBuffer[y * front_p->width + x].g * SheetBuffer[y * front_p->width + x].a / 255 + TempBuffer[BufferOffest].g * (255 - SheetBuffer[y * front_p->width + x].a) / 255;
                        TempBuffer[BufferOffest].b =
                            SheetBuffer[y * front_p->width + x].b * SheetBuffer[y * front_p->width + x].a / 255 + TempBuffer[BufferOffest].b * (255 - SheetBuffer[y * front_p->width + x].a) / 255;
                    }
                    BufferOffest++;
                }
                BufferOffest += sht->scrx - front_p->width + offest_x + dlo_x;
            }
        }
        front_p = front_p->next;
        if (front_p == nullptr)
        {
            break;
        }
    }

    // 刷新
    TempBuffer += (sht->scrx * bey1 + bex1);
    for (int y = bey1; y < bey2; y++)
    {
        for (int x = bex1; x < bex2; x++)
        {
            pixel_writer->Write(x, y, {TempBuffer->r, TempBuffer->g, TempBuffer->b});
            TempBuffer++;
        }
        TempBuffer += (sht->scrx - bex2 + bex1);
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
