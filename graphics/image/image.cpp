#include <proto.hpp>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_THREAD_LOCALS
#include "./stbi.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "./stbir.h"

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

void PrintPicture_blend(SHEET_INFO *sht, uint16_t *sheet_id, int x, int y, int ow, int oh, char* path) {
    int w, h, bpp;
    stbi_uc *b = stbi_load(path, &w, &h, &bpp, 4);
    if (!b) {
        write_serial_string("PrintPic_PNG: File load failed\n");
        return;
    }
    uint8_t *b1 = (uint8_t *)(malloc(ow * oh * 4));
    stbir_resize_uint8(b, w, h, 0, b1, ow, oh, 0, 4);
    SHEET_BUFFER *buff = (SHEET_BUFFER *)b1;

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
    for (int i = 0; i < oh; i++) {
        for (int j = 0; j < ow; j++) {
            SheetBuffer[(i + y) * front_p->width + (j + x)] = LCD_AlphaBlend(
                buff[i * ow + j],
                SheetBuffer[(i + y) * front_p->width + (j + x)],
                buff[i * ow + j].a
            );
        }
    }
    free(b1);
}

void PrintPicture(SHEET_INFO *sht, uint16_t *sheet_id, int x, int y, int ow, int oh, char* path) {
    int w, h, bpp;
    stbi_uc *b = stbi_load(path, &w, &h, &bpp, 4);
    if (!b) {
        write_serial_string("PrintPic_PNG: File load failed\n");
        return;
    }
    uint8_t *b1 = (uint8_t *)(malloc(ow * oh * 4));
    // stbir_resize_uint8(b, w, h, 0, b1, ow, oh, 0, 4);
    SHEET_BUFFER *buff = (SHEET_BUFFER *)b1;

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
    for (int i = 0; i < oh; i++) {
        for (int j = 0; j < ow; j++) {
            SheetBuffer[(i + y) * front_p->width + (j + x)] = buff[i * ow + j];
        }
    }
    free(b1);
}

void LoadPicture(SHEET_BUFFER *buffer, int ow, int oh, char* path) {
    int w, h, bpp;
    stbi_uc *b = stbi_load(path, &w, &h, &bpp, 4);
    if (!b) {
        write_serial_string("PrintPic_PNG: File load failed\n");
    }
    stbir_resize_uint8(b, w, h, 0, (uint8_t *)(buffer), ow, oh, 0, 4);
}
