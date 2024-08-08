#ifndef _ELF_H_
#define _ELF_H_

#include "efi.h"

#define PT_LOAD 1 // PT_LOAD值不变

typedef struct
{
    UINT32 p_type;        /* Segment type */
    UINT32 p_flags;       /* Segment flags */
    UINT64 p_offset;      /* Segment file offset */
    UINT64 p_vaddr;       /* Segment virtual address */
    UINT64 p_paddr;       /* Segment physical address */
    UINT64 p_filesz;      /* Segment size in file */
    UINT64 p_memsz;       /* Segment size in memory */
    UINT64 p_align;       /* Segment alignment */
} Elf64_Phdr;

typedef struct
{
    UINT8  e_ident[16];   /* Magic number and other info */
    UINT16 e_type;        /* Object file type */
    UINT16 e_machine;     /* Architecture */
    UINT32 e_version;     /* Object file version */
    UINT64 e_entry;       /* Entry point virtual address */
    UINT64 e_phoff;       /* Program header table file offset */
    UINT64 e_shoff;       /* Section header table file offset */
    UINT32 e_flags;       /* Processor-specific flags */
    UINT16 e_ehsize;      /* ELF header size in bytes */
    UINT16 e_phentsize;   /* Program header table entry size */
    UINT16 e_phnum;       /* Program header table entry count */
    UINT16 e_shentsize;   /* Section header table entry size */
    UINT16 e_shnum;       /* Section header table entry count */
    UINT16 e_shstrndx;    /* Section header string table index */
} Elf64_Ehdr;

#endif