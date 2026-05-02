#pragma once

#include <krlibc.h>
#include <mm/page.h>

static inline bool user_range_mapped(page_directory_t *pagedir, const void *ptr, size_t size)
{
    if (size == 0) return true;
    if (pagedir == NULL || ptr == NULL) return false;

    uint64_t start = (uint64_t)ptr;
    if (check_user_overflow(start, size)) return false;

    uint64_t end = start + size - 1;
    if (end < start) return false;

    uint64_t page      = start & PAGE_MASK;
    uint64_t last_page = end & PAGE_MASK;
    while (true)
    {
        uint64_t flags = 0;
        if (translate_address(pagedir, page) == 0) return false;
        if (!page_table_get_flags(pagedir, page, &flags)) return false;
        if ((flags & (PTE_PRESENT | PTE_USER)) != (PTE_PRESENT | PTE_USER)) return false;
        if (page == last_page) break;
        if (~page < (uint64_t)PAGE_SIZE - 1) return false;
        page += PAGE_SIZE;
    }

    return true;
}

static inline bool copy_from_user_pagedir(page_directory_t *pagedir, void *dst, const void *src, size_t size)
{
    if (size == 0) return true;
    if (dst == NULL) return false;
    if (!user_range_mapped(pagedir, src, size)) return false;
    memcpy(dst, src, size);
    return true;
}

static inline bool copy_to_user_pagedir(page_directory_t *pagedir, void *dst, const void *src, size_t size)
{
    if (size == 0) return true;
    if (src == NULL) return false;
    if (!user_range_mapped(pagedir, dst, size)) return false;
    memcpy(dst, src, size);
    return true;
}
