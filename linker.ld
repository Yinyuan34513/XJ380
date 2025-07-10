OUTPUT_FORMAT("elf64-x86-64")  
ENTRY(KernelMain)  
  
SECTIONS  
{  
    . = 0xFFFFFFFF80000000;         /* 程序从0xFFFFFFFF80000000的内存地址开始 */  
    . = ALIGN(4K);  /* 确保起始地址是4KB对齐的 */  
  
    _kernel_start = .;

    .text :  
    {  
        _text = .; /* 定义text段的开始位置 */
        *(.text)  
        _etext = .; /* 定义text段的结束位置 */  
    }  
  
    . = ALIGN(4K);  /* 确保起始地址是4KB对齐的 */  
    .rodata : { *(.rodata) } /* 只读数据段，4KB对齐 */
  
    /* 注意：在UEFI或许多现代操作系统中，.data和.bss段的位置不是由链接器脚本直接控制的 */  
    . = ALIGN(4K);  /* 确保起始地址是4KB对齐的 */  
    .data :  
    {  
        *(.data)  
        _edata = .; /* 定义data段的结束位置 */  
    }  

    . = ALIGN(32K);
    .data.init_task :
    {
        *(.data.init_task)
    }

    . = ALIGN(4K);  /* 确保起始地址是4KB对齐的 */  
    .bss :  
    {  
        __bss_start__ = .;  
        *(.bss)  
        *(COMMON)  
        __bss_end__ = .;  
    }  
    /* 定义 _end 符号（位于所有段之后） */
    . = ALIGN(4K);                  /* 4KB 对齐 */
    _end = .;                       /* 内核映像结束地址 */ 
}