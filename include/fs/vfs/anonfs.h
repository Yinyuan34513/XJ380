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

#define IN_NONBLOCK O_NONBLOCK
#define IN_CLOEXEC  O_CLOEXEC

#define IN_ACCESS        0x00000001U
#define IN_MODIFY        0x00000002U
#define IN_ATTRIB        0x00000004U
#define IN_CLOSE_WRITE   0x00000008U
#define IN_CLOSE_NOWRITE 0x00000010U
#define IN_OPEN          0x00000020U
#define IN_MOVED_FROM    0x00000040U
#define IN_MOVED_TO      0x00000080U
#define IN_CREATE        0x00000100U
#define IN_DELETE        0x00000200U
#define IN_DELETE_SELF   0x00000400U
#define IN_MOVE_SELF     0x00000800U
#define IN_UNMOUNT       0x00002000U
#define IN_Q_OVERFLOW    0x00004000U
#define IN_IGNORED       0x00008000U
#define IN_ONLYDIR       0x01000000U
#define IN_DONT_FOLLOW   0x02000000U
#define IN_EXCL_UNLINK   0x04000000U
#define IN_MASK_CREATE   0x10000000U
#define IN_MASK_ADD      0x20000000U
#define IN_ISDIR         0x40000000U
#define IN_ONESHOT       0x80000000U
#define IN_CLOSE         (IN_CLOSE_WRITE | IN_CLOSE_NOWRITE)
#define IN_MOVE          (IN_MOVED_FROM | IN_MOVED_TO)
#define IN_ALL_EVENTS                                                                                 \
    (IN_ACCESS | IN_MODIFY | IN_ATTRIB | IN_CLOSE_WRITE | IN_CLOSE_NOWRITE | IN_OPEN | IN_MOVED_FROM | \
     IN_MOVED_TO | IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_MOVE_SELF)

struct inotify_event {
    int      wd;
    uint32_t mask;
    uint32_t cookie;
    uint32_t len;
};

typedef struct anonfs_inotify_watch {
    int      wd;
    uint32_t mask;
    char    *path;
} anonfs_inotify_watch_t;

typedef struct anonfs_inotify_event_record {
    int      wd;
    uint32_t mask;
    uint32_t cookie;
    char     name[64];
} anonfs_inotify_event_record_t;

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
    ANONFS_INOTIFY,
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

typedef struct anonfs_inotify {
    spin_t   lock;
    uint32_t flags;
    int      next_wd;
    size_t   watch_count;
    anonfs_inotify_watch_t watches[64];
    size_t   event_head;
    size_t   event_tail;
    size_t   event_count;
    anonfs_inotify_event_record_t events[128];
} anonfs_inotify_t;

typedef struct anonfs_handle {
    anonfs_kind_t kind;
    union {
        anonfs_eventfd_t eventfd;
        anonfs_signalfd_t signalfd;
        anonfs_timerfd_t timerfd;
        anonfs_memfd_t memfd;
        anonfs_inotify_t inotify;
    } u;
} anonfs_handle_t;

void anonfs_init(void);
int  anonfs_create_eventfd(uint64_t initial_value, uint32_t flags);
int  anonfs_create_signalfd(int ufd, const sigset_t *mask, size_t sizemask, uint32_t flags);
int  anonfs_create_timerfd(int clockid, uint32_t flags);
int  anonfs_timerfd_settime(int fd, int flags, const struct itimerspec *new_value, struct itimerspec *old_value);
int  anonfs_create_memfd(const char *name, uint32_t flags);
int  anonfs_create_inotify(uint32_t flags);
int  anonfs_inotify_add_watch(int fd, const char *pathname, uint32_t mask);
int  anonfs_inotify_rm_watch(int fd, int wd);
void anonfs_notify_path_event(const char *path, uint32_t mask);
size_t anonfs_notify_signal(pcb_t process, int sig, const siginfo_t *info);

#ifdef __cplusplus
}
#endif
