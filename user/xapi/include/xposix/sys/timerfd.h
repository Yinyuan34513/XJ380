#pragma once

#include "../fcntl.h"
#include "../stdint.h"
#include "../time.h"

#define TFD_TIMER_ABSTIME       0x1
#define TFD_TIMER_CANCEL_ON_SET 0x2
#define TFD_CLOEXEC             O_CLOEXEC
#define TFD_NONBLOCK            O_NONBLOCK

struct timerfd_timespec {
    long long tv_sec;
    long      tv_nsec;
};

struct itimerspec {
    struct timerfd_timespec it_interval;
    struct timerfd_timespec it_value;
};

#ifdef __cplusplus
extern "C" {
#endif

int timerfd_create(int clockid, int flags);
int timerfd_settime(int fd, int flags, const struct itimerspec *new_value, struct itimerspec *old_value);

#ifdef __cplusplus
}
#endif
