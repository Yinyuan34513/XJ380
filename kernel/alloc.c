#include "alloc/sized-pool.h"
#define _detach mman_detach
#define do_free mman_do_free
#define try_split_and_free mman_try_split_and_free
#include "alloc/mman.h"
#define _detach mpool_detach
#define do_free mpool_do_free
#define try_split_and_free mpool_try_split_and_free
#include "alloc/pool.h"

#include "../include/cinclude.h"

static struct mpool pool;

/**
 *
 * @brief 初始化内存管理
 *
 * @param ptr  指向空闲内存起始地址的指针
 * @param size 大小（单位字节）
 *
 */
void memory_init(void *ptr, size_t size)
{
  mpool_init(&pool, ptr, size);
}

/**
 *
 * @brief 开辟内存空间
 *
 * @param size 大小（单位字节）
 *
 */
void *malloc(size_t size)
{
  return mpool_alloc(&pool, size);
}

/**
 *
 * @brief 一键免费内存空间
 *
 * @param size 指向需要释放的内存的指针
 *
 */
void free(void *ptr)
{
  mpool_free(&pool, ptr);
}

/**
 *
 * @brief 获取分配的内存的大小
 *
 * @param ptr      分配的内存地址
 *
 */
size_t malloc_usable_size(void *ptr)
{
  return mpool_msize(&pool, ptr);
}

/**
 *
 * @brief 重新分配内存并复制
 *
 * @param ptr  指向目标的指针
 * @param size 大小（单位字节）
 *
 */
void *realloc(void *ptr, size_t size)
{
  return mpool_realloc(&pool, ptr, size);
}
