/* 这是一个简化的、非UEFI的ld脚本示例 */  
  
OUTPUT_FORMAT("elf64-x86-64")  
ENTRY(_start)  
  
SECTIONS  
{  
    . = 1M; /* 假设我们的程序从1MB的内存地址开始 */  
  
    .text :  
    {  
        *(.text) /* 链接所有.text段 */  
        _etext = .; /* 定义text段的结束位置 */  
    }  
  
    .rodata : { *(.rodata) } /* 只读数据段 */  
  
    .data : AT (ADDR(.text) + SIZEOF(.text))  
    {  
        *(.data) /* 链接所有.data段，它们会被放置在紧随.text段之后的位置 */  
        _edata = .; /* 定义data段的结束位置 */  
    }  
  
    .bss :  
    {  
        __bss_start__ = .; /* bss段的开始位置 */  
        *(.bss) /* 未初始化的数据段 */  
        *(COMMON)  
        __bss_end__ = .; /* bss段的结束位置 */  
    }  
  
    /* 其他段和指令可以根据需要进行添加 */  
  
    /* 如果需要，可以在这里添加堆和栈的定义，但请注意，UEFI通常不需要静态的堆和栈 */  
    /* 堆和栈通常是由UEFI环境在运行时提供的 */  
}  
  
/* 这里可以添加更多的链接器指令，如符号解析规则、版本控制等 */