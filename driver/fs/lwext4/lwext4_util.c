#include <stdint.h>

void *memmove(void *dest, const void *src, size_t n)
{
    uint8_t       *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;

    if (d == s || n == 0) return dest;

    if (d < s)
    {
        for (size_t i = 0; i < n; i++) d[i] = s[i];
    }
    else
    {
        for (size_t i = n; i > 0; i--) d[i - 1] = s[i - 1];
    }

    return dest;
}

static void qsort_swap(uint8_t *a, uint8_t *b, size_t size)
{
    while (size--)
    {
        uint8_t tmp = *a;
        *a++        = *b;
        *b++        = tmp;
    }
}

static void qsort_impl(uint8_t *base, size_t nitems, size_t size, int (*cmp)(const void *, const void *))
{
    if (nitems < 2) return;

    size_t pivot = nitems / 2;
    size_t left  = 0;
    size_t right = nitems - 1;

    qsort_swap(base + pivot * size, base + right * size, size);
    pivot = right;

    for (size_t i = 0; i < right; i++)
    {
        if (cmp(base + i * size, base + pivot * size) < 0)
        {
            qsort_swap(base + i * size, base + left * size, size);
            left++;
        }
    }

    qsort_swap(base + left * size, base + pivot * size, size);
    qsort_impl(base, left, size, cmp);
    qsort_impl(base + (left + 1) * size, nitems - left - 1, size, cmp);
}

void qsort(void *base, size_t nitems, size_t size, int (*cmp)(const void *, const void *))
{
    if (base == 0 || size == 0 || cmp == 0) return;
    qsort_impl((uint8_t *)base, nitems, size, cmp);
}
