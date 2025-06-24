#pragma once
#include "../../../include.h"

#define CMOS_ADDR 0x70 // CMOS 地址寄存器
#define CMOS_DATA 0x71 // CMOS 数据寄存器

// 下面是 CMOS 信息的寄存器索引
#define CMOS_SECOND  0x00 // (0 ~ 59)
#define CMOS_MINUTE  0x02 // (0 ~ 59)
#define CMOS_HOUR    0x04 // (0 ~ 23)
#define CMOS_WEEKDAY 0x06 // (1 ~ 7) 星期天 = 1，星期六 = 7
#define CMOS_DAY     0x07 // (1 ~ 31)
#define CMOS_MONTH   0x08 // (1 ~ 12)
#define CMOS_YEAR    0x09 // (0 ~ 99)
#define CMOS_CENTURY 0x32 // 可能不存在
#define CMOS_NMI     0x80
// We are Chinese ;)
#define UTC          8

struct time_t {
    int tm_sec;   // 秒数 [0，59]
    int tm_min;   // 分钟数 [0，59]
    int tm_hour;  // 小时数 [0，59]
    int tm_mday;  // 1 个月的天数 [0，31]
    int tm_mon;   // 1 年中月份 [0，11]
    int tm_year;  // 从 1900 年开始的年数
    int tm_wday;  // 1 星期中的某天 [0，6] (星期天 =0)
    int tm_yday;  // 1 年中的某天 [0，365]
    int tm_isdst; // 夏令时标志
};

void time_read_bcd(struct time_t *time);
void time_read(struct time_t *time);
