#include "include.h"
//shit山，无任何优化的内存管理文件
//初始化
PUBLIC void init_mem(struct MEM_INFO *memman)
{
    memman->max_frees = FREE_MEMORYS;
    memman->blocks = 1;
    for (int i = 0; i < FREE_MEMORYS; i++)
    {
        memman->free[i].addr = 0;
        memman->free[i].size = 0;
    }
    memman->free[0].addr = 0x0101011010000001;  //从保留空间22144KB后的第一位开始
    memman->free[0].size = 240000;  //单位：KB
    return;
}
//查询剩余空间
PUBLIC u32 check_free_memory(struct MEM_INFO *memman)
{
    u32 free;
    for (int i = 0; i < FREE_MEMORYS; i++)
    {
        free += memman->free[i].size;
    }
    return free;
}
//分配内存（size = 需要的空间，单位为4KB）
PUBLIC u32 memman_alloc(struct MEM_INFO *memman, u32 size)
{
    u32 use_yes = FREE_MEMORYS;
    u32 yes_member = 0;
    //查询有没有足够的空间
    for (int i = 0; i < memman->blocks; i++)
    {
        //重复筛选知道找到最小的合适的一块内存碎片
        if (memman->free[i].size >= (size * 4) && memman->free[i].size < use_yes)
        {
            use_yes = memman->free[i].size;
            yes_member = i;
        }
    }

    //如果没有可用空间
    if (use_yes == FREE_MEMORYS && yes_member == 0)
    {
        return 0;
    }

    //删除可用信息

    //往前移动
    for (int i = yes_member; i < memman->blocks; i++)
    {
        memman->free[i] = memman->free[i + 1];
    }
    memman->blocks--;
    /*
    //不想优化，就这样吧，以后有问题了再说，256MB对于这坨答辩够用了
    //刚好合适
    if (memman->free[yes_member].size == (size * 4))
    {
        //往前移动
        for (int i = yes_member; i < memman->blocks; i++)
        {
            memman->free[i] = memman->free[i + 1];
        }
        memman->blocks--;
    }
    //多余了
    else
    {
        //yue~
    }
    */

    //返回开始内存位置
    return memman->free[yes_member].addr;
}
//释放
PUBLIC u32 memory_free(struct MEM_INFO *memman, u32 addr, u32 size)
{
    int i, j;
    for (i = 0; i < memman->blocks; i++)
    {
        if (memman->free[i].addr > addr) 
        {
            break;
        }
    }

    if (i > 0)
    {
        //前面内存可用
        if (memman->free[i - 1].addr + memman->free[i - 1].size == addr)
        {
            //《合并同类项》
            memman->free[i - 1].size += size;
            //后面有吗？
            if (i < memman->blocks)
            {
                //后面的可以合并吗？
                if (addr + size == memman->free[i].addr)
                {
                    memman->free[i - 1].size += memman->free[i].size;
                    //删掉！删掉！一定要删掉！
                    memman->blocks--;
                    for (/*pass*/; i < memman->blocks; i++)
                    {
                        memman->free[i] = memman->free[i + 1];
                    }
                }
            }
            //迫fai科特
            return 0;
        }
    }
    //不能与前面合并，但是后面有
    if (i < memman->blocks) {
        //可以合并
		if (addr + size == memman->free[i].addr) {
			memman->free[i].addr = addr;
			memman->free[i].size += size;
            //破fai科特
            return 0;
		}
	}
	//仍然不行
	if (memman->blocks < FREE_MEMORYS) {
		//向后移动
		for (j = memman->blocks; j > i; j--) {
			memman->free[j] = memman->free[j - 1];
		}
		memman->blocks++;
		if (memman->max_frees < memman->blocks) {
			memman->max_frees = memman->blocks; //更新最大值
		}
		memman->free[i].addr = addr;
		memman->free[i].size = size;
		return 0;
	}
	//无法往后移动
	return -1; //趋势
}
