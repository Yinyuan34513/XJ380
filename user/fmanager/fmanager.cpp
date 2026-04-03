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
    case MSG_RBUTTON:
        yicixing_lock = true;
        break;
    case MSG_CRL:
        process_crl(hData);
        break;
    case MSG_CHAR:
        process_key((char)lData);
        break;
    case MSG_SPCHAR:
        if ((char)(lData) == '\b') { backspace(); }
        else if ((char)(lData) == '\n') { delete_typebox(); }
    }
    return;
}

static int fmanager_main_impl(int argc, char *argv[], char *envp[])
{
    (void)envp;

    if (argc == 1)
    {
        strcpy(current_path, argv[0]);
    }

    XWINDOW Winfo;
    Winfo.title  = (char *)"文件管理器";
    Winfo.width  = FMR_X;
    Winfo.height = FMR_Y;
    Winfo.sets = XWIN_NORMAL;
    xapi_CreateWindow(&handle, &Winfo);
    xapi_SetIcon(handle, "/system/icon/folder.png");
    SetMsgPrcor(handle, fmanager_MessagePrcor);
    memset(file_ti_index, 0, 2048 * sizeof(FILE_TYPEINFO_INDEX));

    paint_dir(current_path);

    // 初始化路径历史
    for (int i = 0; i < 20; i++) {
        memset(path_his[i], 0, 1024);
    }
    strcpy(path_his[0], "/");  // 初始路径为根目录
    path_p = 0;  // 当前位置指针
    path_r = 0;  // 最早位置指针

    xapi_RefreshWindow(handle);

    RightMenuItem rmi[3];
    rmi[0].text = (char *)"新建文件夹";
    rmi[0].CRLid = 1;
    rmi[1].text = (char *)"重命名";
    rmi[1].CRLid = 2;
    rmi[2].text = (char *)"删除";
    rmi[2].CRLid = 3;
    xapi_RegisterRightButtonMenu(handle, rmi, 3);

    while (1)
    {
        if (double_click && !register_lock)
        {
            double_click = false;
            // Double Click!
            // 搜索点到了哪个文件夹/文件
            // y_begin = 148 + 24 * index
            int click_index = (click_y - 148) / 24 + file_count_base;
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
                cat_path(fdir[click_index].filename);
                paint_dir(current_path);
            }
            else
            {
                // 是文件
                char file_path[1024];
                memset(file_path, 0, 1024);
                strcat(file_path, current_path);
                strcat(file_path, "/");
                strcat(file_path, fdir[click_index].filename);
                xapi_Run(file_path);
            }
        }
        if (need_paint && !register_lock)
        {
            paint_dir(current_path);
            need_paint = false;
        }
        __asm__ __volatile__("pause");
    }
}

extern "C" int fmanager_main_cpp(int argc, char *argv[], char *envp[]) asm("_Z4mainiPPcS0_");
extern "C" int fmanager_main_cpp(int argc, char *argv[], char *envp[])
{
    return fmanager_main_impl(argc, argv, envp);
}

int  tb_cop = -1;

void process_crl(int CRLid)
{
    /*
    1. 新建文件夹
    2. 重命名
    3. 删除
    */
    
    if (CRLid != 3) 
    { 
        tb_cop = CRLid; 
        draw_typebox(); 
    }
    else 
    {
        uint32_t count = 0;
        DirNode  fdir[256];
        memset(fdir, 0, sizeof(DirNode) * 256);
        xapi_SearchFile(current_path, &count, fdir);
        if (count == 0 || count == 404)
        {
            return;
        }
        char filepath[256];
        memset(filepath, 0, 256);
        strcat(filepath, current_path);
        strcat(filepath, "/");
        strcat(filepath, fdir[choosing_index + file_count_base].filename);

        if (fdir[choosing_index + file_count_base].filetype == 0)
            xapi_DeleteFile(filepath);
        else
            xapi_Rmdir(filepath);

        paint_dir(current_path);
    }
}

char typebox_buffer[256];
int  typebox_offset = 0;

void draw_typebox()
{
    memset(typebox_buffer, 0, 256);
    typebox_offset = 0;
    reg_lock();
    xapi_DrawRect(handle, FMR_X / 2 - 100, FMR_Y / 2 - 25, FMR_X / 2 + 100, FMR_Y / 2 + 25, 0x007accff, true);
    xapi_DrawRect(handle, FMR_X / 2 - 100 + 4, FMR_Y / 2 + 25 - 20 - 4, FMR_X / 2 + 100 - 4, FMR_Y / 2 + 25 - 4, 0xffffffff, true);
    xapi_DrawText(handle, FMR_X / 2 - 100 + 4, FMR_Y / 2 - 25, "请输入文本（回车确定）：", 11, 0xffffffff);
    xapi_RefreshWindow(handle);
}

void process_key(char ch)
{
    if (!register_lock || typebox_offset >= 189) return;

    char temp[2] = {ch, '\0'};
    xapi_DrawSWText(handle, FMR_X / 2 - 100 + 8 + typebox_offset, FMR_Y / 2 + 25 - 20 - 4, temp, 0x000000ff);
    xapi_RefreshPartWindow(handle, FMR_X / 2 - 100 + 8 + typebox_offset, FMR_Y / 2 + 25 - 20 - 4,
                            FMR_X / 2 - 100 + 8 + typebox_offset + 9, FMR_Y / 2 + 25 - 20 - 4 + 16);

    typebox_buffer[typebox_offset / 9] = ch;

    typebox_offset += 9;
}

void backspace()
{
    if (!register_lock || typebox_offset <= 0) return;

    typebox_offset -= 9;

    typebox_buffer[typebox_offset / 9] = '\0';

    xapi_DrawRect(handle, FMR_X / 2 - 100 + 8 + typebox_offset, FMR_Y / 2 + 25 - 20 - 4, 
        FMR_X / 2 - 100 + 8 + typebox_offset + 9, FMR_Y / 2 + 25 - 20 - 4 + 16, 0xffffffff, true);
    xapi_RefreshPartWindow(handle, FMR_X / 2 - 100 + 8 + typebox_offset, FMR_Y / 2 + 25 - 20 - 4,
        FMR_X / 2 - 100 + 8 + typebox_offset + 9, FMR_Y / 2 + 25 - 20 - 4 + 16);
}

void delete_typebox()
{
    if (!register_lock) return;

    typebox_buffer[typebox_offset / 9] = '\0';

    if (tb_cop == 1)
    {
        char path_name[256];
        memset(path_name, 0, 256);
        strcat(path_name, current_path);
        strcat(path_name, "/");
        strcat(path_name, typebox_buffer);
        xapi_Mkdir(path_name);
    }
    else if (tb_cop == 2)
    {
        uint32_t count = 0;
        DirNode  fdir[256];
        memset(fdir, 0, sizeof(DirNode) * 256);
        xapi_SearchFile(current_path, &count, fdir);
        if (count == 0 || count == 404)
        {
            return;
        }
        char old_path[256];
        memset(old_path, 0, 256);
        strcat(old_path, current_path);
        strcat(old_path, "/");
        strcat(old_path, fdir[choosing_index + file_count_base].filename);
        char new_path[256];
        memset(new_path, 0, 256);
        strcat(new_path, current_path);
        strcat(new_path, "/");
        strcat(new_path, typebox_buffer);
        xapi_RenameFile(old_path, new_path);
    }

    tb_cop = -1;
    reg_unlock();
    paint_dir(current_path);
}

