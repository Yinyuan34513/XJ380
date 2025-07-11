#ifndef _ELF_H_
#define _ELF_H_

#define PT_LOAD 1 // PT_LOAD值不变
#include <efi/efi.h>

typedef struct
{
    uint32_t p_type;   /* Segment type */
    uint32_t p_flags;  /* Segment flags */
    uint64_t p_offset; /* Segment file offset */
    uint64_t p_vaddr;  /* Segment virtual address */
    uint64_t p_paddr;  /* Segment physical address */
    uint64_t p_filesz; /* Segment size in file */
    uint64_t p_memsz;  /* Segment size in memory */
    uint64_t p_align;  /* Segment alignment */
} Elf64_Phdr;

typedef struct
{
    uint8_t e_ident[16];  /* Magic number and other info */
    uint16_t e_type;      /* Object file type */
    uint16_t e_machine;   /* Architecture */
    uint32_t e_version;   /* Object file version */
    uint64_t e_entry;     /* Entry point virtual address */
    uint64_t e_phoff;     /* Program header table file offset */
    uint64_t e_shoff;     /* Section header table file offset */
    uint32_t e_flags;     /* Processor-specific flags */
    uint16_t e_ehsize;    /* ELF header size in bytes */
    uint16_t e_phentsize; /* Program header table entry size */
    uint16_t e_phnum;     /* Program header table entry count */
    uint16_t e_shentsize; /* Section header table entry size */
    uint16_t e_shnum;     /* Section header table entry count */
    uint16_t e_shstrndx;  /* Section header string table index */
} Elf64_Ehdr;

void CalcLoadAddressRange(Elf64_Ehdr *ehdr, uint64_t *first, uint64_t *last)
{
    Elf64_Phdr *phdr = (Elf64_Phdr *)((uint64_t)ehdr + ehdr->e_phoff); // 第一个 program header 地址
    *first = 0xffffffffffffffff;                                     // UINT64最大值
    *last = 0;                                                       // UINT64最小值

    for (uint16_t i = 0; i < ehdr->e_phnum; i++)
    { // 遍历每一个 program header
        if (phdr[i].p_type != PT_LOAD)
            continue; // 只关心LOAD段
        *first = min(*first, phdr[i].p_vaddr);
        *last = max(*last, phdr[i].p_vaddr + phdr[i].p_memsz); // 每一个program header首尾取最值
    }
}

void CopyLoadSegments(Elf64_Ehdr *ehdr)
{
    Elf64_Phdr *phdr = (Elf64_Phdr *)((uint64_t)ehdr + ehdr->e_phoff); // 第一个 program header 地址
    for (uint16_t i = 0; i < ehdr->e_phnum; i++)
    { // 遍历每一个 program header
        if (phdr[i].p_type != PT_LOAD)
            continue; // 只关心LOAD段

        uint64_t segm_in_file = (uint64_t)ehdr + phdr[i].p_offset;                    // 段在文件中的位置
        xmemcpy((void *)phdr[i].p_vaddr, (void *)segm_in_file, phdr[i].p_filesz); // 将文件中大小的部分copy过去
        uint64_t remain_bytes = phdr[i].p_memsz - phdr[i].p_filesz;                  // 两者之差
        xmemset((void *)(phdr[i].p_vaddr + phdr[i].p_filesz), 0, remain_bytes);   // 赋值为0
    }
}
#endif