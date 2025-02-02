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

void *PageAlloc::alloc_pages(size_t page_count)
{
    if (page_count == 0 || page_count > this->page_count)
    {
        return nullptr;
    }

    // 查找连续的空闲页面
    uint64_t start_page = 0;
    uint64_t consecutive_free = 0;

    for (uint64_t i = 0; i < this->page_count; i++)
    {
        if (!test_bit(i))
        {
            consecutive_free++;
            if (consecutive_free == page_count)
            {
                // 找到足够的连续页面
                start_page = i - page_count + 1;
                // 标记这些页面为已使用
                for (uint64_t j = 0; j < page_count; j++)
                {
                    set_bit(start_page + j);
                }
                return (void *)((char *)page_start + start_page * 4096);
            }
        }
        else
        {
            consecutive_free = 0;
        }
    }

    return nullptr; // 没有找到足够的连续页面
}

void PageAlloc::free_pages(void *page, size_t page_count)
{
    for (size_t i = 0; i < page_count; i++)
    {
        free_page((void *)((char *)page + i * 4096));
    }
}
