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

char          *ttf_buffer_c;
stbtt_fontinfo font_c;

SHEET_BUFFER   LCD_AlphaBlend(SHEET_BUFFER foreground_color, SHEET_BUFFER background_color, uint8_t alpha);
void put_bitmap(SHEET_INFO *sht, uint16_t *sheet_id, unsigned char *bitmap, unsigned x, unsigned y, unsigned width,
                unsigned heigh, unsigned bitmap_xsize, SHEET_BUFFER fc);
void print_fmt_box_ttf_c(SHEET_INFO *sht, uint16_t *sheet_id, SHEET_BUFFER fc, int x, int y, int size, const char *fmt,
                       ...);

unsigned char *TTF_Print_c(int *buf, unsigned *width, unsigned *heigh, int size)
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
    float scale  = stbtt_ScaleForPixelHeight(&font_c, pixels); /* scale = pixels / (ascent - descent) */

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
    stbtt_GetFontVMetrics(&font_c, &ascent, &descent, &lineGap);

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
        stbtt_GetCodepointHMetrics(&font_c, word[i], &advanceWidth, &leftSideBearing);

        /* 获取字符的边框（边界） */
        int c_x1, c_y1, c_x2, c_y2;
        stbtt_GetCodepointBitmapBox(&font_c, word[i], scale, scale, &c_x1, &c_y1, &c_x2, &c_y2);

        /* 计算位图的y (不同字符的高度不同） */
        int y = ascent + c_y1;

        /* 渲染字符 */
        if (i <= 0) { offset_x = (float)(-leftSideBearing) * scale; } /* 把首个字符的左侧偏移，给整体偏移回去 */
        int byteOffset = x + leftSideBearing * scale + (y * bitmap_w) + (int)roundf(offset_x);
        stbtt_MakeCodepointBitmap(&font_c, bitmap + byteOffset, c_x2 - c_x1, c_y2 - c_y1, bitmap_w, scale, scale,
                                  word[i]);

        /* 调整x */
        x += roundf(advanceWidth * scale);

        /* 调整字距 */
        int kern;
        kern  = stbtt_GetCodepointKernAdvance(&font_c, word[i], word[i + 1]);
        x    += roundf(kern * scale);
    }
    *width = x;
    *heigh = h;

    return bitmap;
}

void print_box_ttf_c(SHEET_INFO *sht, uint16_t *sheet_id, char *buf, SHEET_BUFFER fc,
                   /*unsigned bc, */ unsigned x, unsigned y, int size)
{
    print_fmt_box_ttf_c(sht, sheet_id, fc, (int)x, (int)y, size, "%s", buf);
}

#define TTF_BUF_SIZE 128

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
static void flush_ttf_buffer_c(FbTTFWriterData *data)
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
    unsigned char *bitmap = TTF_Print_c(r, &width, &height, data->size);
    put_bitmap(data->sht, data->sheet_id, bitmap, data->x, data->y, width, height, 1920, data->color);
    refresh_part_sheet(data->sht, data->x, data->y, data->x + (int)width, data->y + (int)height);

    // 更新光标位置
    data->x += (int)width;

    free(bitmap);
    if (free_rune) { free(r); }
    data->idx = 0; // 重置缓冲区
}

uint8_t fb_ttf_writer_handler_c(Writer *writer, char ch)
{
    FbTTFWriterData *fbttf = (FbTTFWriterData *)writer->data;

    // 处理缓冲区溢出
    if (fbttf->idx + utf8_char_len(ch) > fbttf->buf_size - 1) { flush_ttf_buffer_c(fbttf); }

    // 将字符添加到缓冲区
    fbttf->buf[fbttf->idx++] = ch;

    // 如果是换行符，立即刷新
    if (ch == '\n')
    {
        flush_ttf_buffer_c(fbttf);
        fbttf->y += fbttf->size + 4;
        fbttf->x  = fbttf->start_x;
    }

    return 1;
}

void print_fmt_box_ttf_c(SHEET_INFO *sht, uint16_t *sheet_id, SHEET_BUFFER fc, int x, int y, int size, const char *fmt,
                       ...)
{
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

    Writer fb_ttf_writer = {.data = &fb_ttf_writer_data, .handler = fb_ttf_writer_handler_c};

    va_list args;
    va_start(args, fmt);
    vwprintf(&fb_ttf_writer, fmt, args);
    va_end(args);

    // 刷新剩余内容
    flush_ttf_buffer_c(&fb_ttf_writer_data);

    free(buf);
    free(rune_buf);
}
