#pragma once

#define NEED_UTC_8

#define CMOS_INDEX 0x70
#define CMOS_DATA  0x71

#define CMOS_CUR_SEC  0x0
#define CMOS_CUR_MIN  0x2
#define CMOS_CUR_HOUR 0x4
#define CMOS_WEEK_DAY 0x6
#define CMOS_MON_DAY  0x7
#define CMOS_CUR_MON  0x8
#define CMOS_CUR_YEAR 0x9
#define CMOS_CUR_CEN  0x32

#define MINUTE 60
#define HOUR   (60 * MINUTE)
#define DAY    (24 * HOUR)
#define YEAR   (365 * DAY)

#include "proto.h"

typedef struct timer {
    uint64_t old_time;
} timer_t;

typedef struct
{
    int tm_sec;   // 秒数 [0，59]
    int tm_min;   // 分钟数 [0，59]
    int tm_hour;  // 小时数 [0，59]
    int tm_mday;  // 1 个月的天数 [0，31]
    int tm_mon;   // 1 年中月份 [0，11]
    int tm_year;  // 从 1900 年开始的年数
    int tm_wday;  // 1 星期中的某天 [0，6] (星期天 =0)
    int tm_yday;  // 1 年中的某天 [0，365]
    int tm_isdst; // 夏令时标志
} tm;

//void timer_handle(void); __attribute__((naked));
uint64_t elapsed();

uint32_t get_hour();
uint32_t get_min();
uint32_t get_sec();
uint32_t get_day_of_month();
uint32_t get_day_of_week();
uint32_t get_mon();
uint32_t get_year();
int      is_leap_year(int year);
char    *get_date_time();
int64_t  mktime();
timer_t *alloc_timer();
uint64_t get_time(timer_t *timer1);
void time_read(tm *time);