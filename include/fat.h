#ifndef _XJ380_FAT_H_
#define _XJ380_FAT_H_

#define ADR_DISKIMG		0x00100000

typedef struct s_fat 
{
	//类型 变量名      作用    偏移量
    u8 name[8];	//文件名    0
    u8 ext[3];	//扩展名    8
    u8 att;			//属性      11
    u16 time;		//创建时间  22
    u16 day;		//创建日期  24
    u16 clu_num;	//首簇号    26
    u32 size;		//大小      28
} FAT_TABLE;

#endif