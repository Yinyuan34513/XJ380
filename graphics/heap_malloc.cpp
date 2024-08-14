#include "stdint.h"

#define HEAP_SIZE 1048576  // 堆大小(bytes)
#define BITMAP_SIZE (HEAP_SIZE / 8)

uint8_t heap[HEAP_SIZE];      // 堆
uint8_t bitmap[BITMAP_SIZE];  // 位图

typedef struct FreeBlock {
    uint32_t start;
    uint32_t size;
} FreeBlock;

FreeBlock free_list[HEAP_SIZE / 8];  // 空闲块列表
uint32_t free_list_size = 0;          // 空闲块列表的大小

void set_bit(uint8_t *bitmap, uint32_t index) {
    bitmap[index / 8] |= (1 << (index % 8));
}

void clear_bit(uint8_t *bitmap, uint32_t index) {
    bitmap[index / 8] &= ~(1 << (index % 8));
}
// 使用快速排序对空闲块列表按起始地址排序
void quicksort_free_blocks(uint32_t left, uint32_t right) {
    if (left < right) {
        FreeBlock pivot = free_list[left];
        uint32_t i = left, j = right;
        while (i < j) {
            while (i < j && free_list[j].start >= pivot.start) j--;
            if (i < j) free_list[i++] = free_list[j];
            while (i < j && free_list[i].start <= pivot.start) i++;
            if (i < j) free_list[j--] = free_list[i];
        }
        free_list[i] = pivot;
        quicksort_free_blocks(left, i - 1);
        quicksort_free_blocks(i + 1, right);
    }
}

void add_free_block(FreeBlock block) {
    if (free_list_size < HEAP_SIZE / 8) {
        if (free_list_size > 0 && free_list[free_list_size - 1].start + free_list[free_list_size - 1].size == block.start) {
            free_list[free_list_size - 1].size += block.size;
        } else if (free_list_size > 0 && free_list[free_list_size - 1].start == block.start + block.size) {
            block.size += free_list[free_list_size - 1].size;
            free_list[free_list_size - 1] = block;
        } else {
            free_list[free_list_size++] = block;
        }
    }
}

void merge_free_blocks() {
    for (uint32_t i = 0; i < free_list_size - 1; i++) {
        if (free_list[i].start + free_list[i].size == free_list[i + 1].start) {
            free_list[i].size += free_list[i + 1].size;
            for (uint32_t j = i + 1; j < free_list_size - 1; j++) {
                free_list[j] = free_list[j + 1];
            }
            free_list_size--;
            i--;  // 回退一步以检查新合并的块是否还可以与其他块合并
        }
    }
    quicksort_free_blocks(0, free_list_size - 1);
}

int find_free_block(uint32_t size) {
    for (uint32_t i = 0; i < free_list_size; i++) {
        if (free_list[i].size >= size) {
            FreeBlock block = free_list[i];
            free_list[i].size -= size;
            free_list[i].start += size;
            if (free_list[i].size == 0) {
                for (uint32_t j = i; j < free_list_size - 1; j++) {
                    free_list[j] = free_list[j + 1];
                }
                free_list_size--;
            }
            return block.start;
        }
    }
    return -1;
}

void* HEAP_malloc(uint32_t size) {
    int start_index = find_free_block(size);
    if (start_index == -1) return nullptr;

    // 标记这块内存为已分配
    for (uint32_t i = 0; i < size; i++) {
        set_bit(bitmap, start_index + i);
    }

    return &heap[start_index];
}

void HEAP_free(void *ptr) {
    uint32_t index = ((uint8_t*)ptr) - heap;
    uint32_t size = 0;

    // 确保不会访问位图之外的位置
    while (index + size < HEAP_SIZE && (~bitmap[(index + size) / 8] & (1 << ((index + size) % 8)))) {
        size++;
    }

    // 清除位图中的位
    for (uint32_t i = 0; i < size; i++) {
        clear_bit(bitmap, index + i);
    }

    // 添加到空闲列表
    add_free_block((FreeBlock){.start = index, .size = size});
    merge_free_blocks();  // 合并相邻的空闲块
}