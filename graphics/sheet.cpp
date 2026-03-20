#include <graphics/GOP.hpp>
#include <graphics/sheet.h>
#include <proto.hpp>
bool SheetIsRefreshing = false;
bool SheetIsCreating   = false;

static size_t sheet_buffer_bytes(uint32_t width, uint32_t height)
{
    return (size_t)width * (size_t)height * sizeof(SHEET_BUFFER);
}

static void *alloc_shared_sheet_buffer(size_t bytes)
{
    // Sheet buffers are shared by the desktop flusher and syscall paths across
    // different CR3 contexts, so they must live in the shared kernel heap.
    void *buffer = calloc(1, bytes);
    if (buffer != NULL) return buffer;

    write_serial_string("Cannot allocate memory for shared sheet buffer. System halted.\n");
    while (1)
    {
        __asm__ __volatile__("hlt");
    }
}

static bool is_valid_sheet_buffer(const void *buffer)
{
    return buffer != NULL && (uint64_t)buffer >= get_physical_memory_offset();
}

static bool clip_sheet_copy_region(const SHEET *dst_sheet, int *dst_x, int *dst_y,
                                   int *src_x, int *src_y, int *width, int *height)
{
    if (!dst_sheet || !is_valid_sheet_buffer(dst_sheet->buffer)) return false;

    int dx = *dst_x;
    int dy = *dst_y;
    int sx = *src_x;
    int sy = *src_y;
    int w  = *width;
    int h  = *height;

    if (w <= 0 || h <= 0) return false;

    if (dx < 0)
    {
        sx += -dx;
        w += dx;
        dx = 0;
    }
    if (dy < 0)
    {
        sy += -dy;
        h += dy;
        dy = 0;
    }

    if (dx >= (int)dst_sheet->width || dy >= (int)dst_sheet->height) return false;

    if (dx + w > (int)dst_sheet->width) w = (int)dst_sheet->width - dx;
    if (dy + h > (int)dst_sheet->height) h = (int)dst_sheet->height - dy;

    if (sx < 0 || sy < 0 || w <= 0 || h <= 0) return false;

    *dst_x  = dx;
    *dst_y  = dy;
    *src_x  = sx;
    *src_y  = sy;
    *width  = w;
    *height = h;
    return true;
}

void init_foxland(SHEET_INFO *sht)
{
    sht->foxland.fl_peek_buffer = (SHEET_BUFFER *)malloc(sht->scrx * sht->scry * 4);//不要乱用pagealloc
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
    size_t temp_buffer_bytes = sheet_buffer_bytes(sht->scrx, sht->scry);
    sht->temp_buffer = alloc_shared_sheet_buffer(temp_buffer_bytes);
    sht->sheet_num            = -1;

    init_foxland(sht);

    write_serial_string("Sheet Manager Initialize Success.\n");
}

void create_sheet(SHEET_INFO *sht, uint32_t bx, uint32_t by, uint32_t x, uint32_t y, uint32_t type, int16_t index,
                  SHEET **sheet)
{
    while (true)
    {
        if (!SheetIsRefreshing && !SheetIsCreating) { break; }
    }

    SheetIsCreating  = true;
    SHEET *new_sheet = (SHEET *)(malloc(sizeof(SHEET))); // 开辟内存空间
    memset(new_sheet, 0, sizeof(SHEET));
    size_t buffer_bytes = sheet_buffer_bytes(x, y);

    SHEET *front_p = sht->start;
    if (index > 0)
    {
        // 指定图层高度
        if (front_p != NULL)
        {
            SHEET *record = front_p;
            int count = -1;
            while (true)
            {
                count++;
                if (front_p->next == NULL) { break; }
                else if (count + 1 >= index) { break; }
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
                // // 给后面的全++
                // while (true)
                // {
                //     front_p->index++;
                //     if (front_p->next == NULL) { break; }
                //     else if (record == front_p->next || front_p == front_p->next)
                //     {
                //         front_p->next = NULL;
                //         break;
                //     }
                //     front_p = front_p->next;
                // }

                // 分配
                *sheet = new_sheet;

                new_sheet->bx        = bx;
                new_sheet->by        = by;
                new_sheet->width     = x;
                new_sheet->height    = y;
                new_sheet->type      = type;
                new_sheet->buffer    = alloc_shared_sheet_buffer(buffer_bytes);
                new_sheet->is_change = true;

                sht->sheet_num++;
            }
            else
            {
                // 哦亲爱的孩子你是第一个分配的
                new_sheet->front = front_p;
                front_p->next    = new_sheet;

                // 分配
                *sheet = new_sheet;

                new_sheet->bx        = bx;
                new_sheet->by        = by;
                new_sheet->width     = x;
                new_sheet->height    = y;
                new_sheet->type      = type;
                new_sheet->buffer    = alloc_shared_sheet_buffer(buffer_bytes);
                new_sheet->is_change = true;

                sht->sheet_num++;
            }
        }
        else
        {
            sht->start = new_sheet;

            *sheet = new_sheet;

            new_sheet->bx        = bx;
            new_sheet->by        = by;
            new_sheet->width     = x;
            new_sheet->height    = y;
            new_sheet->type      = type;
            new_sheet->buffer    = alloc_shared_sheet_buffer(buffer_bytes);
            new_sheet->is_change = true;

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

            *sheet = new_sheet;

            new_sheet->bx        = bx;
            new_sheet->by        = by;
            new_sheet->width     = x;
            new_sheet->height    = y;
            new_sheet->type      = type;
            new_sheet->buffer    = alloc_shared_sheet_buffer(buffer_bytes);
            new_sheet->is_change = true;

            sht->sheet_num++;
        }
        else
        {
            sht->start = new_sheet;

            *sheet = new_sheet;

            new_sheet->bx        = bx;
            new_sheet->by        = by;
            new_sheet->width     = x;
            new_sheet->height    = y;
            new_sheet->type      = type;
            new_sheet->buffer    = alloc_shared_sheet_buffer(buffer_bytes);
            new_sheet->is_change = true;

            sht->sheet_num++;
        }
    }
    // write_serial_fmt("Sheet Create Success. SheetID: %d\n", *ct_sheet);
    SheetIsCreating = false;
}

uint32_t getBX(SHEET_INFO *sht, SHEET *csheet)
{
    if (!csheet) return NULL;
    return csheet->bx;
}

uint32_t getBY(SHEET_INFO *sht, SHEET *csheet)
{
    if (!csheet) return NULL;
    return csheet->by;
}

uint32_t getXsize(SHEET_INFO *sht, SHEET *csheet)
{
    if (!csheet) return NULL;
    return csheet->width;
}

uint32_t getYsize(SHEET_INFO *sht, SHEET *csheet)
{
    if (!csheet) return NULL;
    return csheet->height;
}

SHEET *found_sheet(SHEET_INFO *sht, uint32_t x, uint32_t y)
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
                    return front_p;
                }
                else if (front_p->front == NULL) { return NULL; }
                front_p = front_p->front;
            }
            return NULL;
        }
        front_p = front_p->next;
    }
}

SHEET *found_sheetmb(SHEET_INFO *sht, uint32_t x, uint32_t y)
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
                    return front_p;
                }
                else if (front_p->front == NULL) { return NULL; }
                front_p = front_p->front;
            }
            return NULL;
        }
        front_p = front_p->next;
    }
}

SHEET *found_sheet_movable(SHEET_INFO *sht, uint32_t x, uint32_t y)
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
                        return front_p;
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
                        return front_p;
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

void change_sheet_type(SHEET_INFO *sht, SHEET *csheet, uint32_t new_type)
{
    if (!csheet) return;
    csheet->type = new_type;
}

SHEET *get_sheet(SHEET_INFO *sht, SHEET *csheet)
{
    return csheet;
}

void setBX(SHEET_INFO *sht, SHEET *csheet, uint32_t num)
{
    if (!csheet) return;
    csheet->bx = num;
}

void setBY(SHEET_INFO *sht, SHEET *csheet, uint32_t num)
{
    if (!csheet) return;
    csheet->by = num;
}

void delete_sheet(SHEET_INFO *sht, SHEET *csheet)
{
    if (!csheet) return;
    while (true)
    {
        if (!SheetIsCreating && !SheetIsRefreshing) { break; }
    }

    SheetIsCreating = true;
    
    if (csheet->front == NULL) {
        sht->start = csheet->next;
        if (csheet->next) {
            csheet->next->front = NULL;
        }
    } else if (csheet->next == NULL) {
        csheet->front->next = NULL;
    } else {
        csheet->front->next = csheet->next;
        csheet->next->front = csheet->front;
    }
    
    // 释放内存
    free(csheet->buffer);
    memset(csheet, 0, sizeof(SHEET));
    free((void *)(csheet));
    sht->sheet_num--;

    SheetIsCreating = false;
}

// 这个lift_sheet是将图层提升到倒数第三个（低于任务栏和鼠标），不是最顶上！！！没有100%确认不要动这里plz！！！
void lift_sheet(SHEET_INFO *sht, SHEET *csheet)
{
    if (!csheet) return;
    while (true)
    {
        if (!SheetIsRefreshing && !SheetIsCreating) { break; }
    }

    SheetIsCreating  = true;
    SHEET *front_p = csheet;
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

    SheetIsCreating = false;
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

    // 遍历所有图层进行合成 - 优化：减少分支判断
    for (SHEET *st = sht->start; st != nullptr; st = st->next) {
        // 计算当前图层与刷新区域的交集
        int vx0 = (bex1 > (int)st->bx) ? bex1 : (int)st->bx;
        int vy0 = (bey1 > (int)st->by) ? bey1 : (int)st->by;
        int vx1 = (bex2 < (int)(st->bx + st->width)) ? bex2 : (int)(st->bx + st->width);
        int vy1 = (bey2 < (int)(st->by + st->height)) ? bey2 : (int)(st->by + st->height);

        // 不在当前刷新区域内直接跳过
        if (vx0 >= vx1 || vy0 >= vy1) continue;
        if (!is_valid_sheet_buffer(st->buffer)) continue;

        SHEET_BUFFER *st_buf = (SHEET_BUFFER *)st->buffer;
        int width = vx1 - vx0; // 计算一次，避免重复计算
        
        for (int y = vy0; y < vy1; y++) {
            int screen_off = y * sht->scrx + vx0;
            int st_off = (y - st->by) * st->width + (vx0 - st->bx);
            
            SHEET_BUFFER *p_dst = &temp_buf_base[screen_off];
            SHEET_BUFFER *p_src = &st_buf[st_off];
            uint8_t *p_sign = &temp_peek_sign[screen_off];

            // 展开循环，减少分支预测失败
            for (int x = 0; x < width; x++) {
                uint8_t alpha = p_src[x].a;
                
                if (alpha == 255) {
                    // 全不透明，直接复制
                    p_dst[x] = p_src[x];
                    p_sign[x] = 0;
                } else if (alpha > 0) {
                    // Alpha 混合
                    uint32_t inv_alpha = 255 - alpha;
                    
                    p_dst[x].r = (p_src[x].r * alpha + p_dst[x].r * inv_alpha) >> 8;
                    p_dst[x].g = (p_src[x].g * alpha + p_dst[x].g * inv_alpha) >> 8;
                    p_dst[x].b = (p_src[x].b * alpha + p_dst[x].b * inv_alpha) >> 8;
                    
                    p_sign[x] = 1;
                }
            }
        }
    }

    // Foxland Peek 处理 - 优化：减少指针计算
    SHEET_BUFFER *temp_peek = (SHEET_BUFFER *)sht->foxland.fl_peek_buffer;
    int peek_width = bex2 - bex1;
    
    for (int y = bey1; y < bey2; y++) {
        int offset = y * sht->scrx + bex1;
        SHEET_BUFFER *p_dst = &temp_buf_base[offset];
        uint8_t *p_sign = &temp_peek_sign[offset];
        SHEET_BUFFER *p_peek = &temp_peek[offset];

        for (int x = 0; x < peek_width; x++) {
            if (p_sign[x] == 1) {
                uint32_t alpha = p_peek[x].a;
                uint32_t inv_alpha = 255 - alpha;
                p_dst[x].r = (p_peek[x].r * alpha + p_dst[x].r * inv_alpha) >> 8;
                p_dst[x].g = (p_peek[x].g * alpha + p_dst[x].g * inv_alpha) >> 8;
                p_dst[x].b = (p_peek[x].b * alpha + p_dst[x].b * inv_alpha) >> 8;
                p_sign[x] = 0; // 重置标志位
            }
        }
    }

    // 写入显存 - 优化：使用 memcpy 批量复制
    uint8_t *frame_buffer = (uint8_t *)sht->fbc->frame_buffer;
    uint32_t pixels_per_scan_line = sht->fbc->pixels_per_scan_line;
    size_t row_bytes = (bex2 - bex1) * 4; // 每行的字节数
    
    if (sht->fbc->pixel_format == PixelFormat::kRGBR) {
        // RGB 格式
        for (int y = bey1; y < bey2; y++) {
            SHEET_BUFFER *src_line = &temp_buf_base[y * sht->scrx + bex1];
            uint8_t *dst_line = frame_buffer + (y * pixels_per_scan_line + bex1) * 4;
            
            // 转换并复制整行
            for (int x = 0; x < (bex2 - bex1); x++) {
                dst_line[x * 4 + 0] = src_line[x].r;
                dst_line[x * 4 + 1] = src_line[x].g;
                dst_line[x * 4 + 2] = src_line[x].b;
                dst_line[x * 4 + 3] = 0;
            }
        }
    } else {
        // BGR 格式
        for (int y = bey1; y < bey2; y++) {
            SHEET_BUFFER *src_line = &temp_buf_base[y * sht->scrx + bex1];
            uint8_t *dst_line = frame_buffer + (y * pixels_per_scan_line + bex1) * 4;
            
            // 转换并复制整行
            for (int x = 0; x < (bex2 - bex1); x++) {
                dst_line[x * 4 + 0] = src_line[x].b;
                dst_line[x * 4 + 1] = src_line[x].g;
                dst_line[x * 4 + 2] = src_line[x].r;
                dst_line[x * 4 + 3] = 0;
            }
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

void copy_buffer_by_id(SHEET_INFO *sht, SHEET *dst_sheet, SHEET_BUFFER *src, uint32_t dst_x, uint32_t dst_y,
                       uint32_t width, uint32_t height)
{
    if (!dst_sheet || src == NULL) return;

    int dst_x_i = (int)dst_x;
    int dst_y_i = (int)dst_y;
    int src_x_i = 0;
    int src_y_i = 0;
    int width_i = (int)width;
    int height_i = (int)height;
    int src_stride = (int)width;
    if (!clip_sheet_copy_region(dst_sheet, &dst_x_i, &dst_y_i, &src_x_i, &src_y_i, &width_i, &height_i)) return;

    SHEET_BUFFER *dst = (SHEET_BUFFER *)dst_sheet->buffer;
    for (int y = 0; y < height_i; y++)
    {
        for (int x = 0; x < width_i; x++)
        {
            dst[(dst_y_i + y) * dst_sheet->width + (dst_x_i + x)] =
                src[(src_y_i + y) * src_stride + (src_x_i + x)];
        }
    }
}

void copy_buffer_by_id_without_alpha(SHEET_INFO *sht, SHEET *dst_sheet, SHEET_BUFFER *src, uint32_t dst_x,
                                     uint32_t dst_y, uint32_t width, uint32_t height)
{
    if (!dst_sheet || src == NULL) return;

    int dst_x_i = (int)dst_x;
    int dst_y_i = (int)dst_y;
    int src_x_i = 0;
    int src_y_i = 0;
    int width_i = (int)width;
    int height_i = (int)height;
    int src_stride = (int)width;
    if (!clip_sheet_copy_region(dst_sheet, &dst_x_i, &dst_y_i, &src_x_i, &src_y_i, &width_i, &height_i)) return;

    SHEET_BUFFER *dst = (SHEET_BUFFER *)dst_sheet->buffer;
    for (int y = 0; y < height_i; y++)
    {
        for (int x = 0; x < width_i; x++)
        {
            SHEET_BUFFER pixel = src[(src_y_i + y) * src_stride + (src_x_i + x)];
            if (pixel.a > 10) dst[(dst_y_i + y) * dst_sheet->width + (dst_x_i + x)] = pixel;
        }
    }
}

SHEET_BUFFER LCD_AlphaBlend(SHEET_BUFFER foreground_color, SHEET_BUFFER background_color, uint8_t alpha);

void copy_buffer_blend_by_id(SHEET_INFO *sht, SHEET *dst_sheet, SHEET_BUFFER *src, uint32_t dst_x, uint32_t dst_y,
                             uint32_t width, uint32_t height)
{
    if (!dst_sheet || src == NULL) return;

    int dst_x_i = (int)dst_x;
    int dst_y_i = (int)dst_y;
    int src_x_i = 0;
    int src_y_i = 0;
    int width_i = (int)width;
    int height_i = (int)height;
    int src_stride = (int)width;
    if (!clip_sheet_copy_region(dst_sheet, &dst_x_i, &dst_y_i, &src_x_i, &src_y_i, &width_i, &height_i)) return;

    SHEET_BUFFER *dst = (SHEET_BUFFER *)dst_sheet->buffer;
    for (int y = 0; y < height_i; y++)
    {
        for (int x = 0; x < width_i; x++)
        {
            SHEET_BUFFER pixel = src[(src_y_i + y) * src_stride + (src_x_i + x)];
            dst[(dst_y_i + y) * dst_sheet->width + (dst_x_i + x)] =
                LCD_AlphaBlend(pixel, dst[(dst_y_i + y) * dst_sheet->width + (dst_x_i + x)], pixel.a);
        }
    }
}

void *get_sheet_buffer(SHEET_INFO *sht, SHEET *csheet)
{
    if (!csheet) return NULL;
    return csheet->buffer;
}

SHEET *found_sheet_byid(SHEET_INFO *sht, SHEET *csheet)
{
    return csheet;
}
