#ifndef _ELF_H_
#define _ELF_H_

#define R_X86_64_GLOB_DAT  6 /* Create GOT entry */
#define R_X86_64_JUMP_SLOT 7 /* Create PLT entry */
#define R_X86_64_RELATIVE  8 /* Adjust by program base */

#define DT_NULL            0           /* Marks end of dynamic section */
#define DT_NEEDED          1           /* Name of needed library */
#define DT_PLTRELSZ        2           /* Size in bytes of PLT relocs */
#define DT_PLTGOT          3           /* Processor defined value */
#define DT_HASH            4           /* Address of symbol hash table */
#define DT_STRTAB          5           /* Address of string table */
#define DT_SYMTAB          6           /* Address of symbol table */
#define DT_RELA            7           /* Address of Rela relocs */
#define DT_RELASZ          8           /* Total size of Rela relocs */
#define DT_RELAENT         9           /* Size of one Rela reloc */
#define DT_STRSZ           10          /* Size of string table */
#define DT_SYMENT          11          /* Size of one symbol table entry */
#define DT_INIT            12          /* Address of init function */
#define DT_FINI            13          /* Address of termination function */
#define DT_SONAME          14          /* Name of shared object */
#define DT_RPATH           15          /* Library search path (deprecated) */
#define DT_SYMBOLIC        16          /* Start symbol search here */
#define DT_REL             17          /* Address of Rel relocs */
#define DT_RELSZ           18          /* Total size of Rel relocs */
#define DT_RELENT          19          /* Size of one Rel reloc */
#define DT_PLTREL          20          /* Type of reloc in PLT */
#define DT_DEBUG           21          /* For debugging; unspecified */
#define DT_TEXTREL         22          /* Reloc might modify .text */
#define DT_JMPREL          23          /* Address of PLT relocs */
#define DT_BIND_NOW        24          /* Process relocations of object */
#define DT_INIT_ARRAY      25          /* Array with addresses of init fct */
#define DT_FINI_ARRAY      26          /* Array with addresses of fini fct */
#define DT_INIT_ARRAYSZ    27          /* Size in bytes of DT_INIT_ARRAY */
#define DT_FINI_ARRAYSZ    28          /* Size in bytes of DT_FINI_ARRAY */
#define DT_RUNPATH         29          /* Library search path */
#define DT_FLAGS           30          /* Flags for the object being loaded */
#define DT_ENCODING        32          /* Start of encoded range */
#define DT_PREINIT_ARRAY   32          /* Array with addresses of preinit fct*/
#define DT_PREINIT_ARRAYSZ 33          /* size in bytes of DT_PREINIT_ARRAY */
#define DT_NUM             34          /* Number used */
#define DT_LOOS            0x6000000d  /* Start of OS-specific */
#define DT_HIOS            0x6ffff000  /* End of OS-specific */
#define DT_LOPROC          0x70000000  /* Start of processor-specific */
#define DT_HIPROC          0x7fffffff  /* End of processor-specific */
#define DT_PROCNUM         DT_MIPS_NUM /* Most used by any processor */

/* Legal values for e_type (object file type).  */

#define ET_NONE   0      /* No file type */
#define ET_REL    1      /* Relocatable file */
#define ET_EXEC   2      /* Executable file */
#define ET_DYN    3      /* Shared object file */
#define ET_CORE   4      /* Core file */
#define ET_NUM    5      /* Number of defined types */
#define ET_LOOS   0xfe00 /* OS-specific range start */
#define ET_HIOS   0xfeff /* OS-specific range end */
#define ET_LOPROC 0xff00 /* Processor-specific range start */
#define ET_HIPROC 0xffff /* Processor-specific range end */

/* Legal values for e_version (version).  */

#define EV_NONE    0 /* Invalid ELF version */
#define EV_CURRENT 1 /* Current version */
#define EV_NUM     2

/* Fields in the e_ident array.  The EI_* macros are indices into the
   array.  The macros under each EI_* macro are the values the byte
   may have.  */

#define EI_MAG0 0    /* File identification byte 0 index */
#define ELFMAG0 0x7f /* Magic number byte 0 */

#define EI_MAG1 1   /* File identification byte 1 index */
#define ELFMAG1 'E' /* Magic number byte 1 */

#define EI_MAG2 2   /* File identification byte 2 index */
#define ELFMAG2 'L' /* Magic number byte 2 */

#define EI_MAG3 3   /* File identification byte 3 index */
#define ELFMAG3 'F' /* Magic number byte 3 */

/* Legal values for e_machine (architecture).  */

#define EM_NONE        0  /* No machine */
#define EM_M32         1  /* AT&T WE 32100 */
#define EM_SPARC       2  /* SUN SPARC */
#define EM_386         3  /* Intel 80386 */
#define EM_68K         4  /* Motorola m68k family */
#define EM_88K         5  /* Motorola m88k family */
#define EM_860         7  /* Intel 80860 */
#define EM_MIPS        8  /* MIPS R3000 big-endian */
#define EM_S370        9  /* IBM System/370 */
#define EM_MIPS_RS3_LE 10 /* MIPS R3000 little-endian */

#define EM_PARISC      15 /* HPPA */
#define EM_VPP500      17 /* Fujitsu VPP500 */
#define EM_SPARC32PLUS 18 /* Sun's "v8plus" */
#define EM_960         19 /* Intel 80960 */
#define EM_PPC         20 /* PowerPC */
#define EM_PPC64       21 /* PowerPC 64-bit */
#define EM_S390        22 /* IBM S390 */

#define EM_V800       36 /* NEC V800 series */
#define EM_FR20       37 /* Fujitsu FR20 */
#define EM_RH32       38 /* TRW RH-32 */
#define EM_RCE        39 /* Motorola RCE */
#define EM_ARM        40 /* ARM */
#define EM_FAKE_ALPHA 41 /* Digital Alpha */
#define EM_SH         42 /* Hitachi SH */
#define EM_SPARCV9    43 /* SPARC v9 64-bit */
#define EM_TRICORE    44 /* Siemens Tricore */
#define EM_ARC        45 /* Argonaut RISC Core */
#define EM_H8_300     46 /* Hitachi H8/300 */
#define EM_H8_300H    47 /* Hitachi H8/300H */
#define EM_H8S        48 /* Hitachi H8S */
#define EM_H8_500     49 /* Hitachi H8/500 */
#define EM_IA_64      50 /* Intel Merced */
#define EM_MIPS_X     51 /* Stanford MIPS-X */
#define EM_COLDFIRE   52 /* Motorola Coldfire */
#define EM_68HC12     53 /* Motorola M68HC12 */
#define EM_MMA        54 /* Fujitsu MMA Multimedia Accelerator*/
#define EM_PCP        55 /* Siemens PCP */
#define EM_NCPU       56 /* Sony nCPU embeeded RISC */
#define EM_NDR1       57 /* Denso NDR1 microprocessor */
#define EM_STARCORE   58 /* Motorola Start*Core processor */
#define EM_ME16       59 /* Toyota ME16 processor */
#define EM_ST100      60 /* STMicroelectronic ST100 processor */
#define EM_TINYJ      61 /* Advanced Logic Corp. Tinyj emb.fam*/
#define EM_X86_64     62 /* AMD x86-64 architecture */
#define EM_PDSP       63 /* Sony DSP Processor */

#define EM_FX66     66 /* Siemens FX66 microcontroller */
#define EM_ST9PLUS  67 /* STMicroelectronics ST9+ 8/16 mc */
#define EM_ST7      68 /* STmicroelectronics ST7 8 bit mc */
#define EM_68HC16   69 /* Motorola MC68HC16 microcontroller */
#define EM_68HC11   70 /* Motorola MC68HC11 microcontroller */
#define EM_68HC08   71 /* Motorola MC68HC08 microcontroller */
#define EM_68HC05   72 /* Motorola MC68HC05 microcontroller */
#define EM_SVX      73 /* Silicon Graphics SVx */
#define EM_ST19     74 /* STMicroelectronics ST19 8 bit mc */
#define EM_VAX      75 /* Digital VAX */
#define EM_CRIS     76 /* Axis Communications 32-bit embedded processor */
#define EM_JAVELIN  77 /* Infineon Technologies 32-bit embedded processor */
#define EM_FIREPATH 78 /* Element 14 64-bit DSP Processor */
#define EM_ZSP      79 /* LSI Logic 16-bit DSP Processor */
#define EM_MMIX     80 /* Donald Knuth's educational 64-bit processor */
#define EM_HUANY    81 /* Harvard University machine-independent object files */
#define EM_PRISM    82 /* SiTera Prism */
#define EM_AVR      83 /* Atmel AVR 8-bit microcontroller */
#define EM_FR30     84 /* Fujitsu FR30 */
#define EM_D10V     85 /* Mitsubishi D10V */
#define EM_D30V     86 /* Mitsubishi D30V */
#define EM_V850     87 /* NEC v850 */
#define EM_M32R     88 /* Mitsubishi M32R */
#define EM_MN10300  89 /* Matsushita MN10300 */
#define EM_MN10200  90 /* Matsushita MN10200 */
#define EM_PJ       91 /* picoJava */
#define EM_OPENRISC 92 /* OpenRISC 32-bit embedded processor */
#define EM_ARC_A5   93 /* ARC Cores Tangent-A5 */
#define EM_XTENSA   94 /* Tensilica Xtensa Architecture */
#define EM_NUM      95

/* Legal values for p_flags (segment flags).  */

#define PF_X        (1 << 0)   /* Segment is executable */
#define PF_W        (1 << 1)   /* Segment is writable */
#define PF_R        (1 << 2)   /* Segment is readable */
#define PF_MASKOS   0x0ff00000 /* OS-specific */
#define PF_MASKPROC 0xf0000000 /* Processor-specific */

/* How to extract and insert information held in the r_info field.  */

#define ELF32_R_SYM(val)        ((val) >> 8)
#define ELF32_R_TYPE(val)       ((val) & 0xff)
#define ELF32_R_INFO(sym, type) (((sym) << 8) + ((type) & 0xff))

#define ELF64_R_SYM(i)          ((i) >> 32)
#define ELF64_R_TYPE(i)         ((i) & 0xffffffff)
#define ELF64_R_INFO(sym, type) ((((Elf64_Xword)(sym)) << 32) + (type))

/* Legal values for p_type (segment type).  */

#define PT_NULL         0          /* Program header table entry unused */
#define PT_LOAD         1          /* Loadable program segment */
#define PT_DYNAMIC      2          /* Dynamic linking information */
#define PT_INTERP       3          /* Program interpreter */
#define PT_NOTE         4          /* Auxiliary information */
#define PT_SHLIB        5          /* Reserved */
#define PT_PHDR         6          /* Entry for header table itself */
#define PT_TLS          7          /* Thread-local storage segment */
#define PT_NUM          8          /* Number of defined types */
#define PT_LOOS         0x60000000 /* Start of OS-specific */
#define PT_GNU_EH_FRAME 0x6474e550 /* GCC .eh_frame_hdr segment */
#define PT_GNU_STACK    0x6474e551 /* Indicates stack executability */
#define PT_GNU_RELRO    0x6474e552 /* Read-only after relocation */
#define PT_LOSUNW       0x6ffffffa
#define PT_SUNWBSS      0x6ffffffa /* Sun Specific segment */
#define PT_SUNWSTACK    0x6ffffffb /* Stack segment */
#define PT_HISUNW       0x6fffffff
#define PT_HIOS         0x6fffffff /* End of OS-specific */
#define PT_LOPROC       0x70000000 /* Start of processor-specific */
#define PT_HIPROC       0x7fffffff /* End of processor-specific */

/* Special section indices.  */

#define SHN_UNDEF     0      /* Undefined section */
#define SHN_LORESERVE 0xff00 /* Start of reserved indices */
#define SHN_LOPROC    0xff00 /* Start of processor-specific */
#define SHN_BEFORE                                                                                 \
  0xff00 /* Order section before all others
					   (Solaris).  */
#define SHN_AFTER                                                                                  \
  0xff01                     /* Order section after all others
					   (Solaris).  */
#define SHN_HIPROC    0xff1f /* End of processor-specific */
#define SHN_LOOS      0xff20 /* Start of OS-specific */
#define SHN_HIOS      0xff3f /* End of OS-specific */
#define SHN_ABS       0xfff1 /* Associated symbol is absolute */
#define SHN_COMMON    0xfff2 /* Associated symbol is common */
#define SHN_XINDEX    0xffff /* Index is in extra table.  */
#define SHN_HIRESERVE 0xffff /* End of reserved indices */

#include "../efi.h"

#define EI_NIDENT (16)

typedef struct {
  UINT32 p_type;   /* Segment type */
  UINT32 p_flags;  /* Segment flags */
  UINT64 p_offset; /* Segment file offset */
  UINT64 p_vaddr;  /* Segment virtual address */
  UINT64 p_paddr;  /* Segment physical address */
  UINT64 p_filesz; /* Segment size in file */
  UINT64 p_memsz;  /* Segment size in memory */
  UINT64 p_align;  /* Segment alignment */
} Elf64_Phdr;

typedef struct {
  UINT8  e_ident[16]; /* Magic number and other info */
  UINT16 e_type;      /* Object file type */
  UINT16 e_machine;   /* Architecture */
  UINT32 e_version;   /* Object file version */
  UINT64 e_entry;     /* Entry point virtual address */
  UINT64 e_phoff;     /* Program header table file offset */
  UINT64 e_shoff;     /* Section header table file offset */
  UINT32 e_flags;     /* Processor-specific flags */
  UINT16 e_ehsize;    /* ELF header size in bytes */
  UINT16 e_phentsize; /* Program header table entry size */
  UINT16 e_phnum;     /* Program header table entry count */
  UINT16 e_shentsize; /* Section header table entry size */
  UINT16 e_shnum;     /* Section header table entry count */
  UINT16 e_shstrndx;  /* Section header string table index */
} Elf64_Ehdr;

typedef struct {
  UINT64 r_offset; /* Address */
  UINT64 r_info;   /* Relocation type and symbol index */
  INT64  r_addend; /* Addend */
} Elf64_Rela;

typedef struct {
  INT64 d_tag; /* Dynamic entry type */
  union {
    UINT64 d_val; /* Integer value */
    UINT64 d_ptr; /* Address value */
  } d_un;
} Elf64_Dyn;

typedef struct {
  UINT32        st_name;  /* Symbol name (string tbl index) */
  unsigned char st_info;  /* Symbol type and binding */
  unsigned char st_other; /* Symbol visibility */
  UINT16        st_shndx; /* Section index */
  UINT64        st_value; /* Symbol value */
  UINT64        st_size;  /* Symbol size */
} Elf64_Sym;

static void CalcLoadAddressRange(Elf64_Ehdr *ehdr, UINT64 *first, UINT64 *last) {
  Elf64_Phdr *phdr = (Elf64_Phdr *)((UINT64)ehdr + ehdr->e_phoff); // 第一个 program header 地址
  *first           = 0xffffffffffffffff;                           // UINT64最大值
  *last            = 0;                                            // UINT64最小值

  for (UINT16 i = 0; i < ehdr->e_phnum; i++) { // 遍历每一个 program header
    if (phdr[i].p_type != PT_LOAD) continue;   // 只关心LOAD段
    *first = min(*first, phdr[i].p_vaddr);
    *last  = max(*last, phdr[i].p_vaddr + phdr[i].p_memsz); // 每一个program header首尾取最值
  }
}

static void CopyLoadSegments(Elf64_Ehdr *ehdr) {
  Elf64_Phdr *phdr = (Elf64_Phdr *)((UINT64)ehdr + ehdr->e_phoff); // 第一个 program header 地址
  for (UINT16 i = 0; i < ehdr->e_phnum; i++) {                     // 遍历每一个 program header
    if (phdr[i].p_type != PT_LOAD) continue;                       // 只关心LOAD段

    UINT64 segm_in_file = (UINT64)ehdr + phdr[i].p_offset; // 段在文件中的位置
    xmemcpy((VOID *)phdr[i].p_vaddr, (VOID *)segm_in_file,
            phdr[i].p_filesz);                               // 将文件中大小的部分copy过去
    UINTN remain_bytes = phdr[i].p_memsz - phdr[i].p_filesz; // 两者之差
    xmemset((VOID *)(phdr[i].p_vaddr + phdr[i].p_filesz), 0, remain_bytes); // 赋值为0
  }
}
#endif