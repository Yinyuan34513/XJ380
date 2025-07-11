#pragma once

#include <stdint.h>

int memcmp(const void *a_, const void *b_, size_t size);
void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *dst, int val, size_t size);