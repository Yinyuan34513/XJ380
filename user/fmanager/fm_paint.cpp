#include <x3api.h>
#include <krlibc.h>
#include "fm_proto.h"

char path[1024] = "/";

FILE_TYPEINFO_INDEX file_ti_index[2048];
DirNode file_dir_cache[256];

bool need_duopage = false;
int  file_count_base = 0;
int  file_count = 0;

/*
static const char *xapi_demoSvgCheck =
    "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 448 512\">"
    "<!--!Font Awesome Free v7.2.0 by @fontawesome - https://fontawesome.com License - "
    "https://fontawesome.com/license/free Copyright 2026 Fonticons, Inc.-->"
    "<path d=\"M434.8 70.1c14.3 10.4 17.5 30.4 7.1 44.7l-256 352c-5.5 7.6-14 12.3-23.4 13.1s-18.5-2.7-25.1-9.3l-128-128c-12.5-12.5-12.5-32.8 0-45.3s32.8-12.5 45.3 0l101.5 101.5 234-321.7c10.4-14.3 30.4-17.5 44.7-7.1z\"/>"
    "</svg>";*/

static bool visible_index_is_valid(int visible_index)
{
    int visible_count = file_count - file_count_base;
    if (visible_count > 12)
    {
        visible_count = 12;
    }

    return visible_index >= 0 && visible_index < visible_count;
}

void refresh_fm_part(int x1, int y1, int x2, int y2)
{
    xapi_RefreshPartWindow(handle, x1 + 12, y1 + 27, x2 + 12, y2 + 27);
}

void paint_fm_back()
{
    xapi_DrawRect(handle, 0, 0 , FMR_X - 1, 25, 0xeaeaeaff, true);
    xapi_DrawRect(handle, 52, 4, FMR_X - 5, 22, 0xffffffff, true);
    xapi_DrawPNG(handle, 0, 26, FMR_X - 1, 94, "/system/image/fm_bg.png");
    xapi_DrawPNG(handle, 5, 4, 18, 18, "/system/icon/bleft.png");
    xapi_DrawPNG(handle, 29, 4, 18, 18, "/system/icon/bright.png");
    xapi_DrawText(handle, 56, 2, current_path, 11, 0x000000ff);
    // xapi_drawSvg(handle, FMR_X - 40, 30, 28, (char *)xapi_demoSvgCheck);
    if (need_duopage)
    {
        xapi_DrawPNG(handle, FMR_X - 19, 148, 18, 18, "/system/icon/bup.png");
        xapi_DrawPNG(handle, FMR_X - 19, 170, 18, 18, "/system/icon/bdown.png");
    }
}

void paint_item(int index, char *name, bool is_folder, int size, bool is_choosing)
{
    xapi_DrawRect(handle, 4, 148 + 24 * index - 4, FMR_X / 10 * 8, 148 + 24 * index + 24 - 4, 0xffffffff, true);
    if (is_choosing) xapi_DrawRect(handle, 4, 148 + 24 * index - 4, FMR_X / 10 * 8, 148 + 24 * index + 24 - 4, 0xe5f3ffff, true);

    xapi_DrawText(handle, 28, 148 + 24 * index - 3, name, 10, 0x000000ff);
    if (is_folder)
    {
        xapi_DrawPNG(handle, 8, 148 + 24 * index, 16, 16, "/system/icon/folder.png");
        xapi_DrawText(handle, 240, 148 + 24 * index - 3, "文件夹", 10, 0x6d6d6dff);
    }
    else
    {
        char file_type[32];
        memset(file_type, 0, sizeof(char) * 32);
        if (get_file_type(name, file_type))
        {
            if (strcmp("elf", file_type) == 0 || strcmp("epf", file_type) == 0)
            {
                xapi_DrawText(handle, 240, 148 + 24 * index - 3, "可执行文件", 10, 0x6d6d6dff);
                xapi_DrawPNG(handle, 8, 148 + 24 * index, 16, 16, "/system/icon/unknowexe.png");
            }
            else if (strcmp("txt", file_type) == 0)
            {
                xapi_DrawText(handle, 240, 148 + 24 * index - 3, "文本文件", 10, 0x6d6d6dff);
                xapi_DrawPNG(handle, 8, 148 + 24 * index, 16, 16, "/system/icon/texter.png");
            }
            else if (strcmp("png", file_type) == 0)
            {
                xapi_DrawText(handle, 240, 148 + 24 * index - 3, "PNG 图片文件", 10, 0x6d6d6dff);
                xapi_DrawPNG(handle, 8, 148 + 24 * index, 16, 16, "/system/icon/unknowfile.png");
            }
            else if (strcmp("jpg", file_type) == 0)
            {
                xapi_DrawText(handle, 240, 148 + 24 * index - 3, "JPEG 图片文件", 10, 0x6d6d6dff);
                xapi_DrawPNG(handle, 8, 148 + 24 * index, 16, 16, "/system/icon/unknowfile.png");
            }
            else if (strcmp("bmp", file_type) == 0)
            {
                xapi_DrawText(handle, 240, 148 + 24 * index - 3, "位图文件", 10, 0x6d6d6dff);
                xapi_DrawPNG(handle, 8, 148 + 24 * index, 16, 16, "/system/icon/unknowfile.png");
            }
            else
            {
                for (int i = 0; file_ti_index[i].file_type[0] != '\0'; i++)
                {
                    if (strcmp(file_type, file_ti_index[i].file_type) == 0)
                    {
                        // xapi_DrawPNG(handle, 8, 148 + 24 * index, 16, 16, "/system/icon/folder.png");
                        xapi_DrawText(handle, 240, 148 + 24 * index - 3, file_ti_index[i].file_type_name, 10, 0x6d6d6dff);
                        goto type_is_found;
                    }
                }
                strcat(file_type, " 文件");
                xapi_DrawText(handle, 240, 148 + 24 * index - 3, file_type, 10, 0x6d6d6dff);
                xapi_DrawPNG(handle, 8, 148 + 24 * index, 16, 16, "/system/icon/unknowfile.png");
            }
        }
        else 
        {
            xapi_DrawText(handle, 240, 148 + 24 * index - 3, "未知类型文件", 10, 0x6d6d6dff);
            xapi_DrawPNG(handle, 8, 148 + 24 * index, 16, 16, "/system/icon/unknowfile.png");
        }
type_is_found:
        char file_size_str[32];
        memset(file_size_str, 0, sizeof(char) * 32);
        strcpy(file_size_str, xcr_int2char((size + 1023) / 1024));
        strcat(file_size_str, " KB");
        xapi_DrawText(handle, 400, 148 + 24 * index - 3, file_size_str, 10, 0x6d6d6dff);
    }
}

static void refresh_visible_item(int visible_index)
{
    if (!visible_index_is_valid(visible_index))
    {
        return;
    }

    int file_index = file_count_base + visible_index;
    paint_item(visible_index,
        file_dir_cache[file_index].filename,
        file_dir_cache[file_index].filetype,
        file_dir_cache[file_index].length,
        (choosing_index == visible_index));
    refresh_fm_part(4, 148 + 24 * visible_index - 4, FMR_X - 1, 148 + 24 * visible_index + 20);
}

void refresh_choose_change(int old_visible_index, int new_visible_index)
{
    if (register_lock)
    {
        return;
    }

    reg_lock();
    refresh_visible_item(old_visible_index);
    if (new_visible_index != old_visible_index)
    {
        refresh_visible_item(new_visible_index);
    }
    reg_unlock();
}

void paint_dir(char *path)
{
    if (register_lock) return;
    reg_lock();

    uint32_t count = 0;
    memset(file_dir_cache, 0, sizeof(DirNode) * 256);
    xapi_DrawRect(handle, 0, 0, FMR_X - 1, FMR_Y - 1, 0xffffffff, true);
    xapi_SearchFile(path, &count, file_dir_cache);
    file_count = (count == 404) ? 0 : (int)count;

    if (count > 12)
    {
        need_duopage = true;
    }
    else 
    {
        need_duopage = false;
        file_count_base = 0;
    }

    paint_fm_back();
    
    xapi_DrawLine(handle, 0, 141, FMR_X - 1, 141, 0xc4c4c4ff);
    xapi_DrawText(handle, 16, 120, "文件名称", 10, 0x000000ff);
    xapi_DrawText(handle, 240, 120, "类型", 10, 0x000000ff);
    xapi_DrawText(handle, 400, 120, "大小", 10, 0x000000ff);
    if (count == 0 || count == 404) 
    {
        // 不知道为什么居中会看起来偏右，那就往左偏一点吧（视觉错误这一块）
        xapi_DrawText(handle, FMR_X / 2 - 32, 154, "空文件夹", 10, 0x6d6d6dff);
    }

    for (int i = file_count_base; i < ((count > 12 + file_count_base) ? 12 + file_count_base : count); i++)
    {
        paint_item(i - file_count_base, 
            file_dir_cache[i].filename, 
            file_dir_cache[i].filetype, 
            file_dir_cache[i].length, 
            (choosing_index == i - file_count_base));
    }
    refresh_fm_part(0, 0, FMR_X - 1, FMR_Y - 1);

    reg_unlock();
}
