#pragma once

#include <stdint.h>

int memcmp(const void *a_, const void *b_, size_t size);
void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *dst, int val, size_t size);
char *strcpy(char *dest, const char *src);

static inline bool are_interrupts_enabled() {
    uint64_t rflags;
    __asm__ volatile("pushfq\n\t"
                     "pop %0"
                     : "=r"(rflags));
    return (rflags & (1 << 9)) != 0;
}

#define PADDING_DOWN(size, to) ((size_t)(size) / (size_t)(to) * (size_t)(to))
#define PADDING_UP(size, to)   PADDING_DOWN((size_t)(size) + (size_t)(to) - (size_t)1, to)
