#include <mm/buddy.h>
#include <mm/frame.h>
#include <mm/memory.h>
#include <proto.hpp>
#include <dlinker.h>

#define ENABLE_BUDDY 0

FrameAllocator frame_allocator;
Bitmap         usable_regions;
uint64_t       memory_size       = 0;
uint64_t       memory_total_size = 0;

uint64_t get_memory_size(MEMORY_MAP map)
{
    uint64_t       total_pages = 0;
    EfiMemoryDesc *EfiMemory   = (EfiMemoryDesc *)((uint64_t)(map.Buffer) + 0xFFFF800000000000);

    for (int i = 0; i < (map.MapSize / map.DescriptorSize); i++)
    {
        if (EfiMemory->Type == EfiConventionalMemory) { total_pages += EfiMemory->NumberOfPages; }
        EfiMemory++;
    }

    return total_pages * PAGE_SIZE;
}

uint64_t get_memory_all_size(MEMORY_MAP map)
{
    uint64_t       total_pages = 0;
    EfiMemoryDesc *EfiMemory   = (EfiMemoryDesc *)((uint64_t)(map.Buffer) + 0xFFFF800000000000);

    for (int i = 0; i < (map.MapSize / map.DescriptorSize); i++)
    {
        total_pages += EfiMemory->NumberOfPages;
        EfiMemory++;
    }

    return total_pages * 4;
}

void init_frame(MEMORY_MAP map)
{
#if ENABLE_BUDDY
    init_frame_buddy(map);
#else
    memory_size       = get_memory_size(map);
    memory_total_size = get_memory_all_size(map);

    size_t         bitmap_size    = (memory_size / 4096 + 7) / 8;
    uint64_t       bitmap_address = 0;
    EfiMemoryDesc *EfiMemory      = (EfiMemoryDesc *)((uint64_t)(map.Buffer) + 0xFFFF800000000000);

    for (uint64_t i = 0; i < (map.MapSize / map.DescriptorSize); i++)
    {
        if (EfiMemory->PhysicalStart >= 0x100000 && (EfiMemory->Type == EfiConventionalMemory))
        {
            if (EfiMemory->NumberOfPages * PAGE_SIZE > ((bitmap_size + PAGE_SIZE - 1) & (~PAGE_SIZE - 1)) * 2)
            {
                bitmap_address = EfiMemory->PhysicalStart;
                break;
            }
        }
        EfiMemory++;
    }

    // if (!bitmap_address) return;

    Bitmap *bitmap = &frame_allocator.bitmap;
    bitmap_init(bitmap, (uint8_t *)(bitmap_address + 0xFFFF800000000000), bitmap_size);
    bitmap_init(&usable_regions,
                (uint8_t *)(bitmap_address + ((bitmap_size + PAGE_SIZE - 1) & (~PAGE_SIZE - 1)) + 0xFFFF800000000000),
                bitmap_size);
    EfiMemory            = (EfiMemoryDesc *)((uint64_t)(map.Buffer) + 0xFFFF800000000000);
    size_t origin_frames = 0;
    for (uint64_t i = 0; i < (map.MapSize / map.DescriptorSize); i++)
    {
        if (EfiMemory->PhysicalStart >= 0x100000 && (EfiMemory->Type == EfiConventionalMemory))
        {
            size_t start_frame = EfiMemory->PhysicalStart / PAGE_SIZE;
            size_t frame_count = EfiMemory->NumberOfPages;
            serial_wprintf("%p~%p\n", start_frame * PAGE_SIZE, (start_frame + frame_count) * PAGE_SIZE);
            origin_frames += frame_count;
            bitmap_set_range(bitmap, start_frame, start_frame + frame_count, true);
            bitmap_set_range(&usable_regions, start_frame, start_frame + frame_count, true);
        }
        EfiMemory++;
    }

    size_t bitmap_frame_start = bitmap_address / PAGE_SIZE;
    size_t bitmap_frame_count = (((bitmap_size + PAGE_SIZE - 1) & (~PAGE_SIZE - 1)) * 2) / PAGE_SIZE;
    size_t bitmap_frame_end   = bitmap_frame_start + bitmap_frame_count;
    bitmap_set_range(bitmap, bitmap_frame_start, bitmap_frame_end, false);
    bitmap_set_range(&usable_regions, bitmap_frame_start, bitmap_frame_end, false);

    frame_allocator.origin_frames = origin_frames;
    frame_allocator.usable_frames = origin_frames - bitmap_frame_count;

    write_serial_string("Available memory: ");
    write_serial_dec((origin_frames / 256));
    write_serial_string(" MiB\n");
#endif
}

spin_t frame_op_lock = SPIN_INIT;

void free_frame(uint64_t addr)
{
    spin_lock(frame_op_lock);
#if ENABLE_BUDDY
    buddy_free_pages(addr, 1);
#else
    if (addr == 0)
    {
        spin_unlock(frame_op_lock);
        return;
    }
    size_t frame_index = addr / PAGE_SIZE;
    if (bitmap_get(&usable_regions, frame_index) == false)
    {
        spin_unlock(frame_op_lock);
        return;
    }
    Bitmap *bitmap = &frame_allocator.bitmap;
    bitmap_set(bitmap, frame_index, true);
#endif
    frame_allocator.usable_frames++;
    spin_unlock(frame_op_lock);
}

EXPORT_SYMBOL(free_frame);

uint64_t alloc_frames(size_t count)
{
#if ENABLE_BUDDY
    frame_allocator.usable_frames -= count;
    return buddy_alloc_pages(count);
#else
    Bitmap *bitmap      = &frame_allocator.bitmap;
    size_t  frame_index = bitmap_find_range(bitmap, count, true);

    if (frame_index == (size_t)-1) return 0;
    bitmap_set_range(bitmap, frame_index, frame_index + count, false);

    frame_allocator.usable_frames -= count;

    return frame_index * PAGE_SIZE;
#endif
}

EXPORT_SYMBOL(alloc_frames);

void free_frames(uint64_t addr, uint64_t size)
{
    spin_lock(frame_op_lock);
#if ENABLE_BUDDY
    buddy_free_pages(addr, size);
#else

    if (addr == 0)
    {
        spin_unlock(frame_op_lock);
        return;
    }

    size_t frame_index = addr / PAGE_SIZE;
    for (size_t i = frame_index; i < size; i++)
    {
        if (bitmap_get(&usable_regions, i) == false)
        {
            spin_unlock(frame_op_lock);
            return;
        }
    }

    bitmap_set_range(&frame_allocator.bitmap, frame_index, frame_index + size, true);
#endif
    frame_allocator.usable_frames += size;

    spin_unlock(frame_op_lock);
}

EXPORT_SYMBOL(free_frames);