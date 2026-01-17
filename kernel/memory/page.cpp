#include <cpu/lock.h>
#include <cpu/longm.h>
#include <cpu/regio.h>
#include <dlinker.h>
#include <krlibc.h>
#include <mm/lazyalloc.h>
#include <mm/page.h>
#include <proto.hpp>
#include <stdint.h>

spin_t page_lock;

page_directory_t  kernel_page_dir;
page_directory_t *current_directory = NULL;

extern bool     no_interrupt;
extern uint64_t memory_size;
uint64_t        double_fault_page = 0;
page_t *page_maps;
void *early_alloc(size_t size);
void            page_init()
{
    uint64_t page_maps_size = memory_size / PAGE_SIZE * sizeof(page_t);
    page_maps               = (page_t*)early_alloc(page_maps_size);
    if (page_maps) memset(page_maps, 0, page_maps_size);
}

page_t *get_page(uint64_t addr)
{
    return page_maps ? page_maps + (addr / PAGE_SIZE) : NULL;
}

void page_ref(page_t *page)
{
    if (page) page->refcount++;
}
void page_unref(page_t *page)
{
    if (page) page->refcount--;
}

bool page_can_free(page_t *page)
{
    return page ? page->refcount <= 0 : true;
}

void address_ref(uint64_t addr)
{
    page_ref(get_page(addr));
}
void address_unref(uint64_t addr)
{
    page_unref(get_page(addr));
}

bool address_can_free(uint64_t addr)
{
    return page_can_free(get_page(addr));
}

static bool is_huge_page(page_table_entry_t *entry)
{
    return (((uint64_t)entry->value) & PTE_HUGE) != 0;
}

extern "C" void handle_page_fault(struct X64_REGS *frame, uint64_t error_code)
{
    disable_intr();
    disable_scheduler();
    uint64_t faulting_address;
    __asm__ volatile("mov %%cr2, %0" : "=r"(faulting_address));

    const char *error_msg = !(error_code & 0x1) ? "NotPresent"
                            : error_code & 0x2  ? "WriteError"
                            : error_code & 0x4  ? "UserMode"
                            : error_code & 0x8  ? "ReservedBitsSet"
                            : error_code & 0x10 ? "DecodeAddress"
                                                : "Unknown";

    // 长大后在学习吧！
    if (get_current_task() != NULL)
    {
        pcb_t current_proc = get_current_task()->parent_group;
        if (current_proc->task_level == TASK_APPLICATION_LEVEL)
        {
            errno_t status = lazy_tryalloc(current_proc, faulting_address);
            if (status != 0) { goto err; }
            else
            {
                enable_scheduler();
                open_interrupt;
                return;
            }
        }
    }
err:;
    write_serial_string("Page fault virtual address (cr2): ");
    write_serial_hex(faulting_address);
    write_serial_string(" ");
    write_serial_hex(frame->rip);
    write_serial_string("\n");

    write_serial_string("Type: ");
    write_serial_string(error_msg);
    write_serial_string("\n");

    write_serial_string("Fault Thread Name: ");
    write_serial_string(get_current_task()->name);
    write_serial_string(" ");
    write_serial_hex((uint64_t)get_current_task());
    write_serial_string("\n");

    backtrace(frame);

    while (1)
    {
        __asm__ __volatile__("hlt");
    }
}

void page_table_clear(page_table_t *table)
{
    for (int i = 0; i < 512; i++)
    {
        table->entries[i].value = 0;
    }
}

bool page_table_get_flags(page_directory_t *directory, uint64_t addr, uint64_t *out_flags)
{
    uint64_t l4_index = (((addr >> 39)) & 0x1FF);
    uint64_t l3_index = (((addr >> 30)) & 0x1FF);
    uint64_t l2_index = (((addr >> 21)) & 0x1FF);
    uint64_t l1_index = (((addr >> 12)) & 0x1FF);

    page_table_t *pml4 = directory->table;

    if (!(pml4->entries[l4_index].value & PTE_PRESENT)) return false;
    if (pml4->entries[l4_index].value & PTE_HUGE)
    {
        *out_flags = pml4->entries[l4_index].value & 0xFFF;
        return true;
    }

    page_table_t *pdpt = (page_table_t *)(pml4->entries[l4_index].value & PAGE_MASK);
    if (!(pdpt->entries[l3_index].value & PTE_PRESENT)) return false;
    if (pdpt->entries[l3_index].value & PTE_HUGE)
    {
        *out_flags = pdpt->entries[l3_index].value & 0xFFF;
        return true;
    }

    page_table_t *pd = (page_table_t *)(pdpt->entries[l3_index].value & PAGE_MASK);
    if (!(pd->entries[l2_index].value & PTE_PRESENT)) return false;
    if (pd->entries[l2_index].value & PTE_HUGE)
    {
        *out_flags = pd->entries[l2_index].value & 0xFFF;
        return true;
    }

    page_table_t *pt = (page_table_t *)(pd->entries[l2_index].value & PAGE_MASK);
    if (!(pt->entries[l1_index].value & PTE_PRESENT)) return false;

    *out_flags = pt->entries[l1_index].value & 0xFFF;
    return true;
}

bool page_table_update_flags(page_directory_t *directory, uint64_t addr, uint64_t new_flags)
{
    page_table_t *pml4     = directory->table;
    uint64_t      l4_index = (((addr >> 39)) & 0x1FF);
    uint64_t      l3_index = (((addr >> 30)) & 0x1FF);
    uint64_t      l2_index = (((addr >> 21)) & 0x1FF);
    uint64_t      l1_index = (((addr >> 12)) & 0x1FF);

    if (!(pml4->entries[l4_index].value & PTE_PRESENT)) return false;
    if (pml4->entries[l4_index].value & PTE_HUGE)
    {
        uint64_t phys                 = pml4->entries[l4_index].value & PAGE_MASK;
        pml4->entries[l4_index].value = phys | (new_flags & 0xFFF);
        return true;
    }

    page_table_t *pdpt = (page_table_t *)(pml4->entries[l4_index].value & PAGE_MASK);
    if (!(pdpt->entries[l3_index].value & PTE_PRESENT)) return false;
    if (pdpt->entries[l3_index].value & PTE_HUGE)
    {
        uint64_t phys                 = pdpt->entries[l3_index].value & PAGE_MASK;
        pdpt->entries[l3_index].value = phys | (new_flags & 0xFFF);
        return true;
    }

    page_table_t *pd = (page_table_t *)(pdpt->entries[l3_index].value & PAGE_MASK);
    if (!(pd->entries[l2_index].value & PTE_PRESENT)) return false;
    if (pd->entries[l2_index].value & PTE_HUGE)
    {
        uint64_t phys               = pd->entries[l2_index].value & PAGE_MASK;
        pd->entries[l2_index].value = phys | (new_flags & 0xFFF);
        return true;
    }

    page_table_t *pt = (page_table_t *)(pd->entries[l2_index].value & PAGE_MASK);
    if (!(pt->entries[l1_index].value & PTE_PRESENT)) return false;

    uint64_t phys               = pt->entries[l1_index].value & PAGE_MASK;
    pt->entries[l1_index].value = phys | (new_flags & 0xFFF);

    __asm__ volatile("invlpg (%0)" ::"r"(addr) : "memory");
    return true;
}

page_table_t *page_table_create(page_table_entry_t *entry, bool user)
{
    if (entry->value == (uint64_t)NULL)
    {
        uint64_t frame = alloc_frames(1);
        // write_serial_fmt("oiiaoiia 0x%x\n",(uint64_t)frame);
        entry->value        = frame | PTE_PRESENT | PTE_WRITEABLE | (user ? PTE_USER : 0);
        page_table_t *table = (page_table_t *)phys_to_virt(entry->value & 0x00007fffffff000);
        page_table_clear(table);
        return table;
    }
    entry->value        |= (user ? PTE_USER : 0);
    page_table_t *table  = (page_table_t *)phys_to_virt(entry->value & 0x00007fffffff000);
    return table;
}

page_directory_t *get_kernel_pagedir()
{
    return &kernel_page_dir;
}

extern volatile bool is_scheduler;

page_directory_t gdc_temp_pdt;

page_directory_t *get_current_directory()
{
    gdc_temp_pdt.table = (page_table_t *)phys_to_virt(get_cr3());
    return &gdc_temp_pdt;
}

EXPORT_SYMBOL(get_current_directory);

static page_table_t *copy_page_table_recursive(page_table_t *source_table, int level, bool all_copy, bool kernel_space)
{
    if (source_table == NULL) return NULL;
    if (level == 0)
    {
        if (kernel_space) { return source_table; }

        uint64_t      frame          = alloc_frames(1);
        page_table_t *new_page_table = (page_table_t *)phys_to_virt(frame);
        memcpy(new_page_table, source_table->entries, PAGE_SIZE);
        return new_page_table;
    }

    uint64_t      phy_frame = alloc_frames(1);
    page_table_t *new_table = (page_table_t *)(phys_to_virt(phy_frame));
    for (uint64_t i = 0; i < (all_copy ? 512 : (level == 4 ? 256 : 512)); i++)
    {
        if (source_table->entries[i].value & PTE_HUGE)
        {
            // 指向一块2M的页
            new_table->entries[i].value = source_table->entries[i].value;
            continue;
        }

        // 指向一个4K的页表
        page_table_t *source_page_table_next =
            (page_table_t *)phys_to_virt(source_table->entries[i].value & 0x00007fffffff000);
        page_table_t *new_page_table = copy_page_table_recursive(source_page_table_next, level - 1, all_copy,
                                                                 level != 4 ? kernel_space : i >= 256);
        new_table->entries[i].value =
            (uint64_t)virt_to_phys((uint64_t)new_page_table) | (source_table->entries[i].value & 0xFFFF000000000FFF);
    }
    return new_table;
}

static void free_page_table_recursive(page_table_t *table, int level)
{
    if (table == NULL) return;
    if (level == 0)
    {
        free_frame((uint64_t)virt_to_phys((uint64_t)table));
        return;
    }

    for (int i = 0; i < (level == 4 ? 256 : 512); i++)
    {
        page_table_t *page_table_next = (page_table_t *)(phys_to_virt(table->entries[i].value & 0x00007fffffff000));
        free_page_table_recursive(page_table_next, level - 1);
    }
    free_frame((uint64_t)virt_to_phys((uint64_t)table));
}

page_directory_t *clone_page_directory(page_directory_t *dir, bool all_copy)
{
    spin_lock(page_lock);
    page_directory_t *new_directory = (page_directory_t *)(malloc(sizeof(page_directory_t)));
    if (new_directory == NULL) return NULL;
    new_directory->table = copy_page_table_recursive(dir->table, 4, all_copy, false);
    if (!all_copy) memcpy((uint64_t *)new_directory->table + 256, (uint64_t *)dir->table + 256, PAGE_SIZE / 2);
    spin_unlock(page_lock);
    return new_directory;
}

void free_page_directory(page_directory_t *dir)
{
    spin_lock(page_lock);
    free_page_table_recursive(dir->table, 4);
    free(dir);
    spin_unlock(page_lock);
}

void unmap_page(page_directory_t *directory, uint64_t vaddr)
{
    uint64_t l4_index = (((vaddr >> 39)) & 0x1FF);
    uint64_t l3_index = (((vaddr >> 30)) & 0x1FF);
    uint64_t l2_index = (((vaddr >> 21)) & 0x1FF);
    uint64_t l1_index = (((vaddr >> 12)) & 0x1FF);

    page_table_t *l4_table = directory->table;
    page_table_t *l3_table =
        (page_table_t *)(phys_to_virt((&(l4_table->entries[l4_index]))->value & 0x00007fffffff000));
    page_table_t *l2_table =
        (page_table_t *)(phys_to_virt((&(l3_table->entries[l3_index]))->value & 0x00007fffffff000));
    page_table_t *l1_table =
        (page_table_t *)(phys_to_virt((&(l2_table->entries[l2_index]))->value & 0x00007fffffff000));

    free_frame(l1_table->entries[l1_index].value & 0x00007fffffff000);
    l1_table->entries[l1_index].value = 0;
    flush_tlb(vaddr);
}

void unmap_page_range(page_directory_t *directory, uint64_t vaddr, uint64_t size)
{
    for (uint64_t va = vaddr; va < vaddr + size; va += PAGE_SIZE)
    {
        unmap_page(directory, va);
    }
}

void page_map_to(page_directory_t *directory, uint64_t addr, uint64_t frame, uint64_t flags)
{
    uint64_t l4_index = (((addr >> 39)) & 0x1FF);
    uint64_t l3_index = (((addr >> 30)) & 0x1FF);
    uint64_t l2_index = (((addr >> 21)) & 0x1FF);
    uint64_t l1_index = (((addr >> 12)) & 0x1FF);

    page_table_t *l4_table = directory->table;
    page_table_t *l3_table = page_table_create(&(l4_table->entries[l4_index]), !!(flags & PTE_USER));
    page_table_t *l2_table = page_table_create(&(l3_table->entries[l3_index]), !!(flags & PTE_USER));
    page_table_t *l1_table = page_table_create(&(l2_table->entries[l2_index]), !!(flags & PTE_USER));

    if (!frame) { serial_wprintf("WARNING: Invalid frame addr: 0\n"); }

    l1_table->entries[l1_index].value = (frame & 0x00007fffffff000) | flags;

    flush_tlb(addr);
}

EXPORT_SYMBOL(page_map_to);

void switch_process_page_directory(page_directory_t *dir)
{
    disable_scheduler();
    close_interrupt;
    pcb_t pcb    = get_current_task()->parent_group;
    pcb->pagedir = dir;
    switch_page_directory(dir);
    if (!no_interrupt)
    {
        enable_scheduler();
        open_interrupt;
    }
}

void switch_page_directory(page_directory_t *dir)
{
    // if (cpu->ready) {
    //     cpu->directory = dir;
    // } else {
    current_directory = dir;
    // }
    page_table_t *physical_table = (page_table_t *)(virt_to_phys((uint64_t)((page_table_t *)((uint64_t)dir->table))));
    // write_serial_string("Switch Page Directory:\nPhysAddr:");
    // write_serial_hex((uint64_t)(physical_table));
    // write_serial_string("\nVirtAddr:");
    // write_serial_hex((uint64_t)(phys_to_virt((uint64_t)(physical_table))));
    // write_serial_string("\n");
    __asm__ volatile("mov %0, %%cr3" : : "r"(physical_table));
}

uint64_t page_alloc_random(page_directory_t *directory, uint64_t length, uint64_t flags)
{
    if (length == 0) return -1;
    size_t   p    = length / PAGE_SIZE;
    uint64_t addr = alloc_frames(p == 0 ? 1 : p);
    for (uint64_t i = 0; i < length; i += 0x1000)
    {
        uint64_t var = (uint64_t)addr + i;
        page_map_to(directory, var, var, flags);
    }
    return addr;
}

void page_map_range_to_random(page_directory_t *directory, uint64_t addr, uint64_t length, uint64_t flags)
{
    for (uint64_t i = 0; i < length; i += 0x1000)
    {
        uint64_t var = (uint64_t)addr + i;
        page_map_to(directory, var, alloc_frames(1), flags);
    }
}

void page_map_range_to(page_directory_t *directory, uint64_t frame, uint64_t length, uint64_t flags)
{
    for (uint64_t i = 0; i < length; i += 0x1000)
    {
        uint64_t var = (uint64_t)phys_to_virt(frame + i);
        page_map_to(directory, var, frame + i, flags);
    }
}

void page_map_range(page_directory_t *directory, uint64_t addr, uint64_t frame, uint64_t length, uint64_t flags)
{
    for (uint64_t i = 0; i < length; i += 0x1000)
    {
        uint64_t var = (uint64_t)addr + i;
        page_map_to(directory, var, frame + i, flags);
    }
}

uint64_t translate_address(page_directory_t *page_dir, uint64_t virtual_address)
{
    // 计算各级页表索引
    uint64_t pml4_index = (virtual_address >> 39) & ENTRY_MASK;
    uint64_t pdp_index  = (virtual_address >> 30) & ENTRY_MASK;
    uint64_t pd_index   = (virtual_address >> 21) & ENTRY_MASK;
    uint64_t pt_index   = (virtual_address >> 12) & ENTRY_MASK;

    // 获取PML4表
    page_table_t      *pml4_table = page_dir->table;
    page_table_entry_t pml4_entry = pml4_table->entries[pml4_index];

    // 检查PML4条目是否存在
    if (!(pml4_entry.value & PTE_PRESENT)) { return 0; }

    // 如果PML4条目指向的是1GB大页
    if (ARCH_PT_IS_LARGE(pml4_entry.value))
    {
        uint64_t base_addr = pml4_entry.value & ~((1UL << 30) - 1);
        return base_addr + (virtual_address & ((1UL << 30) - 1));
    }

    // 获取PDPT表
    page_table_t      *pdpt_table = (page_table_t *)phys_to_virt(pml4_entry.value & PAGE_MASK);
    page_table_entry_t pdpt_entry = pdpt_table->entries[pdp_index];

    // 检查PDPT条目是否存在
    if (!(pdpt_entry.value & PTE_PRESENT)) { return 0; }

    // 如果PDPT条目指向的是1GB大页
    if (ARCH_PT_IS_LARGE(pdpt_entry.value))
    {
        uint64_t base_addr = pdpt_entry.value & ~((1UL << 30) - 1);
        return base_addr + (virtual_address & ((1UL << 30) - 1));
    }

    // 获取PD表
    page_table_t      *pd_table = (page_table_t *)phys_to_virt(pdpt_entry.value & PAGE_MASK);
    page_table_entry_t pd_entry = pd_table->entries[pd_index];

    // 检查PD条目是否存在
    if (!(pd_entry.value & PTE_PRESENT)) { return 0; }

    // 如果PD条目指向的是2MB大页
    if (ARCH_PT_IS_LARGE(pd_entry.value))
    {
        uint64_t base_addr = pd_entry.value & ~((1UL << 21) - 1);
        return base_addr + (virtual_address & ((1UL << 21) - 1));
    }

    // 获取PT表
    page_table_t      *pt_table = (page_table_t *)phys_to_virt(pd_entry.value & PAGE_MASK);
    page_table_entry_t pt_entry = pt_table->entries[pt_index];

    // 检查PT条目是否存在
    if (!(pt_entry.value & PTE_PRESENT)) { return 0; }

    // 返回4KB页的物理地址
    return (pt_entry.value & PAGE_MASK) + (virtual_address & ~PAGE_MASK);
}

void switch_xsk_page_directory()
{
    page_directory_t *new_directory = clone_page_directory(&kernel_page_dir, true);
    kernel_page_dir.table           = new_directory->table;
    free(new_directory);
    switch_page_directory(&kernel_page_dir);
    double_fault_page = get_cr3();
    current_directory = &kernel_page_dir;
}

void page_setup()
{
    page_table_t *kernel_page_table = (page_table_t *)phys_to_virt(get_cr3());
    kernel_page_dir                 = (page_directory_t){.table = kernel_page_table};
    double_fault_page               = get_cr3();
    current_directory               = &kernel_page_dir;
}