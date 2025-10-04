#define STB_TRUETYPE_IMPLEMENTATION
#include "./stb_ttf.h"
#include "krlibc.h"
#include "stdarg.h"
#include "stdint.h"
#include <fs/vfs/vfs.h>
#include <graphics/GOP.hpp>
#include <graphics/sheet.h>
#include <math.hpp>
#include <proto.hpp>
#include <ttf.h>

char          *ttf_buffer;
stbtt_fontinfo font;

SHEET_BUFFER   *mono_xpf_buffer;

SHEET_BUFFER   LCD_AlphaBlend(SHEET_BUFFER foreground_color, SHEET_BUFFER background_color, uint8_t alpha)
{
    uint8_t       text_r   = foreground_color.r;
    uint8_t       text_g   = foreground_color.g;
    uint8_t       text_b   = foreground_color.b;
    uint8_t       text_a   = foreground_color.a;
    uint8_t       coverage = alpha;
    SHEET_BUFFER *dst      = &background_color;

    // 转换为浮点运算（0.0-1.0范围）
    float cov       = coverage / 255.0f;
    float txt_alpha = text_a / 255.0f;
    float src_alpha = cov * txt_alpha;

    // 源颜色预乘alpha
    float src_r = (text_r / 255.0f) * src_alpha;
    float src_g = (text_g / 255.0f) * src_alpha;
    float src_b = (text_b / 255.0f) * src_alpha;

    // 目标颜色预乘alpha
    float dst_a = dst->a / 255.0f;
    float dst_r = (dst->r / 255.0f) * dst_a;
    float dst_g = (dst->g / 255.0f) * dst_a;
    float dst_b = (dst->b / 255.0f) * dst_a;

    // 混合计算
    float blended_alpha = src_alpha + dst_a * (1.0f - src_alpha);

    // 颜色混合
    float blended_r = (src_r + dst_r * (1.0f - src_alpha)) / blended_alpha;
    float blended_g = (src_g + dst_g * (1.0f - src_alpha)) / blended_alpha;
    float blended_b = (src_b + dst_b * (1.0f - src_alpha)) / blended_alpha;

    // 转换回0-255并存储
    dst->r = (uint8_t)(blended_r * 255 + 0.5f);
    dst->g = (uint8_t)(blended_g * 255 + 0.5f);
    dst->b = (uint8_t)(blended_b * 255 + 0.5f);
    dst->a = (uint8_t)(blended_alpha * 255 + 0.5f);
    return *dst;
}

void set_size(int s1) {}

int roundf(double n)
{
    // 若为负数，则先化为正数再进行四舍五入
    if (n > 0)
        return n - int(n) >= 0.5 ? int(n) + 1 : int(n);
    else
        return -n - int(-n) >= 0.5 ? -(int(-n) + 1) : -int(-n);
}

unsigned char *TTF_Print(int *buf, unsigned *width, unsigned *heigh, int size)
{
    /* 创建位图 */
    int            bitmap_w = 1920; /* 位图的宽 */
    int            bitmap_h = 1080; /* 位图的高 */
    unsigned char *bitmap   = (unsigned char *)(malloc(bitmap_w * bitmap_h));
    memset((void *)(bitmap), 0, bitmap_w * bitmap_h);

    /* "STB"的 unicode 编码 */
    int *word = buf;

    /* 计算字体缩放 */
    float pixels = size * 2;                                 /* 字体大小（字号） */
    float scale  = stbtt_ScaleForPixelHeight(&font, pixels); /* scale = pixels / (ascent - descent) */

    /**
	 * 获取垂直方向上的度量
	 * ascent：字体从基线到顶部的高度；
	 * descent：基线到底部的高度，通常为负值；
	 * lineGap：两个字体之间的间距；
	 * 行间距为：ascent - descent + lineGap。
	 */
    int ascent  = 0;
    int descent = 0;
    int lineGap = 0;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);

    /* 根据缩放调整字高 */
    ascent  = roundf(ascent * scale);
    descent = roundf(descent * scale);

    int x = 0; /*位图的x*/

    float    offset_x = 0; /* 整体的x偏移量 */
    /* 循环加载word中每个字符 */
    unsigned h = (int)((float)(ascent - descent + lineGap * scale));
    for (int i = 0; word[i] != 0; ++i)
    {
        /**
		 * 获取水平方向上的度量
		 * advanceWidth：字宽；
		 * leftSideBearing：左侧位置；
		 */
        int advanceWidth    = 0;
        int leftSideBearing = 0;
        stbtt_GetCodepointHMetrics(&font, word[i], &advanceWidth, &leftSideBearing);

        /* 获取字符的边框（边界） */
        int c_x1, c_y1, c_x2, c_y2;
        stbtt_GetCodepointBitmapBox(&font, word[i], scale, scale, &c_x1, &c_y1, &c_x2, &c_y2);

        /* 计算位图的y (不同字符的高度不同） */
        int y = ascent + c_y1;

        /* 渲染字符 */
        if (i <= 0) { offset_x = (float)(-leftSideBearing) * scale; } /* 把首个字符的左侧偏移，给整体偏移回去 */
        int byteOffset = x + leftSideBearing * scale + (y * bitmap_w) + (int)roundf(offset_x);
        stbtt_MakeCodepointBitmap(&font, bitmap + byteOffset, c_x2 - c_x1, c_y2 - c_y1, bitmap_w, scale, scale,
                                  word[i]);

        /* 调整x */
        x += roundf(advanceWidth * scale);

        /* 调整字距 */
        int kern;
        kern  = stbtt_GetCodepointKernAdvance(&font, word[i], word[i + 1]);
        x    += roundf(kern * scale);
    }
    *width = x;
    *heigh = h;

    return bitmap;
}

void put_bitmap(SHEET_INFO *sht, uint16_t *sheet_id, unsigned char *bitmap, unsigned x, unsigned y, unsigned width,
                unsigned heigh, unsigned bitmap_xsize, SHEET_BUFFER fc)
{
    // 查找图层
    SHEET *front_p = sht->start;
    while (true)
    {
        if (front_p->index == *sheet_id) { break; }
        else if (front_p->next == NULL) { return; }
        front_p = front_p->next;
    }

    SHEET_BUFFER *SheetBuffer = (SHEET_BUFFER *)(front_p->buffer);
    for (int i = 0; i < width; i++)
    {
        for (int j = 0; j < heigh; j++)
        {
            SheetBuffer[(y + j) * front_p->width + (x + i)] =
                LCD_AlphaBlend(fc, SheetBuffer[(y + j) * front_p->width + (x + i)], bitmap[j * bitmap_xsize + i]);
        }
    }
}

void print_box_ttf(SHEET_INFO *sht, uint16_t *sheet_id, char *buf, SHEET_BUFFER fc,
                   /*unsigned bc, */ unsigned x, unsigned y, int size)
{
    uint32_t temp_i_width;
    print_fmt_box_ttf(sht, sheet_id, fc, (int)x, (int)y, size, &temp_i_width, "%s", buf);
}

void print_box_ttfl(SHEET_INFO *sht, uint16_t *sheet_id, char *buf, SHEET_BUFFER fc,
                   /*unsigned bc, */ unsigned x, unsigned y, int size, uint32_t *i_width)
{
    print_fmt_box_ttf(sht, sheet_id, fc, (int)x, (int)y, size, i_width, "%s", buf);
}

#define TTF_BUF_SIZE 128

typedef struct FbTTFWriterData
{
    SHEET_INFO  *sht;
    uint16_t    *sheet_id;
    SHEET_BUFFER color;
    int          start_x;
    int          start_y;
    int          x;
    int          y;
    int          size;

    // 缓冲区设计
    Rune   *rune_buf;      // Rune缓冲区指针
    size_t  rune_buf_size; // Rune缓冲区总大小
    char   *buf;           // 缓冲区指针
    size_t  idx;           // 当前写入位置
    size_t  buf_size;      // 缓冲区总大小
    uint8_t flush;         // 刷新标志
} FbTTFWriterData;

// 辅助函数：计算UTF-8字符的字节数
static int utf8_char_len(unsigned char c)
{
    if ((c & 0x80) == 0x00) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1; // 无效UTF-8 / 非前缀，按单字节处理
}

// 刷新缓冲区内容到屏幕
static void flush_ttf_buffer(FbTTFWriterData *data, uint32_t *i_width)
{
    if (data->idx == 0) return;

    // 确保缓冲区以'\0'结尾
    data->buf[data->idx] = '\0';

    // 转换UTF-8字符串到Rune数组
    size_t  utf_len   = utflen(data->buf);
    Rune   *r         = NULL;
    uint8_t free_rune = 0;
    if (utf_len > data->rune_buf_size)
    {
        r         = (Rune *)malloc((utf_len + 1) * sizeof(Rune));
        free_rune = 1;
    }
    else { r = data->rune_buf; }
    r[utf_len] = 0;
    int   i    = 0;
    char *p    = data->buf;
    while (*p != '\0')
    {
        p += chartorune(&r[i++], p);
    }

    // 查找图层
    SHEET *front_p = data->sht->start;
    while (front_p != NULL)
    {
        if (front_p->index == *data->sheet_id) break;
        front_p = front_p->next;
    }
    if (front_p == NULL)
    {
        free(r);
        return;
    }

    // 渲染文本
    unsigned int   width  = 0;
    unsigned int   height = 0;
    unsigned char *bitmap = TTF_Print(r, &width, &height, data->size);
    put_bitmap(data->sht, data->sheet_id, bitmap, data->x, data->y, width, height, 1920, data->color);
    refresh_part_sheet(data->sht, data->x, data->y, data->x + (int)width, data->y + (int)height);

    *i_width = width;

    // 更新光标位置
    data->x += (int)width;

    free(bitmap);
    if (free_rune) { free(r); }
    data->idx = 0; // 重置缓冲区
}

uint32_t *input_width; // 玛德沙比format，只能写成这样了

uint8_t fb_ttf_writer_handler(Writer *writer, char ch)
{
    FbTTFWriterData *fbttf = (FbTTFWriterData *)writer->data;

    // 处理缓冲区溢出
    if (fbttf->idx + utf8_char_len(ch) > fbttf->buf_size - 1) { flush_ttf_buffer(fbttf, input_width); }

    // 将字符添加到缓冲区
    fbttf->buf[fbttf->idx++] = ch;

    // 如果是换行符，立即刷新
    if (ch == '\n')
    {
        flush_ttf_buffer(fbttf, input_width);
        fbttf->y += fbttf->size + 4;
        fbttf->x  = fbttf->start_x;
    }

    return 1;
}

void print_fmt_box_ttf(SHEET_INFO *sht, uint16_t *sheet_id, SHEET_BUFFER fc, int x, int y, int size, const char *fmt,
                       ...)
{
    uint32_t temp_i_width;
    input_width = &temp_i_width;
    char *buf = (char *)malloc(TTF_BUF_SIZE * sizeof(char));
    // char buf[TTF_BUF_SIZE];
    buf[0]         = '\0';
    Rune *rune_buf = (Rune *)malloc(TTF_BUF_SIZE * sizeof(Rune));
    // Rune rune_buf[TTF_BUF_SIZE];

    // 初始化写入器数据
    FbTTFWriterData fb_ttf_writer_data = {
        .sht           = sht,
        .sheet_id      = sheet_id,
        .color         = fc,
        .start_x       = x,
        .start_y       = y,
        .x             = x,
        .y             = y,
        .size          = size,
        .rune_buf      = rune_buf,
        .rune_buf_size = TTF_BUF_SIZE,
        .buf           = buf,
        .idx           = 0,
        .buf_size      = TTF_BUF_SIZE,
        .flush         = 0,
    };

    Writer fb_ttf_writer = {.data = &fb_ttf_writer_data, .handler = fb_ttf_writer_handler};

    va_list args;
    va_start(args, fmt);
    vwprintf(&fb_ttf_writer, fmt, args);
    va_end(args);

    // 刷新剩余内容
    flush_ttf_buffer(&fb_ttf_writer_data, &temp_i_width);

    free(buf);
    free(rune_buf);
}

void print_fmt_box_ttf(SHEET_INFO *sht, uint16_t *sheet_id, SHEET_BUFFER fc, int x, int y, int size, uint32_t *i_width, const char *fmt,
                       ...)
{
    input_width = i_width;
    char *buf = (char *)malloc(TTF_BUF_SIZE * sizeof(char));
    // char buf[TTF_BUF_SIZE];
    buf[0]         = '\0';
    Rune *rune_buf = (Rune *)malloc(TTF_BUF_SIZE * sizeof(Rune));
    // Rune rune_buf[TTF_BUF_SIZE];

    // 初始化写入器数据
    FbTTFWriterData fb_ttf_writer_data = {
        .sht           = sht,
        .sheet_id      = sheet_id,
        .color         = fc,
        .start_x       = x,
        .start_y       = y,
        .x             = x,
        .y             = y,
        .size          = size,
        .rune_buf      = rune_buf,
        .rune_buf_size = TTF_BUF_SIZE,
        .buf           = buf,
        .idx           = 0,
        .buf_size      = TTF_BUF_SIZE,
        .flush         = 0,
    };

    Writer fb_ttf_writer = {.data = &fb_ttf_writer_data, .handler = fb_ttf_writer_handler};

    va_list args;
    va_start(args, fmt);
    vwprintf(&fb_ttf_writer, fmt, args);
    va_end(args);

    // 刷新剩余内容
    flush_ttf_buffer(&fb_ttf_writer_data, i_width);

    free(buf);
    free(rune_buf);
}

// void draw_text(SHEET_INFO *sht, WINDOWLS* wls, char *buf, SHEET_BUFFER fc,
// 				/*unsigned bc, */ unsigned x, unsigned y, int size)
// {
// 	print_box_ttf(sht, &(wls->w_sheet), buf, fc,
// 				  /*bc, */ x, y, size);
// }

char mono_index1[53] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
char mono_index2[43] = "0123456789~-=!@#$%^&*()_+[]\\{}|;':\",./<>? ";

// 刷新缓冲区内容到屏幕
void print_box_ttfc(SHEET_INFO *sht, uint16_t *sheet_id, char *buf, SHEET_BUFFER fc,
				   /*unsigned bc, */ unsigned x, unsigned y, int size)
{
	// 查找图层
	SHEET *front_p = sht->start;
	while (true)
	{
		if (front_p->index == *sheet_id)
		{
			break;
		}
		else if (front_p->next == NULL)
		{
			return;
		}
		front_p = front_p->next;
	}
	SHEET_BUFFER *SheetBuffer = (SHEET_BUFFER *)(front_p->buffer);

    int ox = x;
    int oy = y;

    char *current_char = buf;
    for (int len = 0; len < strlen(buf); len++)
    {
        char index_pointer = 0;
        bool in_index1     = true;
        while (mono_index1[index_pointer])
        {
            if (mono_index1[index_pointer] == *current_char)
                goto flush_mono;
            index_pointer++;
        }

        in_index1 = false;
        index_pointer = 0;
        while (mono_index2[index_pointer])
        {
            if (mono_index2[index_pointer] == *current_char)
                goto flush_mono;
            index_pointer++;
        }

        write_serial_string("Font Flusher: Character not found.\n");
        continue;

flush_mono:
        for (int i = 0; i < 9; i++)
        {
            for (int j = 0; j < 16; j++)
            {
                SheetBuffer[(y + j) * front_p->width + (x + i)] =
                    LCD_AlphaBlend(fc, SheetBuffer[(y + j) * front_p->width + (x + i)], 
                    255 - mono_xpf_buffer[(j + (in_index1 ? 0 : 16)) * 468 + (i + index_pointer * 9)].a);
            }
        }

        current_char++;
        x += 9;
    }
    refresh_part_sheet(sht, ox, oy, x + strlen(buf) * 9, y + 16);
}



void init_ttf()
{
    write_serial_string("Initializing Font...\n");
    // delay_s(10);

    // 完整不等宽字体
    vfs_node_t v = vfs_open("/system/XJ380F.ttf");
    if (!v)
    {
        write_serial_string("Cannot find XJ380F.ttf\n");
        // wsod_cannot_load_ttf();
        return;
    }
    ttf_buffer = (char *)malloc(v->size);
    write_serial_string("Reading Font - XJ380F (1/2)...\n");
    vfs_read(v, (uint8_t *)ttf_buffer, 0, v->size);
    if (*(uint64_t *)ttf_buffer == 0)
    {
        write_serial_string("Cannot read XJ380F.ttf\n");
        return;
    }
    write_serial_string("Loading Font - XJ380F (1/2)...\n");
    stbtt_InitFont(&font, (uint8_t *)ttf_buffer, stbtt_GetFontOffsetForIndex((uint8_t *)ttf_buffer, 0));
    set_size(16);

    vfs_close(v);

    // 英文等宽字体
    write_serial_string("Reading Font - XJ380C (2/2)...\n");
    LoadPictureOgM(&mono_xpf_buffer, "/system/XJ380C.xpf");
    if (!mono_xpf_buffer)
    {
        write_serial_string("Cannot find/read XJ380C.xpf\n");
        return;
    }
    write_serial_string("Loading Font - XJ380C (2/2)...\n");

    write_serial_string("TTF Initialize Success.\n");
}
