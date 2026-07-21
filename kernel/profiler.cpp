#include "../kernel/build_settings.h"
#include <krlibc.h>
#include <profiler.h>
#include <proto.hpp>
#include <syscall/syscall.h>
#include <task/pcb.h>

#if CONFIG_KERNEL_PROFILER

#define PROFILER_MAX_SYSCALLS      512
#define PROFILER_MAX_PROCESSES     64
#define PROFILER_TOP_SYSCALLS      10
#define PROFILER_TOP_PROCESSES     8
#define PROFILER_REPORT_NS         5000000000ULL
#define PROFILER_SLOW_SYSCALL_NS   200000000ULL
#define PROFILER_SLOW_BLOCK_IO_NS  50000000ULL
#define PROFILER_BLOCK_BUCKETS     6

typedef struct {
    uint64_t calls;
    uint64_t errors;
    uint64_t total_ns;
    uint64_t max_ns;
} profiler_syscall_stat_t;

typedef struct {
    uint64_t pid;
    uint64_t calls;
    uint64_t total_ns;
    uint64_t max_ns;
    char     name[32];
} profiler_process_stat_t;

typedef struct {
    uint64_t ops;
    uint64_t bytes;
    uint64_t total_ns;
    uint64_t max_ns;
} profiler_block_dir_stat_t;

typedef struct {
    uint64_t ops;
    uint64_t total_ns;
} profiler_block_bucket_t;

static profiler_syscall_stat_t  g_syscalls[PROFILER_MAX_SYSCALLS];
static profiler_process_stat_t  g_processes[PROFILER_MAX_PROCESSES];
static profiler_block_dir_stat_t g_block_read;
static profiler_block_dir_stat_t g_block_write;
static profiler_block_bucket_t   g_block_buckets[PROFILER_BLOCK_BUCKETS];
static uint64_t                  g_boot_ns;
static uint64_t                  g_last_report_ns;
static volatile uint64_t         g_reporting;

static inline void profiler_add_u64(uint64_t *ptr, uint64_t value)
{
    __atomic_fetch_add(ptr, value, __ATOMIC_RELAXED);
}

static inline void profiler_max_u64(uint64_t *ptr, uint64_t value)
{
    uint64_t old = __atomic_load_n(ptr, __ATOMIC_RELAXED);
    while (value > old && !__atomic_compare_exchange_n(ptr, &old, value, false, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
    }
}

static uint64_t profiler_load_u64(const uint64_t *ptr)
{
    return __atomic_load_n(ptr, __ATOMIC_RELAXED);
}

static const char *profiler_syscall_name(uint64_t nr)
{
    switch (nr)
    {
    case SYS_READ: return "read";
    case SYS_WRITE: return "write";
    case SYS_OPEN: return "open";
    case SYS_CLOSE: return "close";
    case SYS_STAT: return "stat";
    case SYS_FSTAT: return "fstat";
    case SYS_LSTAT: return "lstat";
    case SYS_POLL: return "poll";
    case SYS_LSEEK: return "lseek";
    case SYS_MMAP: return "mmap";
    case SYS_MPROTECT: return "mprotect";
    case SYS_MUNMAP: return "munmap";
    case SYS_BRK: return "brk";
    case SYS_IOCTL: return "ioctl";
    case SYS_PREAD64: return "pread64";
    case SYS_PWRITE64: return "pwrite64";
    case SYS_READV: return "readv";
    case SYS_WRITEV: return "writev";
    case SYS_ACCESS: return "access";
    case SYS_PIPE: return "pipe";
    case SYS_SELECT: return "select";
    case SYS_SCHED_YIELD: return "sched_yield";
    case SYS_SCHED_SETSCHEDULER: return "sched_setscheduler";
    case SYS_MREMAP: return "mremap";
    case SYS_MINCORE: return "mincore";
    case SYS_MADVISE: return "madvise";
    case SYS_DUP: return "dup";
    case SYS_DUP2: return "dup2";
    case SYS_NANOSLEEP: return "nanosleep";
    case SYS_GETPID: return "getpid";
    case SYS_SENDFILE: return "sendfile";
    case SYS_SOCKET: return "socket";
    case SYS_CONNECT: return "connect";
    case SYS_ACCEPT: return "accept";
    case SYS_SENDTO: return "sendto";
    case SYS_RECVFROM: return "recvfrom";
    case SYS_SENDMSG: return "sendmsg";
    case SYS_RECVMSG: return "recvmsg";
    case SYS_SOCKETPAIR: return "socketpair";
    case SYS_BIND: return "bind";
    case SYS_LISTEN: return "listen";
    case SYS_GETSOCKNAME: return "getsockname";
    case SYS_GETPEERNAME: return "getpeername";
    case SYS_SETSOCKOPT: return "setsockopt";
    case SYS_GETSOCKOPT: return "getsockopt";
    case SYS_CLONE: return "clone";
    case SYS_FORK: return "fork";
    case SYS_VFORK: return "vfork";
    case SYS_EXECVE: return "execve";
    case SYS_EXIT: return "exit";
    case SYS_WAIT4: return "wait4";
    case SYS_KILL: return "kill";
    case SYS_UNAME: return "uname";
    case SYS_FCNTL: return "fcntl";
    case SYS_FLOCK: return "flock";
    case SYS_FTRUNCATE: return "ftruncate";
    case SYS_GETDENTS: return "getdents";
    case SYS_GETCWD: return "getcwd";
    case SYS_CHDIR: return "chdir";
    case SYS_FCHDIR: return "fchdir";
    case SYS_RENAME: return "rename";
    case SYS_MKDIR: return "mkdir";
    case SYS_RMDIR: return "rmdir";
    case SYS_CREAT: return "creat";
    case SYS_LINK: return "link";
    case SYS_UNLINK: return "unlink";
    case SYS_SYMLINK: return "symlink";
    case SYS_READLINK: return "readlink";
    case SYS_CHMOD: return "chmod";
    case SYS_FCHMOD: return "fchmod";
    case SYS_CHOWN: return "chown";
    case SYS_FCHOWN: return "fchown";
    case SYS_LCHOWN: return "lchown";
    case SYS_GETTIMEOFDAY: return "gettimeofday";
    case SYS_SYSINFO: return "sysinfo";
    case SYS_PRCTL: return "prctl";
    case SYS_ARCH_PRCTL: return "arch_prctl";
    case SYS_SYNC: return "sync";
    case SYS_GETTID: return "gettid";
    case SYS_SETXATTR: return "setxattr";
    case SYS_LSETXATTR: return "lsetxattr";
    case SYS_FSETXATTR: return "fsetxattr";
    case SYS_GETXATTR: return "getxattr";
    case SYS_LGETXATTR: return "lgetxattr";
    case SYS_FGETXATTR: return "fgetxattr";
    case SYS_LISTXATTR: return "listxattr";
    case SYS_LLISTXATTR: return "llistxattr";
    case SYS_FLISTXATTR: return "flistxattr";
    case SYS_REMOVEXATTR: return "removexattr";
    case SYS_LREMOVEXATTR: return "lremovexattr";
    case SYS_FREMOVEXATTR: return "fremovexattr";
    case SYS_FUTEX: return "futex";
    case SYS_SCHED_SETAFFINITY: return "sched_setaffinity";
    case SYS_SCHED_GETAFFINITY: return "sched_getaffinity";
    case SYS_EPOLL_CREATE: return "epoll_create";
    case SYS_GETDENTS64: return "getdents64";
    case SYS_GETRLIMIT: return "getrlimit";
    case SYS_SET_TID_ADDRESS: return "set_tid_address";
    case SYS_FADVISE64: return "fadvise64";
    case SYS_CLOCK_GETTIME: return "clock_gettime";
    case SYS_CLOCK_GETRES: return "clock_getres";
    case SYS_CLOCK_NANOSLEEP: return "clock_nanosleep";
    case SYS_EXIT_GROUP: return "exit_group";
    case SYS_EPOLL_WAIT: return "epoll_wait";
    case SYS_EPOLL_CTL: return "epoll_ctl";
    case SYS_TGKILL: return "tgkill";
    case SYS_UTIMES: return "utimes";
    case SYS_WAITID: return "waitid";
    case SYS_OPENAT: return "openat";
    case SYS_MKDIRAT: return "mkdirat";
    case SYS_FCHOWNAT: return "fchownat";
    case SYS_NEWFSTATAT: return "newfstatat";
    case SYS_UNLINKAT: return "unlinkat";
    case SYS_RENAMEAT: return "renameat";
    case SYS_RENAMEAT2: return "renameat2";
    case SYS_LINKAT: return "linkat";
    case SYS_SYMLINKAT: return "symlinkat";
    case SYS_READLINKAT: return "readlinkat";
    case SYS_FCHMODAT: return "fchmodat";
    case SYS_FACCESSAT: return "faccessat";
    case SYS_PSELECT6: return "pselect6";
    case SYS_PPOLL: return "ppoll";
    case SYS_SYNC_FILE_RANGE: return "sync_file_range";
    case SYS_UTIMENSAT: return "utimensat";
    case SYS_EPOLL_PWAIT: return "epoll_pwait";
    case SYS_EPOLL_PWAIT2: return "epoll_pwait2";
    case SYS_SIGNALFD: return "signalfd";
    case SYS_TIMERFD_CREATE: return "timerfd_create";
    case SYS_EVENTFD: return "eventfd";
    case SYS_FALLOCATE: return "fallocate";
    case SYS_ACCEPT4: return "accept4";
    case SYS_SIGNALFD4: return "signalfd4";
    case SYS_EVENTFD2: return "eventfd2";
    case SYS_EPOLL_CREATE1: return "epoll_create1";
    case SYS_DUP3: return "dup3";
    case SYS_PIPE2: return "pipe2";
    case SYS_INOTIFY_INIT: return "inotify_init";
    case SYS_INOTIFY_ADD_WATCH: return "inotify_add_watch";
    case SYS_INOTIFY_RM_WATCH: return "inotify_rm_watch";
    case SYS_INOTIFY_INIT1: return "inotify_init1";
    case SYS_MEMFD_CREATE: return "memfd_create";
    case SYS_COPY_FILE_RANGE: return "copy_file_range";
    case SYS_STATX: return "statx";
    case SYS_CLONE3: return "clone3";
    case SYS_CLOSE_RANGE: return "close_range";
    case SYS_FACCESSAT2: return "faccessat2";
    case SYS_FCHMODAT2: return "fchmodat2";
    default: return "?";
    }
}

static uint32_t profiler_block_bucket(uint64_t bytes)
{
    if (bytes <= 4096) return 0;
    if (bytes <= 16384) return 1;
    if (bytes <= 65536) return 2;
    if (bytes <= 262144) return 3;
    if (bytes <= 1048576) return 4;
    return 5;
}

static const char *profiler_block_bucket_name(uint32_t bucket)
{
    switch (bucket)
    {
    case 0: return "<=4K";
    case 1: return "<=16K";
    case 2: return "<=64K";
    case 3: return "<=256K";
    case 4: return "<=1M";
    default: return ">1M";
    }
}

static profiler_process_stat_t *profiler_process_slot(tcb_t task)
{
    if (task == NULL || task->parent_group == NULL) return NULL;

    uint64_t pid = task->parent_group->pid;
    uint64_t empty = (uint64_t)-1;
    for (size_t i = 0; i < PROFILER_MAX_PROCESSES; i++)
    {
        uint64_t current_pid = profiler_load_u64(&g_processes[i].pid);
        if (current_pid == pid) return &g_processes[i];
        if (current_pid == 0 && empty == (uint64_t)-1) empty = i;
    }

    if (empty != (uint64_t)-1)
    {
        uint64_t expected = 0;
        if (__atomic_compare_exchange_n(&g_processes[empty].pid, &expected, pid, false,
                                        __ATOMIC_RELAXED, __ATOMIC_RELAXED))
        {
            const char *name = task->parent_group->name[0] != '\0' ? task->parent_group->name : task->name;
            strncpy(g_processes[empty].name, name != NULL ? name : "?", sizeof(g_processes[empty].name) - 1);
            g_processes[empty].name[sizeof(g_processes[empty].name) - 1] = '\0';
            return &g_processes[empty];
        }
    }

    size_t slot = pid % PROFILER_MAX_PROCESSES;
    if (profiler_load_u64(&g_processes[slot].pid) != pid)
    {
        __atomic_store_n(&g_processes[slot].pid, pid, __ATOMIC_RELAXED);
        __atomic_store_n(&g_processes[slot].calls, 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_processes[slot].total_ns, 0, __ATOMIC_RELAXED);
        __atomic_store_n(&g_processes[slot].max_ns, 0, __ATOMIC_RELAXED);
        const char *name = task->parent_group->name[0] != '\0' ? task->parent_group->name : task->name;
        strncpy(g_processes[slot].name, name != NULL ? name : "?", sizeof(g_processes[slot].name) - 1);
        g_processes[slot].name[sizeof(g_processes[slot].name) - 1] = '\0';
    }
    return &g_processes[slot];
}

static void profiler_record_process(uint64_t elapsed_ns)
{
    profiler_process_stat_t *process = profiler_process_slot(get_current_task());
    if (process == NULL) return;

    profiler_add_u64(&process->calls, 1);
    profiler_add_u64(&process->total_ns, elapsed_ns);
    profiler_max_u64(&process->max_ns, elapsed_ns);
}

static void profiler_print_ns(uint64_t ns)
{
    write_serial_fmt("%llu.%03llums",
                     (unsigned long long)(ns / 1000000ULL),
                     (unsigned long long)((ns / 1000ULL) % 1000ULL));
}

static void profiler_report_syscalls(void)
{
    bool used[PROFILER_MAX_SYSCALLS] = {};

    write_serial_string("[prof] top syscalls by time:\n");
    for (size_t rank = 0; rank < PROFILER_TOP_SYSCALLS; rank++)
    {
        uint64_t best_time = 0;
        size_t best = PROFILER_MAX_SYSCALLS;
        for (size_t i = 0; i < PROFILER_MAX_SYSCALLS; i++)
        {
            if (used[i]) continue;
            uint64_t total = profiler_load_u64(&g_syscalls[i].total_ns);
            if (total > best_time)
            {
                best_time = total;
                best = i;
            }
        }
        if (best == PROFILER_MAX_SYSCALLS || best_time == 0) break;
        used[best] = true;

        uint64_t calls = profiler_load_u64(&g_syscalls[best].calls);
        uint64_t errors = profiler_load_u64(&g_syscalls[best].errors);
        uint64_t max_ns = profiler_load_u64(&g_syscalls[best].max_ns);
        write_serial_fmt("[prof]   #%llu nr=%llu(%s) calls=%llu err=%llu total=",
                         (unsigned long long)(rank + 1),
                         (unsigned long long)best,
                         profiler_syscall_name(best),
                         (unsigned long long)calls,
                         (unsigned long long)errors);
        profiler_print_ns(best_time);
        write_serial_string(" max=");
        profiler_print_ns(max_ns);
        if (calls != 0)
        {
            write_serial_string(" avg=");
            profiler_print_ns(best_time / calls);
        }
        write_serial_string("\n");
    }
}

static void profiler_report_processes(void)
{
    bool used[PROFILER_MAX_PROCESSES] = {};

    write_serial_string("[prof] top processes by syscall time:\n");
    for (size_t rank = 0; rank < PROFILER_TOP_PROCESSES; rank++)
    {
        uint64_t best_time = 0;
        size_t best = PROFILER_MAX_PROCESSES;
        for (size_t i = 0; i < PROFILER_MAX_PROCESSES; i++)
        {
            if (used[i] || profiler_load_u64(&g_processes[i].pid) == 0) continue;
            uint64_t total = profiler_load_u64(&g_processes[i].total_ns);
            if (total > best_time)
            {
                best_time = total;
                best = i;
            }
        }
        if (best == PROFILER_MAX_PROCESSES || best_time == 0) break;
        used[best] = true;

        uint64_t calls = profiler_load_u64(&g_processes[best].calls);
        uint64_t max_ns = profiler_load_u64(&g_processes[best].max_ns);
        write_serial_fmt("[prof]   #%llu pid=%llu task=%s calls=%llu total=",
                         (unsigned long long)(rank + 1),
                         (unsigned long long)profiler_load_u64(&g_processes[best].pid),
                         g_processes[best].name,
                         (unsigned long long)calls);
        profiler_print_ns(best_time);
        write_serial_string(" max=");
        profiler_print_ns(max_ns);
        write_serial_string("\n");
    }
}

static void profiler_report_block_dir(const char *name, const profiler_block_dir_stat_t *stat)
{
    uint64_t ops = profiler_load_u64(&stat->ops);
    uint64_t bytes = profiler_load_u64(&stat->bytes);
    uint64_t total_ns = profiler_load_u64(&stat->total_ns);
    uint64_t max_ns = profiler_load_u64(&stat->max_ns);

    write_serial_fmt("[prof] block %s ops=%llu bytes=%llu",
                     name,
                     (unsigned long long)ops,
                     (unsigned long long)bytes);
    if (ops != 0)
    {
        uint64_t mib = bytes / (1024ULL * 1024ULL);
        write_serial_fmt(" MiB=%llu avg_bytes=%llu total=",
                         (unsigned long long)mib,
                         (unsigned long long)(bytes / ops));
        profiler_print_ns(total_ns);
        write_serial_string(" avg=");
        profiler_print_ns(total_ns / ops);
        write_serial_string(" max=");
        profiler_print_ns(max_ns);
    }
    write_serial_string("\n");
}

static void profiler_report_block(void)
{
    profiler_report_block_dir("read ", &g_block_read);
    profiler_report_block_dir("write", &g_block_write);
    write_serial_string("[prof] block request buckets:");
    for (uint32_t i = 0; i < PROFILER_BLOCK_BUCKETS; i++)
    {
        uint64_t ops = profiler_load_u64(&g_block_buckets[i].ops);
        uint64_t total_ns = profiler_load_u64(&g_block_buckets[i].total_ns);
        write_serial_fmt(" %s=%llu", profiler_block_bucket_name(i), (unsigned long long)ops);
        if (ops != 0) write_serial_fmt("/%llums", (unsigned long long)(total_ns / ops / 1000000ULL));
    }
    write_serial_string("\n");
}

static void profiler_maybe_report(uint64_t now_ns)
{
    if (g_boot_ns == 0)
    {
        g_boot_ns = now_ns;
        g_last_report_ns = now_ns;
        write_serial_string("[prof] kernel profiler enabled: summary every 5s, slow syscall >=200ms, slow block IO >=50ms\n");
        return;
    }

    uint64_t last = profiler_load_u64(&g_last_report_ns);
    if (now_ns < last || now_ns - last < PROFILER_REPORT_NS) return;
    if (__atomic_test_and_set(&g_reporting, __ATOMIC_ACQUIRE)) return;

    last = profiler_load_u64(&g_last_report_ns);
    if (now_ns >= last && now_ns - last >= PROFILER_REPORT_NS)
    {
        __atomic_store_n(&g_last_report_ns, now_ns, __ATOMIC_RELAXED);
        write_serial_fmt("[prof] summary uptime=%llus\n", (unsigned long long)((now_ns - g_boot_ns) / 1000000000ULL));
        profiler_report_syscalls();
        profiler_report_processes();
        profiler_report_block();
    }

    __atomic_clear(&g_reporting, __ATOMIC_RELEASE);
}

void profiler_syscall_exit(uint64_t nr, uint64_t elapsed_ns, int64_t ret)
{
    if (nr < PROFILER_MAX_SYSCALLS)
    {
        profiler_syscall_stat_t *stat = &g_syscalls[nr];
        profiler_add_u64(&stat->calls, 1);
        if (ret < 0) profiler_add_u64(&stat->errors, 1);
        profiler_add_u64(&stat->total_ns, elapsed_ns);
        profiler_max_u64(&stat->max_ns, elapsed_ns);
    }

    profiler_record_process(elapsed_ns);

    uint64_t now_ns = bootNanoTime();
    if (elapsed_ns >= PROFILER_SLOW_SYSCALL_NS)
    {
        tcb_t task = get_current_task();
        pcb_t proc = task != NULL ? task->parent_group : NULL;
        write_serial_fmt("[prof] slow syscall nr=%llu(%s) task=%s pid=%llu ret=%lld dur=",
                         (unsigned long long)nr,
                         profiler_syscall_name(nr),
                         task != NULL ? task->name : "?",
                         (unsigned long long)(proc != NULL ? proc->pid : 0),
                         (long long)ret);
        profiler_print_ns(elapsed_ns);
        write_serial_string("\n");
    }
    profiler_maybe_report(now_ns);
}

void profiler_block_io(bool write, int drive, uint64_t offset, uint64_t length, uint64_t elapsed_ns, int64_t ret)
{
    profiler_block_dir_stat_t *stat = write ? &g_block_write : &g_block_read;
    profiler_add_u64(&stat->ops, 1);
    if (ret > 0) profiler_add_u64(&stat->bytes, (uint64_t)ret);
    profiler_add_u64(&stat->total_ns, elapsed_ns);
    profiler_max_u64(&stat->max_ns, elapsed_ns);

    uint32_t bucket = profiler_block_bucket(length);
    profiler_add_u64(&g_block_buckets[bucket].ops, 1);
    profiler_add_u64(&g_block_buckets[bucket].total_ns, elapsed_ns);

    if (elapsed_ns >= PROFILER_SLOW_BLOCK_IO_NS)
    {
        tcb_t task = get_current_task();
        write_serial_fmt("[prof] slow block %s drive=%d task=%s off=%llu len=%llu ret=%lld dur=",
                         write ? "write" : "read",
                         drive,
                         task != NULL ? task->name : "?",
                         (unsigned long long)offset,
                         (unsigned long long)length,
                         (long long)ret);
        profiler_print_ns(elapsed_ns);
        write_serial_string("\n");
    }
}

#else

void profiler_syscall_exit(uint64_t nr, uint64_t elapsed_ns, int64_t ret)
{
    (void)nr;
    (void)elapsed_ns;
    (void)ret;
}

void profiler_block_io(bool write, int drive, uint64_t offset, uint64_t length, uint64_t elapsed_ns, int64_t ret)
{
    (void)write;
    (void)drive;
    (void)offset;
    (void)length;
    (void)elapsed_ns;
    (void)ret;
}

#endif
