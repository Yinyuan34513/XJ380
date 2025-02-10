#include "./alloc/page.h"
#include "../include/include.h"

KERNEL_MAPPER::KERNEL_MAPPER(uint64_t *page_table_addr) : page_table(page_table_addr)
{
}

KERNEL_MAPPER::~KERNEL_MAPPER()
{
}

void KERNEL_MAPPER::map_page(uint64_t vaddr, uint64_t paddr, uint64_t flags)
{
    uint64_t *page_table_addr = this->page_table;

    // Map paddr to vaddr
    uint64_t *pml4 = (uint64_t *)page_table_addr;
    uint64_t pml4_id = (vaddr >> 39) & 0x1FF;
    if (pml4[pml4_id] == 0)
    {
        uint64_t pml4_addr = (uint64_t)PAGE_ALLOC.alloc_page();
        xmemset((void *)convert_physical_to_virtual(pml4_addr), 0, PAGE_SIZE);
        pml4[pml4_id] = pml4_addr | flags;
    }
    uint64_t *pdpt = (uint64_t *)convert_physical_to_virtual(pml4[pml4_id] & ~(0xFFF));
    uint64_t pdpt_id = (vaddr >> 30) & 0x1FF;
    if (pdpt[pdpt_id] == 0)
    {
        uint64_t pdpt_addr = (uint64_t)PAGE_ALLOC.alloc_page();
        xmemset((void *)convert_physical_to_virtual(pdpt_addr), 0, PAGE_SIZE);
        pdpt[pdpt_id] = pdpt_addr | flags;
    }
    uint64_t *pd = (uint64_t *)convert_physical_to_virtual(pdpt[pdpt_id] & ~(0xFFF));
    uint64_t pd_id = (vaddr >> 21) & 0x1FF;
    if (pd[pd_id] == 0)
    {
        uint64_t pd_addr = (uint64_t)PAGE_ALLOC.alloc_page();
        xmemset((void *)convert_physical_to_virtual(pd_addr), 0, PAGE_SIZE);
        pd[pd_id] = pd_addr | flags;
    }
    uint64_t *pt = (uint64_t *)convert_physical_to_virtual(pd[pd_id] & ~(0xFFF));
    uint64_t pt_id = (vaddr >> 12) & 0x1FF;
    if (pt[pt_id] == 0)
    {
        if (paddr == 0)
        {
            pt[pt_id] = (uint64_t)PAGE_ALLOC.alloc_page() | flags;
        }
        else
        {
            pt[pt_id] = paddr | flags;
        }
    }
}

void KERNEL_MAPPER::unmap_page(uint64_t vaddr)
{
    uint64_t *pml4 = this->page_table;

    uint64_t pml4_id = (vaddr >> 39) & 0x1FF;
    uint64_t *pdpt = (uint64_t *)convert_physical_to_virtual(pml4[pml4_id] & ~(0xFFF));
    uint64_t pdpt_id = (vaddr >> 30) & 0x1FF;
    uint64_t *pd = (uint64_t *)convert_physical_to_virtual(pdpt[pdpt_id] & ~(0xFFF));
    uint64_t pd_id = (vaddr >> 21) & 0x1FF;

    PAGE_ALLOC.free_page((void *)pd[pd_id]);
    pd[pd_id] = 0;
    PAGE_ALLOC.free_page((void *)pd);
    pdpt[pdpt_id] = 0;
    PAGE_ALLOC.free_page((void *)pdpt);
    pml4[pml4_id] = 0;
    PAGE_ALLOC.free_page((void *)pml4);
}

void KERNEL_MAPPER::map_range(uint64_t vaddr, uint64_t paddr, uint64_t size, uint64_t flags)
{
    for (uint64_t i = 0; i < size; i += PAGE_SIZE)
    {
        if (paddr == 0)
        {
            map_page(vaddr + i, 0, flags);
        }
        else
        {
            map_page(vaddr + i, paddr + i, flags);
        }
    }
}

void KERNEL_MAPPER::unmap_range(uint64_t vaddr, uint64_t size)
{
    for (uint64_t i = 0; i < size; i += PAGE_SIZE)
    {
        unmap_page(vaddr + i);
    }
}
