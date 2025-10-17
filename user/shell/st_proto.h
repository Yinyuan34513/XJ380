#pragma once

#include "../xapi/include/xguiapi.h"
extern HDLE handle;

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
int check_dir_com(char *str);

void create_directory(char *path);
void change_directory(char *path);
void normalize_path(char *path);
bool create_directory_recursive(char *path);  // 递归创建函数
void list_directory(char *args);
void print_file_content(char *filename);
void remove_file_or_directory(char *path);
void create_file(char *filename);
void copy_file(char *src, char *dst);
void move_file(char *src, char *dst);
void print_working_directory();
void clear_screen();

// int snprintf(char *str, size_t size, const char *format, ...);
void text_editor(char *filename);