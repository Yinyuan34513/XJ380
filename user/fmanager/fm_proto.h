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

extern bool double_click;

extern int click_x;
extern int click_y;

extern char path_his[20][1024];
extern int path_p;
extern int path_r;

extern bool need_duopage;
extern int file_count_base;
extern int file_count;

void paint_fm_back();
void paint_dir(char *path);
bool get_file_type(char *name, char *type);
void register_click(int x, int y);
void cat_path(char *folder_name);
void revert_path(char *path);
void record_current_path();
