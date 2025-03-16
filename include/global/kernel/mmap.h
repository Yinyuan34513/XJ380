#ifndef _MMAP_H
#define _MMAP_H

#include "../../include.h"

// #define PAGE_TABLE_BASE_ADDR ((uint64_t *)0xFFFFFFFFFFFFF000)

class KERNEL_MAPPER
{
public:
    KERNEL_MAPPER(uint64_t *page_table);
    ~KERNEL_MAPPER();

    uint64_t map_page(uint64_t vaddr, uint64_t paddr, uint64_t flags);
    void unmap_page(uint64_t vaddr);
    uint64_t map_range(uint64_t vaddr, uint64_t paddr, uint64_t size, uint64_t flags);
    void unmap_range(uint64_t vaddr, uint64_t size);

    uint64_t translate_address(uint64_t virtual_address);

private:
    uint64_t *page_table;
};

#endif