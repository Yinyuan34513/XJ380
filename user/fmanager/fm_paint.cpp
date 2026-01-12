#include <x3api.h>
#include <krlibc.h>
#include "fm_proto.h"

char path[1024] = "/";

FILE_TYPEINFO_INDEX file_ti_index[2048];

bool need_duopage = false;
int  file_count_base = 0;
int  file_count = 0;

void paint_fm_back()
{
    xapi_DrawRect(handle, 0, 0 , FMR_X - 1, 25, 0xeaeaeaff, true);
    xapi_DrawRect(handle, 52, 4, FMR_X - 5, 22, 0xffffffff, true);
    xapi_DrawPNG(handle, 0, 26, FMR_X - 1, 94, "/system/image/fm_bg.png");
    xapi_DrawPNG(handle, 5, 4, 18, 18, "/system/icon/bleft.png");
    xapi_DrawPNG(handle, 29, 4, 18, 18, "/system/icon/bright.png");
    xapi_DrawText(handle, 56, 2, current_path, 11, 0x000000ff);

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
            }
        }
        else 
        {
            xapi_DrawText(handle, 240, 148 + 24 * index - 3, "未知类型文件", 10, 0x6d6d6dff);
        }
        xapi_DrawPNG(handle, 8, 148 + 24 * index, 16, 16, "/system/icon/unknowfile.png");
type_is_found:
        char file_size_str[32];
        memset(file_size_str, 0, sizeof(char) * 32);
        strcpy(file_size_str, xcr_int2char((size + 1023) / 1024));
        strcat(file_size_str, " KB");
        xapi_DrawText(handle, 400, 148 + 24 * index - 3, file_size_str, 10, 0x6d6d6dff);
    }
}

void paint_dir(char *path)
{
    uint32_t count = 0;
    DirNode  fdir[256];
    memset(fdir, 0, sizeof(DirNode) * 256);
    xapi_DrawRect(handle, 0, 0, FMR_X - 1, FMR_Y - 1, 0xffffffff, true);
    xapi_SearchFile(path, &count, fdir);

    if (count > 12)
    {
        need_duopage = true;
        file_count = count;
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
            fdir[i].filename, 
            fdir[i].filetype, 
            fdir[i].length, 
            (choosing_index == i - file_count_base));
    }
    xapi_RefreshWindow(handle);
}
