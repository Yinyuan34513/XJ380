#include "../include/include.h"
#include "./alloc/page.h"

KERNEL_MAPPER::KERNEL_MAPPER(uint64_t *page_table_addr) : page_table(page_table_addr)
{
}

KERNEL_MAPPER::~KERNEL_MAPPER()
{
}

uint64_t KERNEL_MAPPER::map_page(uint64_t vaddr, uint64_t paddr, uint64_t flags)
{
    uint64_t *page_table_addr = this->page_table;

    // Map paddr to vaddr
    uint64_t *pml4 = (uint64_t *)page_table_addr;
    uint64_t pml4_id = (vaddr >> 39) & 0x1FFUL;
    if (pml4[pml4_id] == 0)
    {
        uint64_t pml4_addr = (uint64_t)PAGE_ALLOC.alloc_page();
        xmemset((void *)convert_physical_to_virtual(pml4_addr), 0, PAGE_SIZE);
        pml4[pml4_id] = pml4_addr | flags;
    }
    uint64_t *pdpt = (uint64_t *)convert_physical_to_virtual(pml4[pml4_id] & ~(0xFFFUL));
    uint64_t pdpt_id = (vaddr >> 30) & 0x1FFUL;
    if (pdpt[pdpt_id] == 0)
    {
        uint64_t pdpt_addr = (uint64_t)PAGE_ALLOC.alloc_page();
        xmemset((void *)convert_physical_to_virtual(pdpt_addr), 0, PAGE_SIZE);
        pdpt[pdpt_id] = pdpt_addr | flags;
    }
    uint64_t *pd = (uint64_t *)convert_physical_to_virtual(pdpt[pdpt_id] & ~(0xFFFUL));
    uint64_t pd_id = (vaddr >> 21) & 0x1FFUL;
    if (pd[pd_id] == 0)
    {
        uint64_t pd_addr = (uint64_t)PAGE_ALLOC.alloc_page();
        xmemset((void *)convert_physical_to_virtual(pd_addr), 0, PAGE_SIZE);
        pd[pd_id] = pd_addr | flags;
    }
    uint64_t *pt = (uint64_t *)convert_physical_to_virtual(pd[pd_id] & ~(0xFFFUL));
    uint64_t pt_id = (vaddr >> 12) & 0x1FFUL;
    if (paddr == 0)
    {
        uint64_t phys = (uint64_t)PAGE_ALLOC.alloc_page();
        pt[pt_id] = phys | flags;
        return phys;
    }
    else
    {
        pt[pt_id] = paddr | flags;
        return paddr;
    }

    return 0;
}

void KERNEL_MAPPER::unmap_page(uint64_t vaddr)
{
    uint64_t *pml4 = this->page_table;

    uint64_t pml4_id = (vaddr >> 39) & 0x1FFUL;
    uint64_t *pdpt = (uint64_t *)convert_physical_to_virtual(pml4[pml4_id] & ~(0xFFFUL));
    uint64_t pdpt_id = (vaddr >> 30) & 0x1FFUL;
    uint64_t *pd = (uint64_t *)convert_physical_to_virtual(pdpt[pdpt_id] & ~(0xFFFUL));
    uint64_t pd_id = (vaddr >> 21) & 0x1FFUL;

    PAGE_ALLOC.free_page((void *)pd[pd_id]);
    pd[pd_id] = 0;
    PAGE_ALLOC.free_page((void *)pd);
    pdpt[pdpt_id] = 0;
    PAGE_ALLOC.free_page((void *)pdpt);
    pml4[pml4_id] = 0;
    PAGE_ALLOC.free_page((void *)pml4);
}

uint64_t KERNEL_MAPPER::map_range(uint64_t vaddr, uint64_t paddr, uint64_t size, uint64_t flags)
{
    uint64_t start_phys = 0;
    vaddr = vaddr & 0xFFFFFFFFFFFFF000;
    paddr = paddr & 0xFFFFFFFFFFFFF000;

    for (uint64_t i = 0; i < size; i += PAGE_SIZE)
    {
        uint64_t ret = 0;
        if (paddr == 0)
        {
            ret = map_page(vaddr + i, 0, flags);
        }
        else
        {
            ret = map_page(vaddr + i, paddr + i, flags);
        }

        if (!start_phys)
        {
            start_phys = ret;
        }
    }

    return start_phys;
}

void KERNEL_MAPPER::unmap_range(uint64_t vaddr, uint64_t size)
{
    for (uint64_t i = 0; i < size; i += PAGE_SIZE)
    {
        unmap_page(vaddr + i);
    }
}

uint64_t KERNEL_MAPPER::translate_address(uint64_t virtual_address)
{
    uint64_t offset = virtual_address & 0xFFF;

    uint64_t *page_table_addr = this->page_table;

    uint64_t *pml4 = (uint64_t *)page_table_addr;
    uint64_t pml4_id = (virtual_address >> 39) & 0x1FFUL;
    if ((pml4[pml4_id] & (1 << 0)) == 0)
    {
        return 0;
    }
    uint64_t *pdpt = (uint64_t *)convert_physical_to_virtual(pml4[pml4_id] & ~(0xFFFUL));
    uint64_t pdpt_id = (virtual_address >> 30) & 0x1FFUL;
    if ((pdpt[pdpt_id] & (1 << 0)) == 0)
    {
        return 0;
    }
    uint64_t *pd = (uint64_t *)convert_physical_to_virtual(pdpt[pdpt_id] & ~(0xFFFUL));
    uint64_t pd_id = (virtual_address >> 21) & 0x1FFUL;
    if ((pd[pd_id] & (1 << 0)) == 0)
    {
        return 0;
    }
    // if (pd[pd_id] & (1 << 7))
    // {
    //     return (pd[pd_id] & ((1UL << 21) - 1)) + (virtual_address & ((1UL << 21) - 1));
    // }
    uint64_t *pt = (uint64_t *)convert_physical_to_virtual(pd[pd_id] & ~(0xFFFUL));
    uint64_t pt_id = (virtual_address >> 12) & 0x1FFUL;
    if ((pt[pt_id] & (1 << 0)) == 0)
    {
        return 0;
    }

    return (pt[pt_id] & 0x00007FFFFFFFF000) + offset;
}
