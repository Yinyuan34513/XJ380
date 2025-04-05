#include "../include/include.h"
#include "./build_settings.h"
#include "./alloc/page.h"

typedef struct
{
    UINT32 Type;
    EFI_PHYSICAL_ADDRESS PhysicalStart;
    EFI_VIRTUAL_ADDRESS VirtualStart;
    UINT64 NumberOfPages;
    UINT64 Attribute;
} __attribute__((__aligned__(16))) EfiMemoryDesc;

extern "C" void memory_init(void *ptr, size_t size);

void memory_test(const FrameBufferConfig &fbc)
{
    /*
    void *ptr1 = malloc(16);
    void *ptr2 = malloc(16);
    void *ptr3 = malloc(16);
    WriteString(fbc, 440, 132, "MemoryManagerTest:", WHITE);
    WriteHex(fbc, 440, 148, (size_t)ptr1, WHITE);
    WriteHex(fbc, 440, 164, (size_t)ptr2, WHITE);
    WriteHex(fbc, 440, 180, (size_t)ptr3, WHITE);
    free(ptr2);
    void *ptr4 = malloc(16);
    WriteHex(fbc, 440, 196, (size_t)ptr4, WHITE);
    free(ptr3);
    free(ptr4);
    void *ptr5 = malloc(8192);
    WriteHex(fbc, 440, 212, (size_t)ptr5, WHITE);
    free(ptr1);
    free(ptr5);
    */
}

void init_MemoryManager(const FrameBufferConfig &fbc, BOOT_CONFIG *BootConfig)
{
    // 初始化
    write_serial_string("Initializing Memory Manager...\n");
    EfiMemoryDesc *EfiMemory = (EfiMemoryDesc *)convert_physical_to_virtual((uint64_t)(BootConfig->MemoryMap.Buffer));
#if BUILD_EDITION == DEBUG_VERSION
    write_serial_string("MemoryMap:\n");
    for (int i = 0; i < (BootConfig->MemoryMap.MapSize / BootConfig->MemoryMap.DescriptorSize); i++)
    {
        write_serial_hex(EfiMemory->Type);
        write_serial_string(" ");
        write_serial_hex(EfiMemory->PhysicalStart);
        write_serial_string(" ");
        write_serial_hex(EfiMemory->VirtualStart);
        write_serial_string(" ");
        write_serial_hex(EfiMemory->NumberOfPages);
        write_serial_string(" ");
        write_serial_hex(EfiMemory->Attribute);
        write_serial_string("\n");
        EfiMemory++;
    }
    EfiMemory = (EfiMemoryDesc *)convert_physical_to_virtual((uint64_t)(BootConfig->MemoryMap.Buffer));
#endif

    OS_MEMORY_DESCRIPTOR *OsMemory = NULL;
    int EfiMemoryPageSize = (BootConfig->MemoryMap.MapSize >> 12) + 1;
    EFI_PHYSICAL_ADDRESS OsMemoryAddress = 0;
    int EfiDescriptorCount = BootConfig->MemoryMap.MapSize / BootConfig->MemoryMap.DescriptorSize;
    for (int i = 0; i < EfiDescriptorCount; i++)
    {
        if (EfiMemoryPageSize < EfiMemory[i].NumberOfPages && EfiMemory[i].Type == EfiConventionalMemory)
        {
            OsMemoryAddress = convert_physical_to_virtual((uint64_t)EfiMemory[i].PhysicalStart);
            OsMemory = (OS_MEMORY_DESCRIPTOR *)OsMemoryAddress;
            break;
        }
    }

    /*
    if (BUILD_EDITION == DEBUG_VERSION) {
      WriteString(fbc, 480, 480, "FirstFreeMemoryAddress:", WHITE);
      WriteHex(fbc, 480, 496, (UINT64)OsMemory, WHITE);
    }
    */

    size_t maxpageaddr = 0;
    size_t maxpagesize = 0;

    int OsMemDescNum = 0;
    // 内存合并与分类
    for (int i = 0; i < EfiDescriptorCount; i++)
    {
        if (EfiMemory[i].Type == EfiConventionalMemory || EfiMemory[i].Type == EfiBootServicesCode || EfiMemory[i].Type == EfiBootServicesData)
        {
            OsMemory->Type = FREE_MEMORY;
        }
        else if (EfiMemory[i].Type == EfiMemoryMappedIO)
        {
            OsMemory->Type = MMIO_MEMORY;
        }
        else
        {
            OsMemory->Type = UEFI_MEMORY;
        }
        OsMemory->PhysicalStart = EfiMemory[i].PhysicalStart;
        OsMemory->PageSize = EfiMemory[i].NumberOfPages;

        if (OsMemory->Type == FREE_MEMORY && OsMemory->PageSize > maxpagesize)
        {
            maxpageaddr = convert_physical_to_virtual((uint64_t)OsMemory->PhysicalStart);
            maxpagesize = OsMemory->PageSize;
        }

        if (i > 0 && OsMemory->Type == (OsMemory - 1)->Type)
        {
            if (OsMemory->PhysicalStart ==
                (OsMemory - 1)->PhysicalStart + ((OsMemory - 1)->PageSize << 12))
            {
                (OsMemory - 1)->PageSize += OsMemory->PageSize;
                continue;
            }
        }
        OsMemory++;
        OsMemDescNum++;
    }

    write_serial_string("maxpageaddr: ");
    write_serial_hex((uint64_t)maxpageaddr);
    write_serial_string(" ");
    write_serial_string("maxpagesize: ");
    write_serial_hex(maxpagesize);
    write_serial_string("\n");
    memory_init((void *)maxpageaddr, maxpagesize * PAGE_SIZE);

    uint64_t page_start = 0;
    size_t page_count = 0;

    for (int i = 0; i < EfiDescriptorCount; i++)
    {
        if (EfiMemory[i].Type == EfiConventionalMemory && EfiMemory[i].PhysicalStart != convert_virtual_to_physical((uint64_t)maxpageaddr) && EfiMemory[i].PhysicalStart != convert_virtual_to_physical((uint64_t)OsMemoryAddress))
        {
            write_serial_string("Found free memory for page alloc\n");
            if (EfiMemory[i].NumberOfPages > page_count)
            {
                page_start = (uint64_t)EfiMemory[i].PhysicalStart;
                page_count = EfiMemory[i].NumberOfPages;
            }
        }
    }

    if (page_count == 0)
    {
        write_serial_string("No free memory found.\n");
        while (1)
        {
            __asm__("pause");
        }
    }

    uint64_t bitmap_size = (page_count + 7) / 8;
    write_serial_string("page_start: ");
    write_serial_hex(page_start);
    write_serial_string(" ");
    write_serial_string("page_count: ");
    write_serial_hex(page_count);
    write_serial_string("\n");
    uint8_t *bitmap = (uint8_t *)malloc(bitmap_size);
    PAGE_ALLOC.init((void *)page_start, page_count, bitmap);

    // 输出合并后的
    OsMemory = (OS_MEMORY_DESCRIPTOR *)OsMemoryAddress;
    UINT64 TotalMemory = 0;

#if BUILD_EDITION == DEBUG_VERSION
    write_serial_string("MemoryMap2:\n");
    write_serial_string("Type | PhysicalStart | PageSize\n");
    for (int i = 0; i < OsMemDescNum; i++)
    {
        write_serial_hex(OsMemory->Type);
        write_serial_string("  | ");
        write_serial_hex(OsMemory->PhysicalStart);
        write_serial_string("       | ");
        write_serial_hex(OsMemory->PageSize);
        write_serial_string("\n");
        if (OsMemory->Type != MMIO_MEMORY)
        {
            TotalMemory += OsMemory->PageSize;
        }
        OsMemory++;
    }
    write_serial_string("TotalMemory: ");
    write_serial_dec(TotalMemory * 4); // 一个Page=4KB
    write_serial_string("KB\n");
    // memory_test(fbc);
#endif

    write_serial_string("Memory Manager Initialize Success.\n");
}

uint64_t mm_do_brk(uint64_t start, uint64_t size)
{
    KERNEL_MAPPER mapper((uint64_t *)convert_physical_to_virtual((uint64_t)current_task->mm->pgd));

    uint64_t i = 0;
    for (i = start; i < start + size; i += PAGE_SIZE)
    {
        mapper.map_page(i, 0, 0x7);
    }

    return i;
}
