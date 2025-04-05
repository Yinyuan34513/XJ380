#include "../include/cinclude.h"

typedef struct heap_block
{
  size_t size;
  bool free;
  struct heap_block *next;
  struct heap_block *prev;
} heap_block_t;

#define HEAP_BLOCK_HEADER_SIZE sizeof(heap_block_t)
#define HEAP_MIN_SIZE (4 * 1024)
#define HEAP_ALIGNMENT 8

static heap_block_t *heap_start = NULL;
static heap_block_t *heap_end = NULL;

void memory_init(uint64_t start_addr, uint64_t size)
{
  if (size < HEAP_MIN_SIZE)
    return;
  heap_start = (heap_block_t *)start_addr;
  heap_start->size = size;
  heap_start->free = true;
  heap_start->next = NULL;
  heap_start->prev = NULL;
  heap_end = heap_start;
}

void *malloc(size_t size)
{
  if (size == 0 || heap_start == NULL)
    return NULL;
  size = (size + HEAP_ALIGNMENT - 1) & ~(HEAP_ALIGNMENT - 1);

  heap_block_t *current = heap_start;
  while (current)
  {
    if (current->free && current->size >= size + HEAP_BLOCK_HEADER_SIZE)
    {
      if (current->size > size + HEAP_BLOCK_HEADER_SIZE * 2)
      {
        heap_block_t *new_block = (heap_block_t *)((char *)current + HEAP_BLOCK_HEADER_SIZE + size);
        new_block->size = current->size - size - HEAP_BLOCK_HEADER_SIZE;
        new_block->free = true;
        new_block->next = current->next;
        new_block->prev = current;

        if (current->next)
          current->next->prev = new_block;
        current->next = new_block;
        current->size = size + HEAP_BLOCK_HEADER_SIZE;
      }
      current->free = false;
      return (void *)((char *)current + HEAP_BLOCK_HEADER_SIZE);
    }
    current = current->next;
  }
  return NULL;
}

void free(void *ptr)
{
  if (ptr == NULL || heap_start == NULL)
    return;
  heap_block_t *block = (heap_block_t *)((char *)ptr - HEAP_BLOCK_HEADER_SIZE);
  block->free = true;

  if (block->next && block->next->free)
  {
    block->size += block->next->size + HEAP_BLOCK_HEADER_SIZE;
    block->next = block->next->next;
    if (block->next)
      block->next->prev = block;
  }

  if (block->prev && block->prev->free)
  {
    block->prev->size += block->size + HEAP_BLOCK_HEADER_SIZE;
    block->prev->next = block->next;
    if (block->next)
      block->next->prev = block->prev;
  }
}
