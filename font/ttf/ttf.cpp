#define STB_TRUETYPE_IMPLEMENTATION
#include "../../include/include.h"
#include "../../lib/math.h"
#include "./stb_ttf.h"

char *ttf_buffer;
stbtt_fontinfo font;
SHEET_BUFFER LCD_AlphaBlend(SHEET_BUFFER foreground_color, SHEET_BUFFER background_color,
							uint8_t alpha)
{
	uint8_t text_r = foreground_color.r;
	uint8_t text_g = foreground_color.g;
	uint8_t text_b = foreground_color.b;
	uint8_t text_a = foreground_color.a;
	uint8_t coverage = alpha;
	SHEET_BUFFER *dst = &background_color;

	// 转换为浮点运算（0.0-1.0范围）
	float cov = coverage / 255.0f;
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
	int bitmap_w = 1920; /* 位图的宽 */
	int bitmap_h = 786;	 /* 位图的高 */
	unsigned char *bitmap = (unsigned char *)(malloc(bitmap_w * bitmap_h));
	xmemset((void *)(bitmap), 0, bitmap_w * bitmap_h);

	/* "STB"的 unicode 编码 */
	int *word = buf;

	/* 计算字体缩放 */
	float pixels = size * 2; /* 字体大小（字号） */
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
	/* 循环加载word中每个字符 */
	unsigned h = (int)((float)(ascent - descent + lineGap * scale));
	for (int i = 0; word[i] != 0; ++i)
	{
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

		/* 渲染字符 */
		int byteOffset = x + leftSideBearing * scale + (y * bitmap_w);
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
	*heigh = h;

	return bitmap;
}

void put_bitmap(SHEET_INFO *sht, UINT16 *sheet_id, unsigned char *bitmap,
				unsigned x, unsigned y,
				unsigned width, unsigned heigh,
				unsigned bitmap_xsize, SHEET_BUFFER fc)
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
	for (int i = 0; i < width; i++)
	{
		for (int j = 0; j < heigh; j++)
		{
			SheetBuffer[(y + j) * front_p->width + (x + i)] = LCD_AlphaBlend(
				fc, SheetBuffer[(y + j) * front_p->width + (x + i)], bitmap[j * bitmap_xsize + i]);
		}
	}
}

void print_box_ttf(SHEET_INFO *sht, UINT16 *sheet_id, char *buf, SHEET_BUFFER fc,
				   /*unsigned bc, */ unsigned x, unsigned y, int size)
{
	Rune *r = (Rune *)malloc((utflen(buf) + 1) * sizeof(Rune));
	r[utflen(buf)] = 0;
	int i = 0;
	while (*buf != '\0')
	{
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
	unsigned char *bitmap = TTF_Print(r, &width, &heigh, size);
	put_bitmap(sht, sheet_id, bitmap, x, y, width, heigh, 1920, fc);
	refresh_part_sheet(sht, x, y,
					   x + width, y + heigh);
	free(bitmap);
	free(r);
}

// void draw_text(SHEET_INFO *sht, WINDOWLS* wls, char *buf, SHEET_BUFFER fc,
// 				/*unsigned bc, */ unsigned x, unsigned y, int size)
// {
// 	print_box_ttf(sht, &(wls->w_sheet), buf, fc,
// 				  /*bc, */ x, y, size);
// }

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
		// try again
		free(ttf_buffer);
		v = path_walk(current_task->cwd_node, "/XJ380F.ttf");
		if (!v)
		{
			write_serial_string("Cannot find XJ380F.ttf\n");
			wsod_cannot_load_ttf();
			return;
		}
		ttf_buffer = (char *)malloc(v->file_size);
		v->ops->read(v, (uint8_t *)ttf_buffer, 0, v->file_size);
		if (*(uint64_t *)ttf_buffer == 0)
		{
			write_serial_string("Cannot read XJ380F.ttf\n");
			wsod_cannot_load_ttf();
			return;
		}
	}
	write_serial_string("Loading TTF...\n");
	if (!stbtt_InitFont(&font, (uint8_t *)ttf_buffer, stbtt_GetFontOffsetForIndex((uint8_t *)ttf_buffer, 0)))
	{
		write_serial_string("Cannot Initialize TTF.\n");
		wsod_cannot_load_ttf();
		return;
	}
	set_size(16);
	v->ops->close(v);

	write_serial_string("TTF Initialize Success.\n");
}
