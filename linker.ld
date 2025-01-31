OUTPUT_FORMAT("elf64-x86-64")  
ENTRY(KernelMain)  
  
SECTIONS  
{  
    . = 1M;         /* 假设程序从1MB的内存地址开始 */  
    . = ALIGN(4K);  /* 确保起始地址是4KB对齐的 */  
  
    .text :  
    {  
        *(.text)  
        _etext = .; /* 定义text段的结束位置 */  
    }  
  
    .rodata ALIGN(4K) : { *(.rodata) } /* 只读数据段，4KB对齐 */  
  
    /* 注意：在UEFI或许多现代操作系统中，.data和.bss段的位置不是由链接器脚本直接控制的 */  
    .data :  
    {  
        *(.data)  
        _edata = .; /* 定义data段的结束位置 */  
    }  

    . = ALIGN(65536);
    .data.init_task :
    {
        *(.data.init_task)
    }
  
    .bss :  
    {  
        __bss_start__ = .;  
        *(.bss)  
        *(COMMON)  
        __bss_end__ = .;  
    }  
}