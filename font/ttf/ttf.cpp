#define STB_TRUETYPE_IMPLEMENTATION
#include "./stb_ttf.h"

char *ttf_buffer;
stbtt_fontinfo font;
unsigned char *bitmap;
unsigned int *vram_buffer;
int w, h, s = (20);
float xpos = 2;
int y_shift = 0;
int height = 0;
int width = 0;
int b;
inline static size_t utf(uint32_t src, uint8_t *des)
{
	if (src == 0)
		return 0;
	static const int PREFIX[] = {0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC};
	static const uint32_t CODE_UP[] =
		{
			0x80,	   // U+00000000 - U+0000007F
			0x800,	   // U+00000080 - U+000007FF
			0x10000,   // U+00000800 - U+0000FFFF
			0x200000,  // U+00010000 - U+001FFFFF
			0x4000000, // U+00200000 - U+03FFFFFF
			0x80000000 // U+04000000 - U+7FFFFFFF
		};
	size_t i, len = sizeof(CODE_UP) / sizeof(uint32_t);
	for (i = 0; i < len; ++i)
		if (src < CODE_UP[i])
			break;
	if (i == len)
		return 0; // the src is invalid
	len = i + 1;
	if (des)
	{
		for (; i > 0; --i)
		{
			des[i] = static_cast<uint8_t>((src & 0x3F) | 0x80);
			src >>= 6;
		}
		des[0] = static_cast<uint8_t>(src | PREFIX[len - 1]);
	}
	return len;
}
/*
UTF-8 to UTF-32
*/
inline static size_t utf(const uint8_t *src, uint32_t &des)
{
	if (!src || (*src) == 0)
		return 0;
	uint8_t b = *(src++);
	if (b < 0x80)
	{
		des = b;
		return 1;
	}
	if (b < 0xC0 || b > 0xFD)
		return 0; // the src is invalid
	size_t len;
	if (b < 0xE0)
	{
		des = b & 0x1F;
		len = 2;
	}
	else if (b < 0xF0)
	{
		des = b & 0x0F;
		len = 3;
	}
	else if (b < 0xF8)
	{
		des = b & 0x07;
		len = 4;
	}
	else if (b < 0xFC)
	{
		des = b & 0x03;
		len = 5;
	}
	else
	{
		des = b & 0x01;
		len = 6;
	}
	size_t i = 1;
	for (; i < len; ++i)
	{
		b = *(src++);
		if (b < 0x80 || b > 0xBF)
			return 0; // the src is invalid
		des = (des << 6) + (b & 0x3F);
	}
	return len;
}
/*
UTF-32 to UTF-16
*/
inline static size_t utf(uint32_t src, uint16_t *des)
{
	if (src == 0)
		return 0;
	if (src <= 0xFFFF)
	{
		if (des)
			(*des) = static_cast<uint16_t>(src);
		return 1;
	}
	else if (src <= 0xEFFFF)
	{
		if (des)
		{
			des[0] = static_cast<uint16_t>(0xD800 + (src >> 10) - 0x40); // high
			des[1] = static_cast<uint16_t>(0xDC00 + (src & 0x03FF));	 // low
		}
		return 2;
	}
	return 0;
}
/*
UTF-16 to UTF-32
*/
inline static size_t utf(const uint16_t *src, uint32_t &des)
{
	if (!src || (*src) == 0)
		return 0;
	uint16_t w1 = src[0];
	if (w1 >= 0xD800 && w1 <= 0xDFFF)
	{
		if (w1 < 0xDC00)
		{
			uint16_t w2 = src[1];
			if (w2 >= 0xDC00 && w2 <= 0xDFFF)
			{
				des = (w2 & 0x03FF) + (((w1 & 0x03FF) + 0x40) << 10);
				return 2;
			}
		}
		return 0; // the src is invalid
	}
	else
	{
		des = w1;
		return 1;
	}
}
/*
UTF-16 to UTF-8
*/
inline static size_t utf(uint16_t src, uint8_t *des)
{
	// make utf-16 to utf-32
	uint32_t tmp;
	if (utf(&src, tmp) != 1)
		return 0;
	// make utf-32 to utf-8
	return utf(tmp, des);
}
/*
UTF-8 to UTF-16
*/
inline static size_t utf(const uint8_t *src, uint16_t &des)
{
	// make utf-8 to utf-32
	uint32_t tmp;
	size_t len = utf(src, tmp);
	if (len == 0)
		return 0;
	// make utf-32 to utf-16
	if (utf(tmp, &des) != 1)
		return 0;
	return len;
}
void TTF_Draw_Char(SHEET_INFO *sht, UINT16 *sheet_id, int x, int y, int ch, const SHEET_BUFFER &color)
{
	int x0, y0, x1, y1;
	stbtt_GetCodepointBitmapBoxSubpixel(
		&font, ch, stbtt_ScaleForPixelHeight(&font, s),
		stbtt_ScaleForPixelHeight(&font, s), 0, 0, &x0, &y0, &x1, &y1);
	bitmap = stbtt_GetCodepointBitmap(
		&font, 0, stbtt_ScaleForPixelHeight(&font, s), ch, &w, &h, 0, 0);

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
	for (int j = 0; j < h; ++j)
	{
		for (int i = 0; i < w; ++i)
		{
			if (bitmap[j * w + i] != 0)
			{
				SheetBuffer[(j + y) * sht->scrx + (i + x)].r = color.r;
				SheetBuffer[(j + y) * sht->scrx + (i + x)].g = color.g;
				SheetBuffer[(j + y) * sht->scrx + (i + x)].b = color.b;
				SheetBuffer[(j + y) * sht->scrx + (i + x)].a = color.a;
			}
		}
	}
	free(bitmap);
}

/*
UTF-X: string to string
*/
template <typename T>
size_t utf(const uint32_t *src, T *des) // UTF-32 to UTF-X(8/16)
{
	if (!src || (*src) == 0)
		return 0;
	size_t num = 0;
	for (; *src; ++src)
	{
		size_t len = utf(*src, des);
		if (len == 0)
			break;
		if (des)
			des += len;
		num += len;
	}
	if (des)
		(*des) = 0;
	return num;
}
template <typename T>
size_t utf(const T *src, uint32_t *des) // UTF-X(8/16) to UTF-32
{
	if (!src || (*src) == 0)
		return 0;
	size_t num = 0;
	while (*src)
	{
		uint32_t tmp;
		size_t len = utf(src, tmp);
		if (len == 0)
			break;
		if (des)
		{
			(*des) = tmp;
			++des;
		}
		src += len;
		num += 1;
	}
	if (des)
		(*des) = 0;
	return num;
}
template <typename T, typename U>
size_t utf(const T *src, U *des) // UTF-X(8/16) to UTF-Y(16/8)
{
	if (!src || (*src) == 0)
		return 0;
	size_t num = 0;
	while (*src)
	{
		// make utf-x to ucs4
		uint32_t tmp;
		size_t len = utf(src, tmp);
		if (len == 0)
			break;
		src += len;
		// make ucs4 to utf-y
		len = utf(tmp, des);
		if (len == 0)
			break;
		if (des)
			des += len;
		num += len;
	}
	if (des)
		(*des) = 0;
	return num;
}

void TTF_Print(SHEET_INFO *sht, UINT16 *sheet_id, int x, int y, char *buf, const SHEET_BUFFER &color)
{
	int start_x = 0;
	for (int l = 0; l < strlen(buf); l++)
	{
		TTF_Draw_Char(sht, sheet_id, x, y, buf[l], color);
		x += width;
	}
}

void set_size(int s1)
{
	float sc;
	s = s1;
	sc = stbtt_ScaleForPixelHeight(&font, s);
	int advance, lsb;
	stbtt_GetCodepointHMetrics(&font, 'A', &advance, &lsb);
	width = advance * sc + stbtt_GetCodepointKernAdvance(&font, 'A', 'B') * sc;
	int as, lg, des;
	stbtt_GetFontVMetrics(&font, &as, &des, &lg);
	b = (int)((float)as * sc);
	height = (int)((float)(as - des + lg) * sc);
}

void init_ttf(SHEET_INFO *sht, UINT16 *sheet_id)
{
	write_serial_string("Initializing TTF...\n");

	VNode *v = path_walk(current_task->cwd_node, "/XJ380F.ttf");
	if (!v)
	{
		write_serial_string("Cannot find XJ380F.ttf");
		return;
	}
	ttf_buffer = (char *)malloc(v->file_size);
	write_serial_string("Reading TTF...\n");
	v->ops->read(v, (uint8_t *)ttf_buffer, 0, v->file_size);
	write_serial_string("Loading TTF...\n");
	stbtt_InitFont(&font, (uint8_t *)ttf_buffer, stbtt_GetFontOffsetForIndex((uint8_t *)ttf_buffer, 0));
	set_size(50);
	v->ops->close(v);
	free(ttf_buffer);

	write_serial_string("TTF Initialize Success.\n");
}