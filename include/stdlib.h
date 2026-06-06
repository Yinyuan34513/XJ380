#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void  free(void *ptr);
int   atoi(const char *str);
void  qsort(void *base, size_t nitems, size_t size, int (*cmp)(const void *, const void *));

#ifdef __cplusplus
}
#endif
