#include <stdint.h>
#include <krlibc.h>
#include <cpu/lock.h>
#include <mm/alloc/area.h>
#include <proto.h>

#define KERNEL_HEAP_START     0xffff900000000000       // 内核堆起始地址
#define KERNEL_HEAP_SIZE      0x6400000                // 内核堆大小 100MB

static struct mpool pool;
static spin_t       lock = SPIN_INIT;

uint64_t get_all_memusage() {
    return KERNEL_HEAP_SIZE;
}

static bool alloc_enter() {
    bool is_sti = are_interrupts_enabled();
    disable_intr();
    spin_lock(lock);
    return is_sti;
}

static void alloc_exit(bool is_sti) {
    spin_unlock(lock);
    if (is_sti) enable_intr();
}

void *malloc(size_t size) noexcept {
    const bool is_sti = alloc_enter();
    void      *ptr    = mpool_alloc(&pool, size);
    alloc_exit(is_sti);
    return ptr;
}

void free(void *ptr) noexcept {
    bool is_sti = alloc_enter();
    mpool_free(&pool, ptr);
    alloc_exit(is_sti);
}

void *xmalloc(size_t size) noexcept {
    void *ptr = malloc(size);
    if (ptr == NULL) write_serial_string("[Error][HEAP]xmalloc failed.");
    return ptr;
}

void *calloc(size_t n, size_t size) noexcept {
    if (__builtin_mul_overflow(n, size, &size)) return NULL;
    void *ptr = malloc(size);
    if (ptr == NULL) return NULL;
    memset(ptr, 0, size);
    return ptr;
}

void *realloc(void *ptr, size_t newsize) noexcept {
    const bool is_sti = alloc_enter();
    void      *n_ptr  = mpool_realloc(&pool, ptr, newsize);
    alloc_exit(is_sti);
    return n_ptr;
}

void *reallocarray(void *ptr, size_t n, size_t size) noexcept {
    return realloc(ptr, n * size);
}

void *aligned_alloc(size_t align, size_t size) noexcept {
    const bool is_sti = alloc_enter();
    void      *ptr    = mpool_aligned_alloc(&pool, size, align);
    alloc_exit(is_sti);
    return ptr;
}

size_t malloc_usable_size(void *ptr) noexcept {
    bool   is_sti = alloc_enter();
    size_t size   = mpool_msize(&pool, ptr);
    alloc_exit(is_sti);
    return size;
}

void *memalign(size_t align, size_t size) noexcept {
    const bool is_sti = alloc_enter();
    void      *ptr    = (void *)(uint64_t)(mpool_aligned_alloc(&pool, size, align));
    alloc_exit(is_sti);
    return ptr;
}

int posix_memalign(void **memptr, size_t alignment, size_t size) noexcept {
    const bool is_sti = alloc_enter();
    void      *ptr    = (void *)(uint64_t)(mpool_aligned_alloc(&pool, size, alignment));
    alloc_exit(is_sti);
    if (ptr == NULL) return 1;
    *memptr = ptr;
    return 0;
}

void *valloc(size_t size) noexcept {
    const bool is_sti = alloc_enter();
    void      *ptr    = (void *)(uint64_t)(mpool_aligned_alloc(&pool, size, PAGE_SIZE));
    alloc_exit(is_sti);
    return ptr;
}

void *pvalloc(size_t size) noexcept {
    const bool is_sti = alloc_enter();
    void      *ptr    = (void *)(uint64_t)(mpool_aligned_alloc(&pool, size, PAGE_SIZE));
    alloc_exit(is_sti);
    return ptr;
}

void init_heap() {
    uint64_t base_addr = KERNEL_HEAP_START;
    write_serial_string("Init heap at ");
    write_serial_hex(base_addr);
    write_serial_string("\n");
    page_map_range_to_random(get_kernel_pagedir(), base_addr, KERNEL_HEAP_SIZE, KERNEL_PTE_FLAGS);
    mpool_init(&pool, (void *)base_addr, KERNEL_HEAP_SIZE);
    switch_xsk_page_directory();
}
