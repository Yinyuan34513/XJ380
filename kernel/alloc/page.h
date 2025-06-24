#ifndef _PAGE_H
#define _PAGE_H

#include "../../include/include.h"

class PageAlloc {
  private:
    uint8_t *bitmap;     // 位图数组
    void    *page_start; // 页面起始地址
    uint64_t page_count; // 总页面数

  public:
    PageAlloc();
    ~PageAlloc();

    void  init(void *page_start, size_t page_count, uint8_t *bitmap);
    void *alloc_page();
    void  free_page(void *page);

    void *alloc_pages(size_t page_count);
    void *alloc_pages_align(size_t page_count, uint64_t align);
    void  free_pages(void *page, size_t page_count);

    void set_bit(uint64_t index);
    void clear_bit(uint64_t index);
    bool test_bit(uint64_t index);
};

extern PageAlloc PAGE_ALLOC;

#endif