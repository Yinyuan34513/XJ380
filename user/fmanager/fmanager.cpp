// file_manager.cpp
#include "../shell/convert.h"
#include "../xapi/include/krlibc.h"
#include "../shell/st_proto.h"
#include "../xapi/include/x3api.h"
void handle_mouse_click(UINT64 x, UINT64 y);
void go_parent_directory();
void refresh_file_list();
void enter_directory(char *dir_name);
// 文件管理器窗口尺寸
#define FM_WIN_WIDTH 800
#define FM_WIN_HEIGHT 600

// 文件列表起始位置
#define LIST_START_X 20
#define LIST_START_Y 50
#define ITEM_HEIGHT 20
#define ITEM_SPACING 5

HDLE fm_handle;
char fm_current_path[512] = "/";
int fm_selected_index = -1;

// 文件管理器消息处理函数
void fm_MessagePrcor(UINT64 Type, UINT64 hData, UINT64 lData)
{
    switch (Type)
    {
    case 100:
        handle_mouse_click(hData, lData);
        break;
    case MSG_CHAR:
        if ((char)lData == '\b') {
            // 退格键返回上一级
            go_parent_directory();
        }
        break;
    }
}

// 主函数
int main(int argc, char *argv[], char *envp[])
{
    (void)argc;
    (void)argv;
    (void)envp;

    // 创建文件管理器窗口
    XWINDOW Winfo;
    Winfo.title = (char *)"文件管理器";
    Winfo.width = FM_WIN_WIDTH;
    Winfo.height = FM_WIN_HEIGHT;
    xapi_CreateWindow(&fm_handle, &Winfo);
    SetMsgPrcor(fm_handle, fm_MessagePrcor);

    // 初始绘制
    refresh_file_list();
    
    while (1)
    {
        __asm__ __volatile__("pause");
    }
    return 0;
}

// 刷新文件列表显示
void refresh_file_list()
{
    // 清空窗口
    xapi_DrawRect(fm_handle, 0, 0, FM_WIN_WIDTH - 1, FM_WIN_HEIGHT - 1, 0xFFFFFFFF, true);
    
    // 显示当前路径
    char path_display[256];
    strcpy(path_display, "路径: ");
    strcat(path_display, fm_current_path);
    xapi_DrawText(fm_handle, 20, 20, path_display, 14, 0x000000FF);
    
    // 显示操作提示
    xapi_DrawText(fm_handle, 20, FM_WIN_HEIGHT - 30, 
                 "点击文件夹进入，按退格键返回上一级", 12, 0x666666FF);
    
    // 获取目录内容
    uint32_t count = 0;
    DirNode fdir[256];
    xapi_SearchFile(fm_current_path, &count, fdir);
    
    if (count == 0 || count == 404)
    {
        xapi_DrawText(fm_handle, LIST_START_X, LIST_START_Y, "空目录", 14, 0x000000FF);
        xapi_RefreshWindow(fm_handle);
        return;
    }
    
    // 显示文件列表
    int y_pos = LIST_START_Y;
    
    // 显示返回上一级项（如果不是根目录）
    if (strcmp(fm_current_path, "/") != 0)
    {
        xapi_DrawText(fm_handle, LIST_START_X, y_pos, "[..] 返回上一级", 14, 0x0000FFFF);
        y_pos += ITEM_HEIGHT + ITEM_SPACING;
    }
    
    // 显示文件和文件夹
    for (int i = 0; i < count; i++)
    {
        char item_text[256];
        
        if (fdir[i].filetype == 1)
        {
            // 文件夹
            strcpy(item_text, "[文件夹] ");
            strcat(item_text, fdir[i].filename);
            xapi_DrawText(fm_handle, LIST_START_X, y_pos, item_text, 14, 0x0000FFFF);
        }
        else
        {
            // 文件
            strcpy(item_text, fdir[i].filename);
            char size_str[32];
            strcat(item_text, " (");
            strcat(item_text, xcr_int2char(fdir[i].length));
            strcat(item_text, " 字节)");
            xapi_DrawText(fm_handle, LIST_START_X, y_pos, item_text, 14, 0x000000FF);
        }
        
        y_pos += ITEM_HEIGHT + ITEM_SPACING;
        
        // 如果超出窗口范围，停止显示
        if (y_pos > FM_WIN_HEIGHT - 50) break;
    }
    
    xapi_RefreshWindow(fm_handle);
}

// 处理鼠标点击
void handle_mouse_click(UINT64 rel_x, UINT64 rel_y)
{
    // rel_x 和 rel_y 现在是窗口客户区内的相对坐标
    int click_x = (int)rel_x;
    int click_y = (int)rel_y;
    
    // 检查是否点击在文件列表区域
    if (click_x < LIST_START_X || click_y < LIST_START_Y) return;
    
    // 计算点击的项目索引
    int start_y = LIST_START_Y;
    int item_index = 0;
    
    // 获取目录内容
    uint32_t count = 0;
    DirNode fdir[256];
    xapi_SearchFile(fm_current_path, &count, fdir);
    
    // 调整起始位置（考虑返回上一级项）
    bool has_parent = (strcmp(fm_current_path, "/") != 0);
    if (has_parent)
    {
        if (click_y >= start_y && click_y < start_y + ITEM_HEIGHT)
        {
            // 点击了返回上一级
            go_parent_directory();
            return;
        }
        start_y += ITEM_HEIGHT + ITEM_SPACING;
    }
    
    // 计算点击的实际文件/文件夹索引
    item_index = (click_y - start_y) / (ITEM_HEIGHT + ITEM_SPACING);
    
    if (item_index >= 0 && item_index < count)
    {
        if (fdir[item_index].filetype == 1)
        {
            // 进入文件夹
            enter_directory(fdir[item_index].filename);
        }
        else
        {
            // 选中文件
            fm_selected_index = item_index;
            // 可以在这里添加选中效果
            char select_msg[256];
            strcpy(select_msg, "选中文件: ");
            strcat(select_msg, fdir[item_index].filename);
            xapi_DrawText(fm_handle, 20, FM_WIN_HEIGHT - 50, select_msg, 12, 0xFF0000FF);
            xapi_RefreshWindow(fm_handle);
        }
    }
    
    // 调试信息：显示点击坐标
    char debug_info[64];
    strcpy(debug_info, "点击位置: ");
    strcat(debug_info, xcr_int2char(click_x));
    strcat(debug_info, ", ");
    strcat(debug_info, xcr_int2char(click_y));
    xapi_DrawText(fm_handle, 400, 20, debug_info, 12, 0xFF0000FF);
    xapi_RefreshWindow(fm_handle);
}
// 进入目录
void enter_directory(char *dir_name)
{
    if (strcmp(fm_current_path, "/") == 0)
    {
        strcpy(fm_current_path, "/");
        strcat(fm_current_path, dir_name);
    }
    else
    {
        strcat(fm_current_path, "/");
        strcat(fm_current_path, dir_name);
    }
    refresh_file_list();
}

// 返回上一级目录
void go_parent_directory()
{
    if (strcmp(fm_current_path, "/") == 0) return; // 已经是根目录
    
    char *last_slash = strrchr(fm_current_path, '/');
    if (last_slash != NULL)
    {
        if (last_slash == fm_current_path) // 如果是根目录下的直接子目录
        {
            fm_current_path[1] = '\0'; // 保持为 "/"
        }
        else
        {
            *last_slash = '\0'; // 截断到上一级目录
        }
    }
    refresh_file_list();
}