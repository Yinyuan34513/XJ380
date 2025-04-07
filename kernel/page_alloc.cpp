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
    this->page_start = page_start;
    this->page_count = page_count;
    this->bitmap = (uint8_t *)bitmap;
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
    return this->alloc_pages_align(page_count, PAGE_SIZE);
}

void *PageAlloc::alloc_pages_align(size_t page_count, uint64_t align)
{
    if (page_count == 0 || page_count > this->page_count || align == 0)
    {
        return nullptr;
    }

    // 确保对齐是页大小的倍数且是2的幂
    if ((align % 4096) != 0 || (align & (align - 1)) != 0)
    {
        return nullptr;
    }

    uint64_t start_page = 0;
    uint64_t consecutive_free = 0;
    uint64_t align_pages = align / 4096; // 对齐要求的页面数

    for (uint64_t i = 0; i < this->page_count; i++)
    {
        if (!test_bit(i))
        {
            consecutive_free++;
            if (consecutive_free >= page_count)
            {
                start_page = i - page_count + 1;

                // 检查对齐要求
                uint64_t addr = (uint64_t)this->page_start + start_page * 4096;
                if ((addr & (align - 1)) == 0)
                {
                    // 满足对齐要求，标记页面为已使用
                    for (uint64_t j = 0; j < page_count; j++)
                    {
                        set_bit(start_page + j);
                    }
                    return (void *)addr;
                }
                // 不满足对齐要求，继续搜索
                consecutive_free = page_count - 1; // 保留可能的连续空间
            }
        }
        else
        {
            consecutive_free = 0;
        }
    }

    return nullptr; // 没有找到满足条件的连续页面
}

void PageAlloc::free_pages(void *page, size_t page_count)
{
    for (size_t i = 0; i < page_count; i++)
    {
        free_page((void *)((char *)page + i * 4096));
    }
}
