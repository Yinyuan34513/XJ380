#include "include.h"
//shit山，无任何优化的内存管理头文件
#define FREE_MEMORYS    240000  //可用内存（单位KB）约合256MB（算上保留空间共22144KB）（256KB为管理表所用）

PUBLIC struct MEM_LAB_INFO
{
    u32 addr, size;
};

PUBLIC struct MEM_INFO
{
    u32 max_frees;  //可用最大空间
    u32 blocks;     //一共多少块碎片
    struct MEM_LAB_INFO free[FREE_MEMORYS];   //未使用空间登记表
}memory;

