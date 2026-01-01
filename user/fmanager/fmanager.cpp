#include <x3api.h>
#include <krlibc.h>
#include "fm_proto.h"

char current_path[1024] = "/";
char path_his[20][1024];
int path_p = 0;
int path_r = 0;

HDLE handle;

void fmanager_MessagePrcor(UINT64 Type, UINT64 hData, UINT64 lData)
{
    switch (Type)
    {
    case MSG_LBUTTON:
        register_click(hData, lData);
        break;
    }
    return;
}

int main(int argc, char *argv[], char *envp[])
{
    (void)argc;
    (void)argv;
    (void)envp;

    XWINDOW Winfo;
    Winfo.title  = (char *)"文件管理器";
    Winfo.width  = FMR_X;
    Winfo.height = FMR_Y;
    xapi_CreateWindow(&handle, &Winfo);
    SetMsgPrcor(handle, fmanager_MessagePrcor);
    memset(file_ti_index, 0, 2048 * sizeof(FILE_TYPEINFO_INDEX));

    paint_fm_back();
    paint_dir(current_path);

    // 初始化路径历史
    for (int i = 0; i < 20; i++) {
        memset(path_his[i], 0, 1024);
    }
    strcpy(path_his[0], "/");  // 初始路径为根目录
    path_p = 0;  // 当前位置指针
    path_r = 0;  // 最早位置指针

    xapi_RefreshWindow(handle);

    xapi_Broken("114514");

    while (1)
    {
        if (double_click)
        {
            double_click = false;
            // Double Click!
            // 搜索点到了哪个文件夹/文件
            // y_begin = 148 + 24 * index
            int click_index = (click_y - 148) / 24;
            uint32_t count = 0;
            DirNode  fdir[256];
            memset(fdir, 0, sizeof(DirNode) * 256);
            xapi_SearchFile(current_path, &count, fdir);
            if (count == 0 || count == 404)
            {
                paint_dir(current_path);
                continue;
            }
            if (fdir[click_index].filetype == 1)
            {
                // 是文件夹
                record_current_path();
                char new_path[1024];
                cat_path(fdir[click_index].filename);
                paint_dir(current_path);
            }
        }
        __asm__ __volatile__("pause");
    }
}


