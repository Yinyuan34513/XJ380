#pragma once

#include "bitmap.h"
#include "stdint.h"
#include "efi/boot.h"

typedef struct {
    Bitmap bitmap;
    size_t origin_frames;
    size_t usable_frames;
} FrameAllocator;

extern FrameAllocator frame_allocator;

void init_frame(MEMORY_MAP map);

void     free_frame(uint64_t addr);
uint64_t alloc_frames(size_t count);
