#pragma once

// 文件管理器窗口尺寸
#define FMR_X 800
#define FMR_Y 450

struct FILE_TYPEINFO_INDEX
{
    char file_type[32];
    char icon_path[64];
    char file_type_name[64];
};

extern HDLE handle;
extern char current_path[1024];
extern FILE_TYPEINFO_INDEX file_ti_index[2048];

void paint_fm_back();
void paint_dir(char *path);
bool get_file_type(char *name, char *type);
void register_click(int x, int y);
