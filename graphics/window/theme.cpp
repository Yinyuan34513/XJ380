#include "./wbuttons.h"
#include <global_color.h>
#include <graphics/window/window.h>
#include <proto.hpp>
#include <ttf.h>

/**
 * @brief 调整窗口标题栏/底部主题宽度
 * 
 * @param width 目标宽度
 * @param swidth 源宽度
 */
void resize_theme_width(SHEET_BUFFER *dst, SHEET_BUFFER *src, uint32_t width, uint32_t swidth, uint32_t height)
{
    if (width < swidth)
    {
        // TODO: 反向调整
    }
    int minus = width - swidth;
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x <= swidth / 2; x++)
        {
            dst[y * width + x] = src[y * swidth + x];
        }
        for (int x = swidth / 2 + 1; x <= width - swidth / 2; x++)
        {
            dst[y * width + x] = src[y * swidth + swidth / 2 + 1];
        }
        for (int x = width - swidth / 2 + 1; x < width; x++)
        {
            dst[y * width + x] = src[y * swidth + (x - minus)];
        }
    }
}

/**
 * @brief 调整窗口左右侧主题宽度
 * 
 * @param height 目标宽度
 * @param sheight 源宽度
 */
void resize_theme_height(SHEET_BUFFER *dst, SHEET_BUFFER *src, uint32_t width, uint32_t height, uint32_t sheight)
{
    if (height < sheight)
    {
        resize_theme_unheight(dst, src, width, height, sheight);
        return;
    }
    int minus = height - sheight;
    for (int x = 0; x < width; x++)
    {
        for (int y = 0; y <= sheight / 2; y++)
        {
            dst[y * width + x] = src[y * width + x];
        }
        for (int y = sheight / 2 + 1; y <= height - sheight / 2; y++)
        {
            dst[y * width + x] = src[y * width + width / 2 + 1];
        }
        for (int y = height - sheight / 2 + 1; y < height; y++)
        {
            dst[y * width + x] = src[y * width + (x - minus)];
        }
    }
}

void resize_theme_unheight(SHEET_BUFFER *dst, SHEET_BUFFER *src, uint32_t width, uint32_t height, uint32_t sheight)
{
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            dst[y * width + x] = src[y * width + x];
        }
    }
}