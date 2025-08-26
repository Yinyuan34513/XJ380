#include "../../include/include.h"
#include "../fs/vfs.h"
uint64_t sys_brk(uint64_t addr)
{
    unsigned long new_brk = (addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    if (new_brk == 0)
        return current_task->mm->start_brk;
    if (new_brk < current_task->mm->end_brk) // release  brk space
        return 0;

    new_brk = mm_do_brk(current_task->mm->end_brk, new_brk - current_task->mm->end_brk); // expand brk space

    current_task->mm->end_brk = new_brk;
    return new_brk;
}

uint64_t sys_mmap(uint64_t addr, uint64_t len, uint64_t prot, uint64_t flags, uint64_t fd, uint64_t offset)
{
    if (addr == 0)
    {
        return (uint64_t)-EINVAL;
    }

    uint64_t count = (len + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t vaddr = addr;

    KERNEL_MAPPER mapper((uint64_t *)convert_physical_to_virtual((uint64_t)current_task->mm->pgd));

    for (uint64_t i = 0; i < count; i++)
    {
        uint64_t page = vaddr + PAGE_SIZE * i;
        uint64_t flags = (1 << 0) | (1 << 2);

        if (prot & PROT_WRITE)
        {
            flags |= (1 << 1);
        }

        // Unsupported flags
        // if (!(prot & PROT_EXEC))
        // {
        //     flags |= (1 << 63);
        // }

        mapper.map_page(page, 0, flags);
    }

    flush_tlb();

    if (fd > 2 && fd < MAX_FD_NUM)
    {
        vfs_node_t file = (vfs_node_t)current_task->fds[fd];
        if (offset > file->size)
        {
            return (uint64_t)-EINVAL;
        }
        file->offset = offset;
        vfs_read(file, (void *)vaddr, file->offset, len);
    }

    return vaddr;
}
