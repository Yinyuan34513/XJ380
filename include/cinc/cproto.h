#ifndef _CPROTO_HPP_
#define _CPROTO_HPP_

/*
*   lib/*
*/
// lib/string.c
char *strcpy(char *dest, const char *src);
UINT64 strlen(const char *str);
UINT64 strcmp(char *from_str, char *cmp_str);
UINT64 part_strcmp(char *from_str, char *cmp_str, UINT64 size);

#endif
