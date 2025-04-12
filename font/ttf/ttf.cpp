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
size_t wcslen(const wchar_t *str)
{
	if (str == nullptr)
		return 0; // 处理空指针（可选，标准库的 wcslen 不检查）

	const wchar_t *p = str;
	while (*p != L'\0')
	{ // 遍历直到遇到宽空字符
		p++;
	}
	return p - str; // 指针差值即为长度
}
void TTF_Print(SHEET_INFO *sht, UINT16 *sheet_id, int x, int y, wchar_t *buf, const SHEET_BUFFER &color,int font_size)
{
	int code_point = 0;
	for (const wchar_t *ptr = buf; *ptr != L'\0'; ++ptr)
	{
		code_point = 0;
		code_point = (int)(*ptr);
		if (code_point <= 0x007F&&code_point>0x001F)
		{
			set_size(font_size+5);
			TTF_Draw_Char(sht, sheet_id, x, y+(font_size/5-5), code_point, color);
			x += width;
		}
		else if(code_point>=0x0041&&code_point<=0x005A)
		{
			set_size(font_size);
			TTF_Draw_Char(sht, sheet_id, x, y, code_point, color);
			x += width+(font_size/3);
		}
		else if(code_point>=0x4E00&&code_point<=0x9FFF)
		{
			set_size(font_size);
			TTF_Draw_Char(sht, sheet_id, x, y, code_point, color);
			x += width+(font_size/3+3);
		}
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
	set_size(40);
	v->ops->close(v);
	free(ttf_buffer);

	write_serial_string("TTF Initialize Success.\n");
}
