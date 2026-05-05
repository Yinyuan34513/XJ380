#include <x3api.h>
#include <krlibc.h>
#include "fm_proto.h"

char current_path[1024] = "/";
char path_his[20][1024];
int path_p = 0;
int path_r = 0;

HDLE handle;
int fmr_width = FMR_X;
int fmr_height = FMR_Y;

static int fmanager_dialog_x()
{
    return fmr_width / 2 - 100;
}

static int fmanager_dialog_y()
{
    return fmr_height / 2 - 25;
}

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
        break;
    case MSG_RESIZE:
        fmr_width = (int)hData;
        fmr_height = (int)lData;
        if (file_count_base + fm_visible_rows() > file_count)
        {
            file_count_base = file_count - fm_visible_rows();
            if (file_count_base < 0) file_count_base = 0;
        }
        need_paint = true;
        break;
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
    Winfo.sets = XWIN_NORMAL | XWIN_SUPPORT_RESIZEABLE;
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
            need_paint = false;

            int click_index = double_click_file_index;
            double_click_file_index = -1;
            if (file_count <= 0)
            {
                continue;
            }
            if (click_index < 0 || click_index >= file_count)
            {
                continue;
            }
            if (file_dir_cache[click_index].filetype == 1)
            {
                // 是文件夹
                record_current_path();
                cat_path(file_dir_cache[click_index].filename);
                paint_dir(current_path);
            }
            else
            {
                // 是文件
                char file_path[1024];
                memset(file_path, 0, 1024);
                strcat(file_path, current_path);
                strcat(file_path, "/");
                strcat(file_path, file_dir_cache[click_index].filename);
                xapi_Run(file_path);
            }
        }
        if (need_paint && !register_lock)
        {
            paint_dir(current_path);
            need_paint = false;
        }
        xapi_Sleep(1);
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
    const int box_x = fmanager_dialog_x();
    const int box_y = fmanager_dialog_y();
    reg_lock();
    xapi_DrawRect(handle, box_x, box_y, box_x + 200, box_y + 50, 0x007accff, true);
    xapi_DrawRect(handle, box_x + 4, box_y + 50 - 20 - 4, box_x + 200 - 4, box_y + 50 - 4, 0xffffffff, true);
    xapi_DrawText(handle, box_x + 4, box_y, "请输入文本（回车确定）：", 11, 0xffffffff);
    xapi_RefreshWindow(handle);
}

void process_key(char ch)
{
    if (!register_lock || typebox_offset >= 189) return;

    char temp[2] = {ch, '\0'};
    const int text_x = fmanager_dialog_x() + 8 + typebox_offset;
    const int text_y = fmanager_dialog_y() + 50 - 20 - 4;
    xapi_DrawSWText(handle, text_x, text_y, temp, 0x000000ff);
    xapi_RefreshPartWindow(handle, text_x, text_y, text_x + 9, text_y + 16);

    typebox_buffer[typebox_offset / 9] = ch;

    typebox_offset += 9;
}

void backspace()
{
    if (!register_lock || typebox_offset <= 0) return;

    typebox_offset -= 9;

    typebox_buffer[typebox_offset / 9] = '\0';

    const int text_x = fmanager_dialog_x() + 8 + typebox_offset;
    const int text_y = fmanager_dialog_y() + 50 - 20 - 4;
    xapi_DrawRect(handle, text_x, text_y, text_x + 9, text_y + 16, 0xffffffff, true);
    xapi_RefreshPartWindow(handle, text_x, text_y, text_x + 9, text_y + 16);
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
