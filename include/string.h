#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int     memcmp(const void *a, const void *b, size_t size);
void   *memcpy(void *dest, const void *src, size_t n);
void   *memmove(void *dest, const void *src, size_t n);
void   *memset(void *dst, int val, size_t size);
char   *strcat(char *dest, const char *src);
char   *strchr(const char *s, int c);
int     strcmp(const char *s1, const char *s2);
char   *strcpy(char *dest, const char *src);
char   *strdup(const char *str);
size_t  strlen(const char *str);
int     strncmp(const char *s1, const char *s2, size_t n);
char   *strncpy(char *dest, const char *src, size_t n);
char   *strrchr(const char *s, int c);
char   *strstr(const char *str1, const char *str2);

#ifdef __cplusplus
}
#endif
