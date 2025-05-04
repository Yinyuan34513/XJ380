#define STB_TRUETYPE_IMPLEMENTATION
#include "../../include/include.h"
#include "../../lib/math.h"
#include "./stb_ttf.h"


char *ttf_buffer;
stbtt_fontinfo font;
uint32_t LCD_AlphaBlend(uint32_t foreground_color, uint32_t background_color,
                        uint8_t alpha) {
	uint8_t *fg = (uint8_t *)&foreground_color;
	uint8_t *bg = (uint8_t *)&background_color;

	uint32_t rb = (((uint32_t)(*fg & 0xFF) * alpha) +
					((uint32_t)(*bg & 0xFF) * (256 - alpha))) >>
					8;
	uint32_t g = (((uint32_t)(*(fg + 1) & 0xFF) * alpha) +
					((uint32_t)(*(bg + 1) & 0xFF) * (256 - alpha))) >>
				8;
	uint32_t a = (((uint32_t)(*(fg + 2) & 0xFF) * alpha) +
					((uint32_t)(*(bg + 2) & 0xFF) * (256 - alpha))) >>
				8;

	return (rb & 0xFF) | ((g & 0xFF) << 8) | ((a & 0xFF) << 16);
}
void set_size(int s1) {}
#define EPS (2.22044604925031308085e-16)
static const float32_t toint = 1 / EPS;

float roundf(float x) {
	union {
		float f;
		uint32_t i;
	} u = {x};
	int e = u.i >> 23 & 0xff;
	float32_t y;

	if (e >= 0x7f + 23)
		return x;
	if (u.i >> 31)
		x = -x;
	if (e < 0x7f - 1) {
		FORCE_EVAL(x + toint);
		return 0 * u.f;
	}
	y = x + toint - toint - x;
	if (y > 0.5f)
		y = y + x - 1;
	else if (y <= -0.5f)
		y = y + x + 1;
	else
		y = y + x;
	if (u.i >> 31)
		y = -y;
	return y;
}
unsigned char *TTF_Print(int *buf, unsigned *width, unsigned *heigh) {
	/* 创建位图 */
	int bitmap_w = 1024; /* 位图的宽 */
	int bitmap_h = 256; /* 位图的高 */
	unsigned char *bitmap = (unsigned char *)(malloc(bitmap_w * bitmap_h));
	xmemset((void *)(bitmap), 0, sizeof(bitmap_w * bitmap_h));

	/* "STB"的 unicode 编码 */
	int *word = buf;

	/* 计算字体缩放 */
	float pixels = 30.0; /* 字体大小（字号） */
	float scale = stbtt_ScaleForPixelHeight(
		&font, pixels); /* scale = pixels / (ascent - descent) */

	/**
	 * 获取垂直方向上的度量
	 * ascent：字体从基线到顶部的高度；
	 * descent：基线到底部的高度，通常为负值；
	 * lineGap：两个字体之间的间距；
	 * 行间距为：ascent - descent + lineGap。
	 */
	int ascent = 0;
	int descent = 0;
	int lineGap = 0;
	stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);

	/* 根据缩放调整字高 */
	ascent = roundf(ascent * scale);
	descent = roundf(descent * scale);

	int x = 0; /*位图的x*/
	int max = 0;
	/* 循环加载word中每个字符 */
	unsigned h = (int)((float)(ascent - descent + lineGap * scale));
	for (int i = 0; word[i] != 0; ++i) {
		/**
		 * 获取水平方向上的度量
		 * advanceWidth：字宽；
		 * leftSideBearing：左侧位置；
		 */
		int advanceWidth = 0;
		int leftSideBearing = 0;
		stbtt_GetCodepointHMetrics(&font, word[i], &advanceWidth, &leftSideBearing);

		/* 获取字符的边框（边界） */
		int c_x1, c_y1, c_x2, c_y2;
		stbtt_GetCodepointBitmapBox(&font, word[i], scale, scale, &c_x1, &c_y1,
									&c_x2, &c_y2);

		/* 计算位图的y (不同字符的高度不同） */
		int y = ascent + c_y1;
		// if (y > max)
		//   max = y;
		/* 渲染字符 */
		int byteOffset = x + roundf(leftSideBearing * scale) + (y * bitmap_w);
		stbtt_MakeCodepointBitmap(&font, bitmap + byteOffset, c_x2 - c_x1,
								c_y2 - c_y1, bitmap_w, scale, scale, word[i]);

		/* 调整x */
		x += roundf(advanceWidth * scale);

		/* 调整字距 */
		int kern;
		kern = stbtt_GetCodepointKernAdvance(&font, word[i], word[i + 1]);
		x += roundf(kern * scale);
	}
	*width = x;
	*heigh = max + h;
	return bitmap;
}

void put_bitmap(SHEET_INFO *sht, UINT16 *sheet_id, unsigned char *bitmap, 
				unsigned x, unsigned y, 
				unsigned width, unsigned heigh, 
				unsigned bitmap_xsize, unsigned fc) {
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

	uint32_t *SheetBuffer = (uint32_t *)(front_p->buffer);
	for (int i = 0; i < width; i++) {
		for (int j = 0; j < heigh; j++) {
			SheetBuffer[(y + j) * front_p->width + (x + i)] = LCD_AlphaBlend(
				fc, SheetBuffer[(y + j) * front_p->width + (x + i)], bitmap[j * bitmap_xsize + i]);
		}
	}
}

void print_box_ttf(SHEET_INFO *sht, UINT16 *sheet_id, char *buf, unsigned fc,
                   /*unsigned bc, */unsigned x, unsigned y) {
	Rune *r = (Rune *)malloc((utflen(buf) + 1) * sizeof(Rune));
	r[utflen(buf)] = 0;
	int i = 0;
	while (*buf != '\0') {
		buf += chartorune(&(r[i++]), buf);
	}

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

	unsigned width, heigh;
	unsigned char *bitmap = TTF_Print(r, &width, &heigh);
	put_bitmap(sht, sheet_id, bitmap, x, y, width, heigh, 1024, fc);
	refresh_part_sheet(sht, front_p->bx + x, front_p->by + y, 
							front_p->bx + x + width, front_p->by + y + heigh);
	free(bitmap);
	free(r);
}

void init_ttf()
{
	write_serial_string("Initializing TTF...\n");
	delay_s(10);
	VNode *v = path_walk(current_task->cwd_node, "/XJ380F.ttf");
	if (!v)
	{
		write_serial_string("Cannot find XJ380F.ttf\n");
		wsod_cannot_load_ttf();
		return;
	}
	ttf_buffer = (char *)malloc(v->file_size);
	write_serial_string("Reading TTF...\n");
	v->ops->read(v, (uint8_t *)ttf_buffer, 0, v->file_size);
	if (*(uint64_t *)ttf_buffer == 0)
	{
		write_serial_string("Cannot read XJ380F.ttf\n");
		wsod_cannot_load_ttf();
		return;
	}
	write_serial_string("Loading TTF...\n");
	stbtt_InitFont(&font, (uint8_t *)ttf_buffer, stbtt_GetFontOffsetForIndex((uint8_t *)ttf_buffer, 0));
	set_size(16);
	v->ops->close(v);

	write_serial_string("TTF Initialize Success.\n");
}
