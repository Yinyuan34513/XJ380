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