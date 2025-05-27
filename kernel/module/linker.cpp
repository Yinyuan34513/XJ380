#include "../../include/cinc/elf.h"
#include "../../include/include.h"

bool elf_test_head(Elf64_Ehdr *ehdr)
{
    if (ehdr->e_ident[EI_MAG0] != ELFMAG0 || ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr->e_ident[EI_MAG2] != ELFMAG2 || ehdr->e_ident[EI_MAG3] != ELFMAG3 ||
        ehdr->e_version != EV_CURRENT || ehdr->e_ehsize != sizeof(Elf64_Ehdr) ||
        ehdr->e_phentsize != sizeof(Elf64_Phdr)) {
        return false;
    }

    switch (ehdr->e_machine) {
        case EM_X86_64:
        case EM_386: break;
        default: return false;
    }

    return true;
}

void *resolve_symbol(Elf64_Sym *symtab, uint32_t sym_idx)
{
    return (void *)symtab[sym_idx].st_value;
}

bool handle_relocations(Elf64_Rela *rela_start, Elf64_Sym *symtab, char *strtab, size_t jmprel_sz, uint64_t offset)
{
    Elf64_Rela *rela_plt   = rela_start;
    size_t      rela_count = jmprel_sz / sizeof(Elf64_Rela);

    for (size_t i = 0; i < rela_count; i++) {
        Elf64_Rela *rela        = &rela_plt[i];
        Elf64_Sym  *sym         = &symtab[ELF64_R_SYM(rela->r_info)];
        char       *sym_name    = &strtab[sym->st_name];
        void *func = NULL; //TODO 根据 sym_name 提供的函数名(or方法名)查找函数地址, 并设置进func
        uint64_t   *target_addr = (uint64_t *)(rela->r_offset + offset);
        if (func != NULL) {
            *target_addr = (uint64_t)func;
        } else {
            write_serial_string("Failed relocating ");
            write_serial_string(sym_name);
            write_serial_string(" at ");
            write_serial_dec(rela->r_offset + offset);
            write_serial_string("\n");
            return false;
        }
    }
    return true;
}

void *find_symbol_address(char *symbol_name, Elf64_Sym *symtab, char *strtab, size_t symtabsz,
                          Elf64_Ehdr *ehdr, uint64_t offset)
{
    if (symbol_name == NULL || symtab == NULL || strtab == NULL) return NULL;

    for (size_t i = 0; i < symtabsz; i++) {
        Elf64_Sym *sym      = &symtab[i];
        char      *sym_name = &strtab[sym->st_name];
        if (strcmp(symbol_name, sym_name) == 0) {
            if (sym->st_shndx == SHN_UNDEF) {
                write_serial_string("Symbol ");
                write_serial_string(sym_name);
                write_serial_string(" is undefined.\n");
                return NULL;
            }
            void *addr = (void *)sym->st_value + offset;
            return addr;
        }
    }
    return NULL;
}

void *load_dynamic(Elf64_Phdr *phdrs, Elf64_Ehdr *ehdr, uint64_t offset)
{
    Elf64_Dyn *dyn_entry = NULL;
    for (size_t i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type == PT_DYNAMIC) {
            dyn_entry = (Elf64_Dyn *)(phdrs[i].p_vaddr);
            break;
        }
    }
    if (dyn_entry == NULL) {
        write_serial_string("Dynamic section not found.\n");
        return NULL;
    }
    uint64_t addr_dyn = ((uint64_t)dyn_entry) + offset;
    dyn_entry         = (Elf64_Dyn *)addr_dyn;

    Elf64_Sym  *symtab = NULL;
    char       *strtab = NULL;
    Elf64_Rela *rel    = NULL;
    Elf64_Rela *jmprel = NULL;
    size_t      relsz = 0, symtabsz = 0, jmprel_sz = 0;

    while (dyn_entry->d_tag != DT_NULL) {
        if(dyn_entry->d_tag == DT_SYMTAB){
            uint64_t symtab_addr = dyn_entry->d_un.d_ptr + offset;
            symtab               = (Elf64_Sym *)symtab_addr;
        }
        if(dyn_entry->d_tag == DT_RELA){
            uint64_t rel_addr = dyn_entry->d_un.d_ptr + offset;
            rel               = (Elf64_Rela *)rel_addr;
        }
        if(dyn_entry->d_tag == DT_JMPREL){
            uint64_t jmprel_addr = dyn_entry->d_un.d_ptr + offset;
            jmprel               = (Elf64_Rela *)jmprel_addr;
        }
        if(dyn_entry->d_tag == DT_SYMENT){
            symtabsz = dyn_entry->d_un.d_val;
        }
        if(dyn_entry->d_tag == DT_PLTRELSZ){
            jmprel_sz = dyn_entry->d_un.d_val;
        }
        if(dyn_entry->d_tag == DT_STRTAB){
            strtab = (char *)dyn_entry->d_un.d_ptr + offset;
        }
        if(dyn_entry->d_tag == DT_RELASZ){
            relsz = dyn_entry->d_un.d_val;
        }
        if(dyn_entry->d_tag == DT_PLTGOT) {
            // 需要解析 PLT 表
        }
        dyn_entry++;
    }

    for (size_t i = 0; i < relsz / sizeof(Elf64_Rela); i++) {
        Elf64_Rela *r          = &rel[i];
        uint64_t   *reloc_addr = (uint64_t *)(r->r_offset + offset);
        uint32_t    sym_idx    = ELF64_R_SYM(r->r_info);
        uint32_t    type       = ELF64_R_TYPE(r->r_info);

        if (type == R_X86_64_GLOB_DAT || type == R_X86_64_JUMP_SLOT) {
            *reloc_addr = (uint64_t)resolve_symbol(symtab, sym_idx);
        } else if (type == R_X86_64_RELATIVE) {
            *reloc_addr = (uint64_t)((char *)ehdr + r->r_addend);
        }
    }
    if (!handle_relocations(jmprel, symtab, strtab, jmprel_sz, offset)) { return NULL; }

    void *entry = find_symbol_address("dlmain", symtab, strtab, symtabsz, ehdr, offset);
    if (entry == NULL) {
        write_serial_string("Cannot find dynamic library main function.\n");
        return NULL;
    }

    return entry;
}

void load_segment(Elf64_Phdr *phdr, void *elf, KERNEL_MAPPER *directory, bool is_user, uint64_t offset)
{
    size_t   hi    = PADDING_UP(phdr->p_paddr + phdr->p_memsz, 0x1000) + offset;
    size_t   lo    = PADDING_DOWN(phdr->p_paddr, 0x1000) + offset;
    uint64_t flags = PTE_PRESENT | PTE_WRITEABLE;
    // if (is_user) flags |= PTE_USER;
    if ((phdr->p_flags & PF_R) && !(phdr->p_flags & PF_W)) {
        for (size_t i = lo; i < hi; i += 0x1000) {
           //TODO 页表映射 page_map_to(directory, i, alloc_frames(1), flags);
           /**
            * @param directory 页表目录
            * @param i 虚拟地址
            * @param alloc_frames 分配一个物理页框
            * @param flags 页表标志
            */
        }
    } else
        for (size_t i = lo; i < hi; i += 0x1000) {
            //TODO 页表映射 page_map_to(directory, i, alloc_frames(1), flags);
            /**
             * @param directory 页表目录
             * @param i 虚拟地址
             * @param alloc_frames 分配一个物理页框
             * @param flags 页表标志
             */
        }
    uint64_t          p_vaddr  = (uint64_t)phdr->p_vaddr + offset;
    uint64_t          p_filesz = (uint64_t)phdr->p_filesz;
    uint64_t          p_memsz  = (uint64_t)phdr->p_memsz;
   // KERNEL_MAPPER *dir      = get_current_directory();
   // switch_process_page_directory(directory);
   /* 上述注释掉的代码是切换到改模块的上下文再执行段拷贝操作, 如果共用页表的话不用进行切换 */
    xmemcpy((void *)p_vaddr, elf + phdr->p_offset, p_memsz);

    if (p_memsz > p_filesz) { // 这个是bss段
        xmemset((void *)(p_vaddr + p_filesz), 0, p_memsz - p_filesz);
    }
   // switch_process_page_directory(dir);
}

bool mmap_phdr_segment(Elf64_Ehdr *ehdr, Elf64_Phdr *phdrs, KERNEL_MAPPER *directory,
                       bool is_user, uint64_t offset)
{
    size_t i = 0;
    while (i < ehdr->e_phnum && phdrs[i].p_type != PT_LOAD) {
        i++;
    }
    if (i == ehdr->e_phnum) { return false; }
    for (i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            load_segment(&phdrs[i], (void *)ehdr, directory, is_user, offset);
        }
    }
    return true;
}

void *dlinker_load(VOID *file_addr, UINT64 mmap_offset)
{
    if (file_addr == NULL) return NULL;

    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)file_addr;
    if (!elf_test_head(ehdr)) {
        write_serial_string("No elf file.");
        return NULL;
    }

    Elf64_Phdr phdr[12];
    if (ehdr->e_phnum > sizeof(phdr) / sizeof(phdr[0]) || ehdr->e_phnum < 1) {
        write_serial_string("ELF file has wrong number of program headers\n");
        return NULL;
    }

    if (ehdr->e_type != ET_DYN) {
        write_serial_string("ELF file is not a dynamic library.\n");
        return NULL;
    }

    Elf64_Phdr *phdrs = (Elf64_Phdr *)((char *)ehdr + ehdr->e_phoff);
    if (!mmap_phdr_segment(ehdr, phdrs, NULL/*需要填写页表*/, false, mmap_offset)) {
        write_serial_string("Cannot mmap elf segment.\n");
        return NULL;
    }

    void *dlmain = load_dynamic(phdrs, ehdr, mmap_offset);
    if (dlmain == NULL) {
        write_serial_string("Cannot load dynamic section.\n");
        return NULL;
    }
    /* TODO
     * dlmain 是动态库的入口函数指针, 可自行决定如何调用执行
     */
    return dlmain;
}


