#ifndef _STRING_H_
#define _STRING_H_

#include "../include/include.h"

char  *strncpy(char *dest, const char *src, UINT64 length);
char  *strcpy(char *dest, const char *src);
UINT64 strlen(const char *str);
UINT64 strcmp(const char *from_str, const char *cmp_str);
UINT64 strncmp(char *from_str, char *cmp_str, UINT64 size);
UINT64 part_strcmp(char *from_str, char *cmp_str, UINT64 size);
UINT64 strtol(char *str, char **endptr, int base);
char  *Dec2Char(unsigned long long dec);
char  *Hex2Char(unsigned long long hex);
char  *strstr(const char *haystack, const char *needle);
int    str2float(const char *str, float *result);

#endif