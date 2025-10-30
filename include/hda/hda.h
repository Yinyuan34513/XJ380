#ifndef HDA_H
#define HDA_H

#include <stdint.h>

typedef struct {
    void*    regs_base;           // 内存映射寄存器基地址
    uint32_t* corb_buffer;        // CORB缓冲区
    uint16_t corb_entries;        // CORB条目数
    uint8_t  num_codecs;          // 编解码器数量
    // 就先这么多DMA
} hda_controller_t;

// 函数声明
int hda_init();

#endif