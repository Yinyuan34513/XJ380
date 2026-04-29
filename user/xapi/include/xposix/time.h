#pragma once

#include "sys/types.h"

#define CLOCK_REALTIME           0
#define CLOCK_MONOTONIC          1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID  3
#define CLOCK_MONOTONIC_RAW      4
#define CLOCK_REALTIME_COARSE    5
#define CLOCK_MONOTONIC_COARSE   6
#define CLOCK_BOOTTIME           7

struct timespec {
    time_t tv_sec;
    long   tv_nsec;
};

#ifdef __cplusplus
extern "C" {
#endif

int clock_gettime(int clockid, struct timespec *ts);
int clock_getres(int clockid, struct timespec *ts);
int nanosleep(const struct timespec *req, struct timespec *rem);

#ifdef __cplusplus
}
#endif
