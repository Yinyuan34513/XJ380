#include <proto.hpp>
#include <dlinker.h>
#include <fs/vfs/vfs.h>
#include <elf.h>
#include <id_alloc.h>

uint64_t kernel_modules_load_offset = 0;

extern dlfunc_t __ksymtab_start[]; // .ksymtab section
extern dlfunc_t __ksymtab_end[];
size_t          dlfunc_count = 0;

char fmt_buf[4096];

void *resolve_symbol(Elf64_Sym *symtab, uint32_t sym_idx) {
    return (void *)symtab[sym_idx].st_value;
}

bool handle_relocations(Elf64_Rela *rela_start, Elf64_Sym *symtab, char *strtab, size_t jmprel_sz,
                        uint64_t offset) {
    Elf64_Rela *rela_plt   = rela_start;
    size_t      rela_count = jmprel_sz / sizeof(Elf64_Rela);

    for (size_t i = 0; i < rela_count; i++) {
        Elf64_Rela *rela        = &rela_plt[i];
        Elf64_Sym  *sym         = &symtab[ELF64_R_SYM(rela->r_info)];
        char       *sym_name    = &strtab[sym->st_name];
        write_serial_fmt("sym name %s\n",sym_name);
        dlfunc_t   *func        = find_func(sym_name);
        uint64_t   *target_addr = (uint64_t *)(rela->r_offset + offset);
        if (func != NULL) {
            *target_addr = (uint64_t)func->addr;
        } else {
            write_serial_fmt("Failed relocating %s at %p\n", sym_name, rela->r_offset + offset);
            return false;
        }
    }
    return true;
}

void *find_symbol_address(const char *symbol_name, Elf64_Ehdr *ehdr, uint64_t offset) {
    if (symbol_name == NULL || ehdr == NULL) return NULL;

    Elf64_Sym *symtab = NULL;
    char      *strtab = NULL;

    Elf64_Shdr *shdrs    = (Elf64_Shdr *)((char *)ehdr + ehdr->e_shoff);
    char       *shstrtab = (char *)ehdr + shdrs[ehdr->e_shstrndx].sh_offset;

    size_t symtabsz = 0;

    for (int i = 0; i < ehdr->e_shnum; i++) {
        if (shdrs[i].sh_type == SHT_SYMTAB || shdrs[i].sh_type == SHT_DYNSYM) {
            symtab   = (Elf64_Sym *)((char *)ehdr + shdrs[i].sh_offset);
            symtabsz = shdrs[i].sh_size;
            strtab   = (char *)ehdr + shdrs[shdrs[i].sh_link].sh_offset;
            break;
        }
    }

    size_t num_symbols = symtabsz / sizeof(Elf64_Sym);

    for (size_t i = 0; i < num_symbols; i++) {
        Elf64_Sym *sym      = &symtab[i];
        char      *sym_name = &strtab[sym->st_name];
        if (strcmp(symbol_name, sym_name) == 0) {
            if (sym->st_shndx == SHN_UNDEF) {
                write_serial_fmt("Symbol %s is undefined.\n", sym_name);
                return NULL;
            }
            void *addr = (void*)((uint64_t)sym->st_value + offset);
            return addr;
        }
    }
    write_serial_fmt("Cannot find symbol %s in ELF file.\n", symbol_name);
    return NULL;
}

dlinit_t load_dynamic(kernel_mode_t *kmod,Elf64_Phdr *phdrs, Elf64_Ehdr *ehdr, uint64_t offset) {
    Elf64_Dyn *dyn_entry = NULL;
    for (size_t i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type == PT_DYNAMIC) {
            dyn_entry = (Elf64_Dyn *)(phdrs[i].p_vaddr);
            break;
        }
    }
    if (dyn_entry == NULL) {
        write_serial_fmt("Dynamic section not found.\n");
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
        switch (dyn_entry->d_tag) {
        case DT_SYMTAB:
            {uint64_t symtab_addr = dyn_entry->d_un.d_ptr + offset;
            symtab               = (Elf64_Sym *)symtab_addr;
            break;}
        case DT_STRTAB: strtab = (char *)dyn_entry->d_un.d_ptr + offset; break;
        case DT_RELA:
            {uint64_t rel_addr = dyn_entry->d_un.d_ptr + offset;
            rel               = (Elf64_Rela *)rel_addr;
            break;}
        case DT_RELASZ: relsz = dyn_entry->d_un.d_val; break;
        case DT_JMPREL:
            {uint64_t jmprel_addr = dyn_entry->d_un.d_ptr + offset;
            jmprel               = (Elf64_Rela *)jmprel_addr;
            break;}
        case DT_SYMENT: symtabsz = dyn_entry->d_un.d_val; break;
        case DT_PLTRELSZ: jmprel_sz = dyn_entry->d_un.d_val; break;
        case DT_PLTGOT: /* 需要解析 PLT 表 */ break;
        }
        dyn_entry++;
    }

    for (size_t i = 0; i < relsz / sizeof(Elf64_Rela); i++) {
        Elf64_Rela *r          = &rel[i];
        uint64_t   *reloc_addr = (uint64_t *)(r->r_offset + offset);
        uint32_t    sym_idx    = ELF64_R_SYM(r->r_info);
        uint32_t    type       = ELF64_R_TYPE(r->r_info);

        if (type == R_X86_64_GLOB_DAT || type == R_X86_64_JUMP_SLOT) {
            *reloc_addr = (uint64_t)resolve_symbol(symtab, sym_idx) + offset;
        } else if (type == R_X86_64_RELATIVE) {
            *reloc_addr = (uint64_t)(offset + r->r_addend);
        } else if (type == R_X86_64_64) {
            *reloc_addr = (uint64_t)resolve_symbol(symtab, sym_idx) + r->r_addend + offset;
        }
    }
    if (!handle_relocations(jmprel, symtab, strtab, jmprel_sz, offset)) {
        write_serial_fmt("Failed to handle relocations.\n");
        return NULL;
    }

    void *entry = find_symbol_address("dlmain", ehdr, offset);
    if (entry == NULL) { entry = find_symbol_address("_dlmain", ehdr, offset); }
    kmod->entry = (dlinit_t)entry;

    void *tentry = find_symbol_address("dlstart", ehdr, offset);
    if (tentry == NULL) { tentry = find_symbol_address("_dlstart", ehdr, offset); }
    kmod->task_entry = (dlinit_t)tentry;

    dlinit_t dlinit_func = (dlinit_t)entry;
    return dlinit_func;
}

bool elf_test_head(Elf64_Ehdr *ehdr) {
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

void load_segment(Elf64_Phdr *phdr, void *elf, page_directory_t *directory, bool is_user,
                  uint64_t offset, uint64_t *load_start) {
    size_t hi = PADDING_UP(phdr->p_paddr + phdr->p_memsz, 0x1000) + offset;
    size_t lo = PADDING_DOWN(phdr->p_paddr, 0x1000) + offset;
    if (load_start != NULL) {
        if (lo < *load_start) { *load_start = lo; }
    }
    uint64_t flags = PTE_PRESENT | PTE_WRITEABLE;
    if (is_user) flags |= PTE_USER;
    if ((phdr->p_flags & PF_R) && !(phdr->p_flags & PF_W)) {
        for (size_t i = lo; i < hi; i += 0x1000) {
            page_map_to(directory, i, alloc_frames(1), flags);
        }
    } else
        for (size_t i = lo; i < hi; i += 0x1000) {
            page_map_to(directory, i, alloc_frames(1), flags);
        }
    uint64_t          p_vaddr  = (uint64_t)phdr->p_vaddr + offset;
    uint64_t          p_filesz = (uint64_t)phdr->p_filesz;
    uint64_t          p_memsz  = (uint64_t)phdr->p_memsz;
    page_directory_t *dir      = get_current_directory();
    switch_process_page_directory(directory);
    memcpy((void *)p_vaddr, (void*)((uint64_t)elf + phdr->p_offset), p_memsz);

    if (p_memsz > p_filesz) { // 这个是bss段
        memset((void *)(p_vaddr + p_filesz), 0, p_memsz - p_filesz);
    }
    switch_process_page_directory(dir);
}

bool mmap_phdr_segment(Elf64_Ehdr *ehdr, Elf64_Phdr *phdrs, page_directory_t *directory,
                       bool is_user, uint64_t offset, uint64_t *load_start, uint64_t *load_size) {
    size_t i = 0;
    while (i < ehdr->e_phnum && phdrs[i].p_type != PT_LOAD) {
        i++;
    }

    if (i == ehdr->e_phnum) { return false; }

    uint64_t load_min = 0xffffffffffffffff;
    uint64_t load_max = 0x0000000000000000;

    for (i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            load_segment(&phdrs[i], (void *)ehdr, directory, is_user, offset, load_start);
            if (phdrs[i].p_vaddr + offset + phdrs[i].p_memsz > load_max)
                load_max = phdrs[i].p_vaddr + offset + phdrs[i].p_memsz;
            if (phdrs[i].p_vaddr + offset < load_min) load_min = phdrs[i].p_vaddr + offset;
        }
    }

    if (load_size) { *load_size = load_max - load_min; }

    return true;
}

bool is_dynamic(Elf64_Ehdr *ehdr) {
    if (ehdr->e_type != ET_DYN) { return false; }
    if (ehdr->e_phnum == 0 || ehdr->e_phoff == 0) { return false; }
    return true;
}

elf_start load_executor_elf(uint8_t *data, page_directory_t *dir, uint64_t offset,
                            uint64_t *load_start, pcb_t process) {
    if (data == NULL) return NULL;
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)data;
    if (!elf_test_head(ehdr)) { return NULL; }
    Elf64_Phdr       *phdrs = (Elf64_Phdr *)((char *)ehdr + ehdr->e_phoff);
    page_directory_t *cur   = get_current_directory();
    switch_process_page_directory(dir);
    uint64_t load_size = 0;
    if (!mmap_phdr_segment(ehdr, phdrs, dir, true, offset, load_start, &load_size)) { return NULL; }
    // VMA
    if (process != NULL) {
        vma_t *ld_so_vma = vma_alloc();

        ld_so_vma->vm_start  = *load_start;
        ld_so_vma->vm_end    = *load_start + load_size;
        ld_so_vma->vm_flags |= VMA_READ | VMA_WRITE | VMA_EXEC;

        ld_so_vma->vm_type = VMA_TYPE_ANON;
        ld_so_vma->vm_name = strdup(process->name);
        vma_insert(&process->vma_manager, ld_so_vma);
    }
    switch_process_page_directory(cur);
    return (elf_start)ehdr->e_entry;
}

#define KERNEL_MOD_SPACE_START 0xffffffffb0000000       // 内核模块加载起始地址
#define KERNEL_MOD_SPACE_END   0xffffffffc0000000       // 内核模块加载结束地址

void dlinker_load(kernel_mode_t *kmod,cp_module_t *module) {
    if (module == NULL) return;

    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)module->data;

    Elf64_Phdr phdr[12];
    if (ehdr->e_phnum > sizeof(phdr) / sizeof(phdr[0]) || ehdr->e_phnum < 1) {
        write_serial_fmt("ELF file has wrong number of program headers");
        return;
    }

    if (ehdr->e_type != ET_DYN) {
        write_serial_fmt("ELF file is not a dynamic library.\n");
        return;
    }

    uint64_t load_size = 0;

    Elf64_Phdr *phdrs = (Elf64_Phdr *)((char *)ehdr + ehdr->e_phoff);
    if (!mmap_phdr_segment(ehdr, phdrs, get_current_directory(), false,
                           KERNEL_MOD_SPACE_START + kernel_modules_load_offset, NULL, &load_size)) {
        write_serial_fmt("Cannot mmap elf segment.\n");
        return;
    }

    dlinit_t dlinit =
        load_dynamic(kmod,phdrs, ehdr, KERNEL_MOD_SPACE_START + kernel_modules_load_offset);
    if (dlinit == NULL) {
        dlinit = (dlinit_t)ehdr->e_entry;
        if (dlinit == NULL) {
            write_serial_fmt("Cannot find dlinit function.\n");
            return;
        }
    }
    write_serial_fmt("kmod: loaded module %s at %#018lx\n", module->module_name, KERNEL_MOD_SPACE_START + kernel_modules_load_offset);
    kmod->entry_exit_code = dlinit();

    kernel_modules_load_offset += (load_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

dlfunc_t *find_func(const char *name) {
    for (size_t i = 0; i < dlfunc_count; i++) {
        dlfunc_t *entry = &__ksymtab_start[i];
        if (strcmp(entry->name, name) == 0) { return entry; }
    }
    return NULL;
}

void find_kernel_symbol() {
    dlfunc_count = __ksymtab_end - __ksymtab_start;
    for (size_t i = 0; i < dlfunc_count; i++) {
        dlfunc_t *entry = &__ksymtab_start[i];
    }
}

void dlinker_init() {
    find_kernel_symbol();
}


cp_module_t module_ls[256];
size_t      module_count = 0;

id_allocator_t *kmod_allocator = NULL;
kernel_mode_t *kmods[MAX_KERNEL_MODULE];

void extract_name(const char *input, char *output, size_t output_size) {
    const char *name = strrchr(input, '/');
    if (!name) { return; }
    name++;
    const char *dot = strchr(name, '.');
    if (dot) {
        size_t len = dot - name;
        if (len >= output_size) { len = output_size - 1; }
        strncpy(output, name, len);
        output[len] = '\0';
    } else {
        strncpy(output, name, output_size - 1);
        output[output_size - 1] = '\0';
    }
}

cp_module_t *get_module(const char *module_name) {
    if(module_name == NULL) return NULL;
    for (size_t i = 0; i < module_count; i++) {
        if (module_ls[i].is_use && strcmp(module_ls[i].module_name, module_name) == 0) {
            return &module_ls[i];
        }
    }
    return NULL;
}

cp_module_t *get_module_raw(const char *module_name) {
    if(module_name == NULL) return NULL;
    for (size_t i = 0; i < module_count; i++) {
        if (module_ls[i].is_use && strcmp(module_ls[i].raw_name, module_name) == 0) {
            return &module_ls[i];
        }
    }
    return NULL;
}

void module_setup() {
    vfs_node_t mr = vfs_open("/lib/modules");
    list_foreach(mr->child,i) {
        vfs_node_t ch = (vfs_node_t)i->data;
        char* buf = strdup(strcat("/lib/modules",ch->name));
        extract_name(buf, module_ls[module_count].module_name, sizeof(char) * 20);
        module_ls[module_count].path = buf;
        uint8_t *data=(uint8_t*)malloc(ch->size+128);
        vfs_read(ch,data,0,ch->size);
        module_ls[module_count].data = data;
        module_ls[module_count].size = ch->size;
        strcpy(module_ls[module_count].raw_name, buf);
        module_ls[module_count].is_use = true;
        write_serial_string("kmod: Module");
        write_serial_string(module_ls[module_count].module_name);
        write_serial_string(" ");
        write_serial_string(buf);
        write_serial_string("\n");
        module_count++;
    }
}

static bool ends_with_km(const char *str) {
    size_t len = strlen(str);
    if (len < 3) return false;
    return strcmp(str + len - 3, ".km") == 0;
}

void start_all_kernel_module() {
    for (size_t i = 0; i < MAX_KERNEL_MODULE; i++) {
        kernel_mode_t *kmod = kmods[i];
        if(kmod == NULL) continue;
        if(kmod->task_entry == NULL) continue;
        if(kmod->entry_exit_code & 0x7FFFFFFFFFFFF000) {
            write_serial_fmt("kmod: cannot start mod(%s) - exit_code: %d\n",kmod->module->module_name,
                  kmod->entry_exit_code);
        }else{
            int ret = kmod->task_entry();
        }
    }
}

void load_all_kernel_module() {
    kmod_allocator = id_allocator_create(MAX_KERNEL_MODULE);
    for (size_t i = 0; i < module_count; i++) {
        if (module_ls[i].is_use) {
            if (ends_with_km(module_ls[i].raw_name)) {
                cp_module_t *mod = get_module(module_ls[i].module_name);
                if (mod) {
                    int id = id_alloc(kmod_allocator);
                    kmods[id] = (kernel_mode_t*)calloc(1,sizeof(kernel_mode_t));
                    dlinker_load(kmods[id],mod);
                    kmods[id]->module = mod;
                }
            }
        }
    }
}
