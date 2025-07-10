#include <stdint.h>
#include <mm/memory.h>

UINT64 get_memory_size(MEMORY_MAP map)
{
    UINT64 total_pages = 0;
    EfiMemoryDesc *EfiMemory = (EfiMemoryDesc *)((uint64_t)(map.Buffer) + 0xFFFF800000000000);

    for (int i = 0; i < (map.MapSize / map.DescriptorSize); i++)
    {
        if (EfiMemory->Type == EfiConventionalMemory || EfiMemory->Type == EfiBootServicesCode || 
            EfiMemory->Type == EfiBootServicesData)
        {
            total_pages += EfiMemory->NumberOfPages;
        }
        EfiMemory++;
    }

    return total_pages * PAGE_SIZE;
}

