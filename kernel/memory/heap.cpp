#include <mm/alloc/alloc.h>

void *calloc(size_t num, size_t size)
{
    size_t tot = num * size;
    void  *ptr = malloc(tot);
    if (ptr) memset(ptr, 0, tot);
    return ptr;
}
