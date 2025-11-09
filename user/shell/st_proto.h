#pragma once

#include "../xapi/include/x3api.h"
extern HDLE handle;
extern int  cur_x;
extern int  cur_y;
extern int  ab_hsf;
extern int *ab_x;
extern int *ab_y;

extern char char_buffer[64];
extern int  p_chbuffer;

extern char input_buf[1024];
extern int  p_inbuf;

extern char current_path[512];

extern bool front_line_is_arrow;
extern bool command_ck_lock;
extern bool putchar_lock;
extern bool paint_cursor_lock;

#define TML_X 720
#define TML_Y 405

#define BACKGROUND_COLOR    0xffffffff
#define TERMINAL_TEXT_COLOR 0x000000ff

void check_command();
void print_dir();

void print_to_console(char *str);
void print_to_console_zh(char *str);
void putchar_at_cur(char *ch);
void paint_arrow_sign(int sx, int sy);
void flush_buffer();
void newline();
void scroll();

void backspace();

void merge_string(char *dst, char *src);
void revert_path(char *path);
int  check_dir_com(char *str);

void create_directory(char *path);
void change_directory(char *path);
void normalize_path(char *path);
bool create_directory_recursive(char *path); // 递归创建函数
void print_file_content(char *filename);
void remove_file_or_directory(char *path);
void create_file(char *filename);
void copy_file(char *src, char *dst);
void move_file(char *src, char *dst);
void print_working_directory();
void clear_screen();

// int snprintf(char *str, size_t size, const char *format, ...);