#include "../../include/include.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_THREAD_LOCALS
#include "./stbi.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "./stbir.h"

void PrintPic_PNG(SHEET_INFO *sht, UINT16 *sheet_id, int buffer, int x, int y, int ow, int oh, char *path) {
    int w, h, bpp;
    stbi_uc *b = stbi_load(path, &w, &h, &bpp, 4);
    if (!b) {
        write_serial_string("PrintPic_PNG: File load failed");
    }
    uint8_t *b1 = (uint8_t *)(malloc(ow * oh * 4));
    // stbir_resize_uint8(b, w, h, 0, b1, ow, oh, 0, 4);
    SHEET_BUFFER_ARGB *buff = (SHEET_BUFFER_ARGB *)b1;

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
    for (int i = y; i < y + oh; i++) {
        for (int j = x; j < x + ow; j++) {
            SheetBuffer[front_p->width * (i >> 10) + (j >> 10)].r = buff[front_p->width * (i >> 10) + (j >> 10)].r;
            SheetBuffer[front_p->width * (i >> 10) + (j >> 10)].g = buff[front_p->width * (i >> 10) + (j >> 10)].g;
            SheetBuffer[front_p->width * (i >> 10) + (j >> 10)].b = buff[front_p->width * (i >> 10) + (j >> 10)].b;
            SheetBuffer[front_p->width * (i >> 10) + (j >> 10)].a = buff[front_p->width * (i >> 10) + (j >> 10)].a;
        }
    }
}
