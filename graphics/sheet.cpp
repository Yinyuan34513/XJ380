#include <graphics/GOP.hpp>
#include <graphics/sheet.h>
#include <proto.hpp>
bool SheetIsRefreshing = false;
bool SheetIsCreating   = false;

void init_foxland(SHEET_INFO *sht)
{
    sht->foxland.fl_peek_buffer = (SHEET_BUFFER *)malloc(sht->scrx * sht->scry * 4);
    sht->foxland.peek_sign      = (uint8_t *)malloc(sht->scrx * sht->scry * sizeof(uint8_t));
    LoadPicture((SHEET_BUFFER *)sht->foxland.fl_peek_buffer, sht->scrx, sht->scry, "/system/foxland/peek.png");
}

void init_sheet(const FrameBufferConfig &fbc, SHEET_INFO *sht)
{
    write_serial_string("Initializing Sheet Manager...\n");
    sht->fbc         = &fbc;
    sht->scrx        = fbc.horizontal_resolution;
    sht->scry        = fbc.vertical_resolution;
    sht->start       = NULL;
    sht->temp_buffer = malloc((sht->scrx * sht->scry * 4) + 4); // 多分配4字节防止溢出啥的
    if (sht->temp_buffer == NULL)
    {
        // 没分配成，去世。
        // WSOD_Cannot_Alloc_Memory(fbc);  // 死了啦，都是你害的啦
        write_serial_string("Cannot allocate memory for Sheet Manager. System halted.\n");
        while (1)
        {
            __asm__ __volatile__("hlt");
        }
    }
    sht->sheet_num            = -1;
    sht->sht_numed            = -1;
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

    init_foxland(sht);

    write_serial_string("Sheet Manager Initialize Success.\n");
}

void create_sheet(SHEET_INFO *sht, uint32_t bx, uint32_t by, uint32_t x, uint32_t y, uint32_t type, int16_t index,
                  uint16_t *sheet_id)
{
    while (true)
    {
        if (!SheetIsRefreshing && !SheetIsCreating) { break; }
    }

    SheetIsCreating  = true;
    SHEET *new_sheet = (SHEET *)(malloc(sizeof(SHEET))); // 开辟内存空间
    memset(new_sheet, 0, sizeof(SHEET));

    SHEET *front_p = sht->start;
    if (index > 0)
    {
        // 指定图层高度
        if (front_p != NULL)
        {
            SHEET *record = front_p;
            while (true)
            {
                if (front_p->next == NULL) { break; }
                else if (front_p->next->index >= index) { break; }
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
                new_sheet->next      = front_p->next;   // 把自己的下一个标记为曾经的
                new_sheet->front     = front_p;         // 把自己的上一个标记为上一个
                front_p->next->front = new_sheet;       // 把曾经的上一个标记为自己
                front_p->next        = new_sheet;       // 把自己的上一个的下一个标记为自己
                front_p              = new_sheet->next; // 指针指向自己的下一个（曾经的）
                record               = front_p;
                // 给后面的全++
                while (true)
                {
                    front_p->index++;
                    if (front_p->next == NULL) { break; }
                    else if (record == front_p->next || front_p == front_p->next)
                    {
                        front_p->next = NULL;
                        break;
                    }
                    front_p = front_p->next;
                }

                // 分配
                *sheet_id = index;

                new_sheet->index     = index;
                new_sheet->bx        = bx;
                new_sheet->by        = by;
                new_sheet->width     = x;
                new_sheet->height    = y;
                new_sheet->type      = type;
                new_sheet->buffer    = malloc((x * y * 4));
                new_sheet->is_change = true;
                memset(new_sheet->buffer, 0, x * y * 4);

                sht->sheet_num++;
                sht->sht_numed++;
            }
            else
            {
                // 哦亲爱的孩子你是第一个分配的
                new_sheet->front = front_p;
                front_p->next    = new_sheet;

                // 分配
                *sheet_id = index;

                new_sheet->index     = index;
                new_sheet->bx        = bx;
                new_sheet->by        = by;
                new_sheet->width     = x;
                new_sheet->height    = y;
                new_sheet->type      = type;
                new_sheet->buffer    = malloc((x * y * 4));
                new_sheet->is_change = true;
                memset(new_sheet->buffer, 0, x * y * 4);

                sht->sheet_num++;
                sht->sht_numed++;
            }
        }
        else
        {
            sht->start = new_sheet;

            *sheet_id = 0;

            new_sheet->index     = 0;
            new_sheet->bx        = bx;
            new_sheet->by        = by;
            new_sheet->width     = x;
            new_sheet->height    = y;
            new_sheet->type      = type;
            new_sheet->buffer    = malloc((x * y * 4));
            new_sheet->is_change = true;
            memset(new_sheet->buffer, 0, x * y * 4);

            sht->sheet_num++;
            sht->sht_numed++;
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
                front_p->next    = new_sheet;
                new_sheet->front = front_p;
            }
            else
            {
                new_sheet->next      = front_p->next;
                front_p->next->front = new_sheet;
                front_p->next        = new_sheet;
                new_sheet->front     = front_p;
            }

            *sheet_id = sht->sht_numed - 1;

            new_sheet->index     = front_p->index + 1;
            new_sheet->bx        = bx;
            new_sheet->by        = by;
            new_sheet->width     = x;
            new_sheet->height    = y;
            new_sheet->type      = type;
            new_sheet->buffer    = malloc((x * y * 4));
            new_sheet->is_change = true;
            memset(new_sheet->buffer, 0, x * y * 4);

            sht->sheet_num++;
            sht->sht_numed++;
        }
        else
        {
            sht->start = new_sheet;

            *sheet_id = 0;

            new_sheet->index     = 0;
            new_sheet->bx        = bx;
            new_sheet->by        = by;
            new_sheet->width     = x;
            new_sheet->height    = y;
            new_sheet->type      = type;
            new_sheet->buffer    = malloc((x * y * 4));
            new_sheet->is_change = true;
            memset(new_sheet->buffer, 0, x * y * 4);

            sht->sheet_num++;
            sht->sht_numed++;
        }
    }
    // write_serial_fmt("Sheet Create Success. SheetID: %d\n", *sheet_id);
    SheetIsCreating = false;
}

uint32_t getBX(SHEET_INFO *sht, uint16_t *sheet_id)
{
    SHEET *front_p = sht->start;
    while (true)
    {
        if (front_p->index == *sheet_id) { return front_p->bx; }
        else if (front_p->next == NULL) { return NULL; }
        front_p = front_p->next;
    }
}

uint32_t getBY(SHEET_INFO *sht, uint16_t *sheet_id)
{
    SHEET *front_p = sht->start;
    while (true)
    {
        if (front_p->index == *sheet_id) { return front_p->by; }
        else if (front_p->next == NULL) { return NULL; }
        front_p = front_p->next;
    }
}

uint32_t getXsize(SHEET_INFO *sht, uint16_t *sheet_id)
{
    SHEET *front_p = sht->start;
    while (true)
    {
        if (front_p->index == *sheet_id) { return front_p->width; }
        else if (front_p->next == NULL) { return NULL; }
        front_p = front_p->next;
    }
}

uint32_t getYsize(SHEET_INFO *sht, uint16_t *sheet_id)
{
    SHEET *front_p = sht->start;
    while (true)
    {
        if (front_p->index == *sheet_id) { return front_p->height; }
        else if (front_p->next == NULL) { return NULL; }
        front_p = front_p->next;
    }
}

uint16_t *found_sheet(SHEET_INFO *sht, uint32_t x, uint32_t y)
{
    SHEET *front_p = sht->start;
    while (true)
    {
        if (front_p->next == NULL)
        {
            while (true)
            {
                if (front_p->bx <= x && front_p->bx + front_p->width >= x && front_p->by <= y &&
                    front_p->by + front_p->height >= y)
                {
                    return &front_p->index;
                }
                else if (front_p->front == NULL) { return NULL; }
                front_p = front_p->front;
            }
            return NULL;
        }
        front_p = front_p->next;
    }
}

uint16_t *found_sheetmb(SHEET_INFO *sht, uint32_t x, uint32_t y)
{
    SHEET *front_p = sht->start;
    while (true)
    {
        if (front_p->next == NULL)
        {
            while (true)
            {
                if (front_p->bx <= x && front_p->bx + front_p->width >= x && front_p->by <= y &&
                    front_p->by + front_p->height >= y &&
                    (front_p->type == MovableSheetType || front_p->type == NoEdgeWindowSheetType ||
                     front_p->type == TopWindowSheetType))
                {
                    return &front_p->index;
                }
                else if (front_p->front == NULL) { return NULL; }
                front_p = front_p->front;
            }
            return NULL;
        }
        front_p = front_p->next;
    }
}

uint16_t *found_sheet_movable(SHEET_INFO *sht, uint32_t x, uint32_t y)
{
    SHEET *front_p = sht->start;
    while (true)
    {
        if (front_p->next == NULL)
        {
            front_p = front_p->front;
            front_p = front_p->front;
            while (true)
            {
                if (front_p->type == MovableSheetType)
                {
                    if (front_p->bx + 22 <= x && front_p->bx + front_p->width - 32 >= x && front_p->by <= y &&
                        front_p->by + 22 >= y)
                    {
                        return &front_p->index;
                    }
                    else
                    {
                        return NULL;
                    }
                }
                else if (front_p->type == NoEdgeWindowSheetType)
                {
                    if (front_p->bx <= x && front_p->bx + front_p->width >= x && front_p->by <= y &&
                        front_p->by + front_p->height >= y)
                    {
                        return &front_p->index;
                    }
                    else
                    {
                        return NULL;
                    }
                }
                else if (front_p->front == NULL) { return NULL; }
                front_p = front_p->front;
            }
            return NULL;
        }
        front_p = front_p->next;
    }
}

void change_sheet_type(SHEET_INFO *sht, uint16_t *sheet_id, uint32_t new_type)
{
    SHEET *front_p = sht->start;
    while (true)
    {
        if (front_p->index == *sheet_id)
        {
            front_p->type = new_type;
            return;
        }
        else if (front_p->next == NULL) { return; }
        front_p = front_p->next;
    }
}

SHEET *get_sheet(SHEET_INFO *sht, uint16_t *sheet_id)
{
    SHEET *front_p = sht->start;
    while (true)
    {
        if (front_p->index == *sheet_id) { return front_p; }
        else if (front_p->next == NULL) { return NULL; }
        front_p = front_p->next;
    }
}

void setBX(SHEET_INFO *sht, uint16_t *sheet_id, uint32_t num)
{
    SHEET *front_p = sht->start;
    while (true)
    {
        if (front_p->index == *sheet_id)
        {
            front_p->bx = num;
            return;
        }
        else if (front_p->next == NULL) { return; }
        front_p = front_p->next;
    }
}

void setBY(SHEET_INFO *sht, uint16_t *sheet_id, uint32_t num)
{
    SHEET *front_p = sht->start;
    while (true)
    {
        if (front_p->index == *sheet_id)
        {
            front_p->by = num;
            return;
        }
        else if (front_p->next == NULL) { return; }
        front_p = front_p->next;
    }
}

void delete_sheet(SHEET_INFO *sht, uint16_t *sheet_id)
{
    while (true)
    {
        if (!SheetIsCreating && !SheetIsRefreshing) { break; }
    }

    SheetIsCreating = true;
    SHEET *front_p  = sht->start;
    while (true)
    {
        if (front_p->index == *sheet_id)
        {
            front_p->front->next = front_p->next;
            front_p->next->front = front_p->front;
            free(front_p->buffer);
            memset(front_p, 0, sizeof(SHEET));
            free((void *)(front_p));
            // sht->sheet_num--;
            write_serial_string("Sheet Delete Success. SheetID: ");
            write_serial_dec(*sheet_id);
            write_serial_string("\n");
            break;
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

    SheetIsCreating = false;
}

void lift_sheet(SHEET_INFO *sht, uint16_t *sheet_id)
{
    SHEET *front_p = sht->start;
    while (true)
    {
        if (front_p->index == *sheet_id)
        {
            front_p->next->front = front_p->front;
            front_p->front->next = front_p->next;
            SHEET *front_p2      = sht->start;
            for (int i = 0; i < sht->sheet_num - 3; i++)
            {
                front_p2 = front_p2->next; // 寻找最后一个
            }

            front_p->next        = front_p2->next;
            front_p->next->front = front_p;
            front_p2->next       = front_p;
            front_p->front       = front_p2;
            return;
        }
        else if (front_p->next == NULL) { return; }
        front_p = front_p->next;
    }
}

bool allow_to_flush = false;

void refresh_part_sheet(SHEET_INFO *sht, int bex1, int bey1, int bex2, int bey2) {
    if (!allow_to_flush) return;
    SheetIsRefreshing = true;

    // 屏幕边界裁剪
    if (bex1 < 0) bex1 = 0;
    if (bey1 < 0) bey1 = 0;
    if (bex2 > (int)sht->scrx) bex2 = sht->scrx;
    if (bey2 > (int)sht->scry) bey2 = sht->scry;
    if (bex1 >= bex2 || bey1 >= bey2) {
        SheetIsRefreshing = false;
        return;
    }

    SHEET_BUFFER *temp_buf_base = (SHEET_BUFFER *)sht->temp_buffer;
    uint8_t *temp_peek_sign = sht->foxland.peek_sign;

    // 遍历所有图层进行合成
    for (SHEET *st = sht->start; st != nullptr; st = st->next) {
        // 计算当前图层与刷新区域的交集
        int vx0 = (bex1 > (int)st->bx) ? bex1 : (int)st->bx;
        int vy0 = (bey1 > (int)st->by) ? bey1 : (int)st->by;
        int vx1 = (bex2 < (int)(st->bx + st->width)) ? bex2 : (int)(st->bx + st->width);
        int vy1 = (bey2 < (int)(st->by + st->height)) ? bey2 : (int)(st->by + st->height);

        // 不在当前刷新区域内直接跳过
        if (vx0 >= vx1 || vy0 >= vy1) continue;

        SHEET_BUFFER *st_buf = (SHEET_BUFFER *)st->buffer;
        
        for (int y = vy0; y < vy1; y++) {
            int screen_off = y * sht->scrx + vx0;
            int st_off = (y - st->by) * st->width + (vx0 - st->bx);
            
            SHEET_BUFFER *p_dst = &temp_buf_base[screen_off];
            SHEET_BUFFER *p_src = &st_buf[st_off];
            uint8_t *p_sign = &temp_peek_sign[screen_off];

            for (int x = vx0; x < vx1; x++) {
                if (p_src->a == 255) {
                    // 全透明直接覆盖，关闭Peek标志
                    *p_dst = *p_src;
                    *p_sign = 0;
                } else if (p_src->a > 0) {
                    // 快速整数Alpha混合：(src * a + dst * (255 - a)) >> 8
                    uint32_t alpha = p_src->a;
                    uint32_t inv_alpha = 255 - alpha;
                    
                    p_dst->r = (p_src->r * alpha + p_dst->r * inv_alpha) >> 8;
                    p_dst->g = (p_src->g * alpha + p_dst->g * inv_alpha) >> 8;
                    p_dst->b = (p_src->b * alpha + p_dst->b * inv_alpha) >> 8;
                    
                    // 有透明度就标记需要Peek处理
                    *p_sign = 1;
                }
                p_dst++;
                p_src++;
                p_sign++;
            }
        }
    }

    // Foxland Peek处理
    SHEET_BUFFER *temp_peek = (SHEET_BUFFER *)sht->foxland.fl_peek_buffer;
    for (int y = bey1; y < bey2; y++) {
        int offset = y * sht->scrx + bex1;
        SHEET_BUFFER *p_dst = &temp_buf_base[offset];
        uint8_t *p_sign = &temp_peek_sign[offset];
        SHEET_BUFFER *p_peek = &temp_peek[offset];

        for (int x = bex1; x < bex2; x++) {
            if (*p_sign == 1) {
                uint32_t alpha = p_peek->a;
                uint32_t inv_alpha = 255 - alpha;
                p_dst->r = (p_peek->r * alpha + p_dst->r * inv_alpha) >> 8;
                p_dst->g = (p_peek->g * alpha + p_dst->g * inv_alpha) >> 8;
                p_dst->b = (p_peek->b * alpha + p_dst->b * inv_alpha) >> 8;
                *p_sign = 0; // 重置标志位
            }
            p_dst++;
            p_sign++;
            p_peek++;
        }
    }

    // 写入显存
    bool isRGB = (sht->fbc->pixel_format == PixelFormat::kRGBR);
    for (int y = bey1; y < bey2; y++) {
        SHEET_BUFFER *line_ptr = &temp_buf_base[y * sht->scrx + bex1];
        for (int x = bex1; x < bex2; x++) {
            if (isRGB) {
                WriteRGBR(x, y, {line_ptr->r, line_ptr->g, line_ptr->b}, *sht->fbc);
            } else {
                WriteBGRR(x, y, {line_ptr->r, line_ptr->g, line_ptr->b}, *sht->fbc);
            }
            line_ptr++;
        }
    }

    SheetIsRefreshing = false;
}

void refresh_sheet(SHEET_INFO *sht)
{
    // 已摆烂
    refresh_part_sheet(sht, 0, 0, sht->scrx, sht->scry);
}

uint32_t get_hor()
{
    return fbc_addr->horizontal_resolution;
}

uint32_t get_ver()
{
    return fbc_addr->vertical_resolution;
}

void copy_buffer_by_id(SHEET_INFO *sht, uint16_t *dst_sheet_id, SHEET_BUFFER *src, uint32_t dst_x, uint32_t dst_y,
                       uint32_t width, uint32_t height)
{
    SHEET *front_p = sht->start;
    while (true)
    {
        if (front_p->index == *dst_sheet_id)
        {
            // 找到啦！
            SHEET_BUFFER *dst = (SHEET_BUFFER *)front_p->buffer;
            for (int y = 0; y < height; y++)
            {
                for (int x = 0; x < width; x++)
                {
                    dst[(dst_y + y) * front_p->width + (dst_x + x)] = src[y * width + x];
                }
            }
        }
        else if (front_p->next == NULL) { return; }
        front_p = front_p->next;
    }
}

void copy_buffer_by_id_without_alpha(SHEET_INFO *sht, uint16_t *dst_sheet_id, SHEET_BUFFER *src, uint32_t dst_x,
                                     uint32_t dst_y, uint32_t width, uint32_t height)
{
    SHEET *front_p = sht->start;
    while (true)
    {
        if (front_p->index == *dst_sheet_id)
        {
            // 找到啦！
            SHEET_BUFFER *dst = (SHEET_BUFFER *)front_p->buffer;
            for (int y = 0; y < height; y++)
            {
                for (int x = 0; x < width; x++)
                {
                    if (src[y * width + x].a > 10) dst[(dst_y + y) * front_p->width + (dst_x + x)] = src[y * width + x];
                }
            }
        }
        else if (front_p->next == NULL) { return; }
        front_p = front_p->next;
    }
}

SHEET_BUFFER LCD_AlphaBlend(SHEET_BUFFER foreground_color, SHEET_BUFFER background_color, uint8_t alpha);

void copy_buffer_blend_by_id(SHEET_INFO *sht, uint16_t *dst_sheet_id, SHEET_BUFFER *src, uint32_t dst_x, uint32_t dst_y,
                             uint32_t width, uint32_t height)
{
    SHEET *front_p = sht->start;
    while (true)
    {
        if (front_p->index == *dst_sheet_id)
        {
            // 找到啦！
            SHEET_BUFFER *dst = (SHEET_BUFFER *)front_p->buffer;
            for (int y = 0; y < height; y++)
            {
                for (int x = 0; x < width; x++)
                {
                    dst[(dst_y + y) * front_p->width + (dst_x + x)] = LCD_AlphaBlend(
                        src[y * width + x], dst[(dst_y + y) * front_p->width + (dst_x + x)], src[y * width + x].a);
                }
            }
        }
        else if (front_p->next == NULL) { return; }
        front_p = front_p->next;
    }
}

void *get_sheet_buffer(SHEET_INFO *sht, uint16_t *sheet_id)
{
    SHEET *front_p = sht->start;
    while (true)
    {
        if (front_p->index == *sheet_id) { return front_p->buffer; }
        else if (front_p->next == NULL) { return NULL; }
        front_p = front_p->next;
    }
}

SHEET *found_sheet_byid(SHEET_INFO *sht, uint16_t *sheet_id)
{
    SHEET *front_p = sht->start;
    while (true)
    {
        if (front_p->index == *sheet_id) { return front_p; }
        else if (front_p->next == NULL) { return NULL; }
        front_p = front_p->next;
    }
}
