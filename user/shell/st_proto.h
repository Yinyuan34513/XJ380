#pragma once

extern HDLE handle;

void check_command();
void print_to_console(char *str);
void putchar_at_cur(char *ch);
void paint_arrow_sign(int sx, int sy);
void flush_buffer();
void newline();