#define STB_TRUETYPE_IMPLEMENTATION
#include "../../lib/stb_truetype.h"

char *ttf_buffer;
#define argb(a, r, g, b) ((a) << 24 | (r) << 16 | (g) << 8 | (b))
uint32_t LCD_AlphaBlend(uint32_t foreground_color, uint32_t background_color,
                        uint8_t alpha)
{
  if (alpha != 0xff)
  {
    return 0;
  }
  return foreground_color;
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
stbtt_fontinfo font;
unsigned char *bitmap;
unsigned int *vram_buffer;
int w, h, i, j, s = (20);
float xpos = 500;
int y_shift = 500;
int height = 0;
int width = 0;
int b;

void WriteAscii(const FrameBufferConfig &fbc, int x, int y, const PixelColor &c, int w, int h, uint32_t *font)
{
  static char pixel_writer_buf[sizeof(RGBRWriter) > sizeof(BGRRWriter) ? sizeof(RGBRWriter) : sizeof(BGRRWriter)];
  static PixelWriter *pixel_writer = nullptr;

  switch (fbc.pixel_format)
  {
  case PixelFormat::kRGBR:
    pixel_writer = new (pixel_writer_buf) RGBRWriter(fbc); // 使用 placement new
    break;
  case PixelFormat::kBGRR:
    pixel_writer = new (pixel_writer_buf) BGRRWriter(fbc);
    break;
  default:
    break;
  }

  for (int dy = 0; dy < w; dy++)
  { // 第几行
    for (int dx = 0; dx < h; dx++)
    { // 第几列
      if ((font[dy] << dx) & 0x80u)
      {                                         // 判断第dx位是否为1
        pixel_writer->Write(x + dx, y + dy, c); // 在x和y的基础上，偏移dx,dy绘制像素
      }
    }
  }
}
void TTF_Draw_Char(SHEET_INFO *sht, UINT16 *sheet_id, const FrameBufferConfig &fbc, int x, int y, int ch, unsigned int bc, unsigned fc)
{
  int x0, y0, x1, y1;
  stbtt_GetCodepointBitmapBoxSubpixel(
      &font, ch, stbtt_ScaleForPixelHeight(&font, s),
      stbtt_ScaleForPixelHeight(&font, s), 0, 0, &x0, &y0, &x1, &y1);
  // printf("y1=%d y0=%d,x1=%d,x0=%d\n", y1, y0, x1, x0);
  bitmap = stbtt_GetCodepointBitmap(
      &font, 0, stbtt_ScaleForPixelHeight(&font, s), ch, &w, &h, 0, 0);
  unsigned int *buf1 = (uint32_t *)malloc(w * h * sizeof(unsigned int));

  int r = (fc >> 16) & 0xFF; // 获取红色分量
  int g = (fc >> 8) & 0xFF;  // 获取绿色分量
  int b = fc & 0xFF;
  for (j = 0; j < h; ++j)
  {
    for (i = 0; i < w; ++i)
    {
        //todo:
    }
    free(buf1);
    free(bitmap);
  }
}
  void TTF_Print(SHEET_INFO * sht, UINT16 * sheet_id, const FrameBufferConfig &fbc, char *buf)
  {
    int start_x = 0;
    for (int l = 0; l < strlen(buf); l++)
    {
      TTF_Draw_Char(sht, sheet_id, fbc, xpos, y_shift, buf[l], 0x0, 0xFFFFFF);
      xpos += width;
      // y_shift += height;
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

  void init_ttf(SHEET_INFO * sht, UINT16 * sheet_id, const FrameBufferConfig &fbc)
  {
    VNode *v;

    v = path_walk(current_task->cwd_node, "/usr/XJ380F.ttf");
    ttf_buffer = (char *)malloc(v->file_size);
    v->ops->read(v, (uint8_t *)ttf_buffer, 0, v->file_size);
    stbtt_InitFont(&font, (uint8_t *)ttf_buffer, stbtt_GetFontOffsetForIndex((uint8_t *)ttf_buffer, 0));
    set_size(50);
    TTF_Print(sht, sheet_id, fbc, "TTF");
    refresh_sheet(sht);
    v->ops->close(v);
  }