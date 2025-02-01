#include "./alloc/page.h"
#include "../include/include.h"

// 这个page alloc专门用来dma，task的page table等
PageAlloc PAGE_ALLOC;

PageAlloc::PageAlloc() : bitmap(nullptr), page_start(nullptr), page_count(0)
{
}

PageAlloc::~PageAlloc()
{
}

void PageAlloc::init(void *page_start, size_t page_count, uint8_t *bitmap)
{
    // 假设从boot_config获取内存信息
    this->page_start = page_start;
    this->page_count = page_count; // 假设页大小为4KB
    this->bitmap = (uint8_t *)bitmap;

    // 初始化位图，全部标记为可用
    for (uint64_t i = 0; i < (page_count + 7) / 8; i++)
    {
        bitmap[i] = 0;
    }
}

void *PageAlloc::alloc_page()
{
    for (uint64_t i = 0; i < page_count; i++)
    {
        if (!test_bit(i))
        {
            set_bit(i);
            return (void *)((char *)page_start + i * 4096);
        }
    }
    return nullptr; // 没有可用页面
}

void PageAlloc::free_page(void *page)
{
    uint64_t index = ((char *)page - (char *)page_start) / 4096;
    if (index < page_count)
    {
        clear_bit(index);
    }
}

void PageAlloc::set_bit(uint64_t index)
{
    bitmap[index / 8] |= (1 << (index % 8));
}

void PageAlloc::clear_bit(uint64_t index)
{
    bitmap[index / 8] &= ~(1 << (index % 8));
}

bool PageAlloc::test_bit(uint64_t index)
{
    return (bitmap[index / 8] & (1 << (index % 8))) != 0;
}
