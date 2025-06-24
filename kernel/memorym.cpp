#include "../include/include.h"
#include "./build_settings.h"
#include "./alloc/page.h"

#if BUILD_VERSION == DEBUG_VERSION
extern bool MemoryWasInit;
#endif

typedef struct
{
    UINT32 Type;
    EFI_PHYSICAL_ADDRESS PhysicalStart;
    EFI_VIRTUAL_ADDRESS VirtualStart;
    UINT64 NumberOfPages;
    UINT64 Attribute;
} __attribute__((__aligned__(16))) EfiMemoryDesc;

extern "C" void memory_init(void *ptr, size_t size);
extern spinlock_t malloc_lock;

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

// 检查 CPU 是否支持 SSE2 (x86_64 必备)
bool supports_sse2() {
    uint32_t eax, ebx, ecx, edx;
    // 获取 CPU 特征信息
    asm volatile("cpuid"
                 : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                 : "a"(1));
    return edx & (1 << 26);  // SSE2 位于 EDX 第 26 位
}

// 检查 CPU 是否支持 AVX
bool supports_avx() {
    uint32_t eax, ebx, ecx, edx;
    asm volatile("cpuid"
                 : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                 : "a"(1), "c"(0));
    return ecx & (1 << 28);  // AVX 位于 ECX 第 28 位
}

void set_bitmap(void* bitmap, size_t size) {
    // 检测 AVX2 支持 (可选)
    if (supports_avx()) {
        asm volatile(
            "call set_bitmap_avx2"
            : 
            : "D"(bitmap), "S"(size)
            : "rax", "rcx", "ymm0", "memory"
        );
    } else {
        // 默认使用 SSE2 (x86_64 必须支持)
        asm volatile(
            "call set_bitmap_sse"
            : 
            : "D"(bitmap), "S"(size)
            : "rax", "rcx", "xmm0", "memory"
        );
    }
}

void init_MemoryManager(const FrameBufferConfig &fbc, BOOT_CONFIG *BootConfig)
{
    // Setup PAGE_ALLOC
    uint64_t total_pages = 0;

    // 初始化
    write_serial_string("Initializing Memory Manager...\n");
    EfiMemoryDesc *EfiMemory = (EfiMemoryDesc *)convert_physical_to_virtual((uint64_t)(BootConfig->MemoryMap.Buffer));
#if BUILD_EDITION == DEBUG_VERSION
    write_serial_string("MemoryMap:\n");
#endif

    for (int i = 0; i < (BootConfig->MemoryMap.MapSize / BootConfig->MemoryMap.DescriptorSize); i++)
    {
#if BUILD_EDITION == DEBUG_VERSION
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
#endif
        if (EfiMemory->Type == EfiConventionalMemory)
        {
            total_pages += EfiMemory->NumberOfPages;
        }
        EfiMemory++;
    }
    EfiMemory = (EfiMemoryDesc *)convert_physical_to_virtual((uint64_t)(BootConfig->MemoryMap.Buffer));

    write_serial_string("Total pages: ");
    write_serial_hex(total_pages);
    write_serial_string("\n");

    uint64_t last_size = ~(0ULL);
    uint64_t bitmap_phys = 0;
    uint8_t *bitmap = nullptr;

    uint64_t bitmap_size = ((total_pages / 8) + 7) / 8;
    for (int i = 0; i < (BootConfig->MemoryMap.MapSize / BootConfig->MemoryMap.DescriptorSize); i++)
    {
        if (EfiMemory->Type == EfiConventionalMemory && EfiMemory->NumberOfPages * PAGE_SIZE >= bitmap_size && EfiMemory->NumberOfPages * PAGE_SIZE < last_size)
        {
            bitmap_phys = EfiMemory->PhysicalStart;
            bitmap = (uint8_t *)convert_physical_to_virtual(bitmap_phys);
        }
        EfiMemory++;
    }
    EfiMemory = (EfiMemoryDesc *)convert_physical_to_virtual((uint64_t)(BootConfig->MemoryMap.Buffer));

    xmemset(bitmap, 0xFF, bitmap_size);
    write_serial_string("Masked all physical memory\n");

    PAGE_ALLOC.init((void *)0x0, total_pages, bitmap);
    for (int i = 0; i < (BootConfig->MemoryMap.MapSize / BootConfig->MemoryMap.DescriptorSize); i++)
    {
        if (EfiMemory->Type == EfiConventionalMemory)
        {
            write_serial_string("Clearing physical memory mask at ");
            write_serial_hex(EfiMemory->PhysicalStart);
            write_serial_string(" - ");
            write_serial_hex(EfiMemory->PhysicalStart + EfiMemory->NumberOfPages * PAGE_SIZE);
            write_serial_string("\n");
            for (uint64_t idx = (EfiMemory->PhysicalStart / PAGE_SIZE); idx < (EfiMemory->PhysicalStart / PAGE_SIZE) + EfiMemory->NumberOfPages; idx++)
            {
                PAGE_ALLOC.clear_bit(idx);
            }
        }
        EfiMemory++;
    }

    set_bitmap(bitmap, bitmap_size);

    // EfiMemory = (EfiMemoryDesc *)convert_physical_to_virtual((uint64_t)(BootConfig->MemoryMap.Buffer));

    // Setup heap
    uint64_t cr3;
    __asm__ __volatile__("movq  %%cr3,  %0" : "=r"(cr3));
    KERNEL_MAPPER mapper((uint64_t *)convert_physical_to_virtual(cr3));

#define KHEAP_START_VADDR 0xFFFFFFFFC0000000
#define KHEAP_SIZE 64 * 1024 * 1024

    mapper.map_range(KHEAP_START_VADDR, 0, KHEAP_SIZE, 0x3);
    flush_tlb();

    memory_init((void *)KHEAP_START_VADDR, KHEAP_SIZE);

    spin_init(&malloc_lock);

#if BUILD_EDITION == DEBUG_VERSION
    MemoryWasInit = true;
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

    flush_tlb();

    return i;
}
