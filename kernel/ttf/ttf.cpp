#define STB_TRUETYPE_IMPLEMENTATION
#include "../../lib/stb_truetype.h"

char *ttf_buffer;
stbtt_fontinfo font;
unsigned char *bitmap;
unsigned int *vram_buffer;
int w, h, i, j, s = (20);
float xpos = 2;
int y_shift = 0;
int height = 0;
int width = 0;
int b;

void TTF_Draw_Char(SHEET_INFO *sht, UINT16 *sheet_id, int x, int y, int ch, unsigned int bc, unsigned fc)
{
  int x0, y0, x1, y1;
  stbtt_GetCodepointBitmapBoxSubpixel(
      &font, ch, stbtt_ScaleForPixelHeight(&font, s),
      stbtt_ScaleForPixelHeight(&font, s), 0, 0, &x0, &y0, &x1, &y1);
  // printf("y1=%d y0=%d,x1=%d,x0=%d\n", y1, y0, x1, x0);
  bitmap = stbtt_GetCodepointBitmap(
      &font, 0, stbtt_ScaleForPixelHeight(&font, s), ch, &w, &h, 0, 0);
  unsigned int *buf1 = (uint32_t *)malloc(w * h * sizeof(unsigned int));

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

  int r = (fc >> 16) & 0xFF; // 获取红色分量
  int g = (fc >> 8) & 0xFF;  // 获取绿色分量
  int b = fc & 0xFF;

  SHEET_BUFFER *SheetBuffer = (SHEET_BUFFER *)(front_p->buffer);
  int tx = ((int)x0 + x);
  int ty = (b + y0) + y;
  for (j = 0; j < h; ++j)
  {
    for (i = 0; i < w; ++i)
    {
      
      SheetBuffer += ty*i+(j-tx);//疑似这里有问题修修吧🐟
      if (bitmap[j * w + i] != 0)
      {
        SheetBuffer->r = r;
        SheetBuffer->g = g;
        SheetBuffer->b = b;
        SheetBuffer->a = 0xFF;
        front_p->is_change = true;
      }
    }
  }
  free(buf1);
  free(bitmap);
}

void TTF_Print(SHEET_INFO *sht, UINT16 *sheet_id, char *buf)
{
  int start_x = 0;
  for (int l = 0; l < strlen(buf); l++)
  {
    TTF_Draw_Char(sht, sheet_id, xpos, y_shift, buf[l], 0x0, 0xFFFFFF);
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

void init_ttf(SHEET_INFO *sht, UINT16 *sheet_id)
{
  VNode *v;

  v = path_walk(current_task->cwd_node, "/XJ380F.ttf");
  ttf_buffer = (char *)malloc(v->file_size);
  v->ops->read(v, (uint8_t *)ttf_buffer, 0, v->file_size);
  stbtt_InitFont(&font, (uint8_t *)ttf_buffer, stbtt_GetFontOffsetForIndex((uint8_t *)ttf_buffer, 0));
  set_size(50);
  v->ops->close(v);
}