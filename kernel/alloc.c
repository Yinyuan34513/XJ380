#include "alloc/sized-pool.c"
#define _detach            mman_detach
#define do_free            mman_do_free
#define try_split_and_free mman_try_split_and_free
#include "alloc/mman.c"
#define _detach            mpool_detach
#define do_free            mpool_do_free
#define try_split_and_free mpool_try_split_and_free
#include "alloc/pool.c"

#include "../include/cinclude.h"

static struct mpool pool;

void memory_init(void *ptr, size_t size) {
  mpool_init(&pool, ptr, size);
}

// size单位=字节
void *malloc(size_t size) {
  return mpool_alloc(&pool, size);
}

void free(void *ptr) {
  mpool_free(&pool, ptr);
}

size_t malloc_usable_size(void *ptr) {
  return mpool_msize(&pool, ptr);
}

void *realloc(void *ptr, size_t size) {
  return mpool_realloc(&pool, ptr, size);
}
