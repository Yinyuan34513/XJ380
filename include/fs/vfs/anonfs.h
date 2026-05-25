#pragma once

#include <cpu/lock.h>
#include <fs/vfs/vfs.h>
#include <syscall/signal.h>

#ifdef __cplusplus
extern "C" {
#endif

struct timerfd_timespec {
    long long tv_sec;
    long      tv_nsec;
};

struct itimerspec {
    struct timerfd_timespec it_interval;
    struct timerfd_timespec it_value;
};

#define EFD_SEMAPHORE 0x1
#define EFD_NONBLOCK  O_NONBLOCK
#define EFD_CLOEXEC   O_CLOEXEC

#define SFD_NONBLOCK O_NONBLOCK
#define SFD_CLOEXEC  O_CLOEXEC

#define TFD_TIMER_ABSTIME      0x1
#define TFD_TIMER_CANCEL_ON_SET 0x2
#define TFD_NONBLOCK           O_NONBLOCK
#define TFD_CLOEXEC            O_CLOEXEC

#define MFD_CLOEXEC       0x0001U
#define MFD_ALLOW_SEALING 0x0002U

struct signalfd_siginfo {
    uint32_t ssi_signo;
    int32_t  ssi_errno;
    int32_t  ssi_code;
    uint32_t ssi_pid;
    uint32_t ssi_uid;
    int32_t  ssi_fd;
    uint32_t ssi_tid;
    uint32_t ssi_band;
    uint32_t ssi_overrun;
    uint32_t ssi_trapno;
    int32_t  ssi_status;
    int32_t  ssi_int;
    uint64_t ssi_ptr;
    uint64_t ssi_utime;
    uint64_t ssi_stime;
    uint64_t ssi_addr;
    uint16_t ssi_addr_lsb;
    uint16_t __pad2;
    int32_t  ssi_syscall;
    uint64_t ssi_call_addr;
    uint32_t ssi_arch;
    uint8_t  __pad[28];
};

typedef enum anonfs_kind {
    ANONFS_EVENTFD = 1,
    ANONFS_SIGNALFD,
    ANONFS_TIMERFD,
    ANONFS_MEMFD,
} anonfs_kind_t;

typedef struct anonfs_eventfd {
    spin_t   lock;
    uint64_t counter;
    uint32_t flags;
} anonfs_eventfd_t;

typedef struct anonfs_signalfd {
    spin_t lock;
    sigset_t mask;
    uint32_t flags;
    size_t head;
    size_t tail;
    size_t count;
    struct signalfd_siginfo queue[64];
} anonfs_signalfd_t;

typedef struct anonfs_timerfd {
    spin_t   lock;
    uint32_t flags;
    int      clockid;
    bool     armed;
    uint64_t interval_ns;
    uint64_t next_expire_ns;
    uint64_t pending;
} anonfs_timerfd_t;

typedef struct anonfs_memfd {
    spin_t   lock;
    char    *name;
    uint64_t phys;
    uint8_t *data;
    size_t   size;
    size_t   capacity;
    uint32_t flags;
} anonfs_memfd_t;

typedef struct anonfs_handle {
    anonfs_kind_t kind;
    union {
        anonfs_eventfd_t eventfd;
        anonfs_signalfd_t signalfd;
        anonfs_timerfd_t timerfd;
        anonfs_memfd_t memfd;
    } u;
} anonfs_handle_t;

void anonfs_init(void);
int  anonfs_create_eventfd(uint64_t initial_value, uint32_t flags);
int  anonfs_create_signalfd(int ufd, const sigset_t *mask, size_t sizemask, uint32_t flags);
int  anonfs_create_timerfd(int clockid, uint32_t flags);
int  anonfs_timerfd_settime(int fd, int flags, const struct itimerspec *new_value, struct itimerspec *old_value);
int  anonfs_create_memfd(const char *name, uint32_t flags);
void anonfs_notify_signal(pcb_t process, int sig, const siginfo_t *info);

#ifdef __cplusplus
}
#endif
