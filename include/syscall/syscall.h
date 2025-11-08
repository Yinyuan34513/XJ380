#pragma once
#include <proto.hpp>
#include "pxapi.h"

#define XJ380_PRIVATE_SIGNAL_REVERT_ADDRESS 0xf717f000
#define XPSR_OFFEST                         0x927

// XJ380API POSIX Edition
#define SYS_READ     0
#define SYS_WRITE    1
#define SYS_OPEN     2
#define SYS_CLOSE    3
#define SYS_STAT     4
#define SYS_FSTAT    5
#define SYS_LSTAT    6
#define SYS_POLL     7
#define SYS_LSEEK    8
#define SYS_MMAP     9
#define SYS_MPROTECT 10
#define SYS_MUNMAP   11
#define SYS_BRK      12

#define SYS_RT_SIGPROCMASK 14

#define SYS_IOCTL 16

#define SYS_READV  19
#define SYS_WRITEV 20

#define SYS_PIPE 22

#define SYS_MREMAP 25
#define SYS_MSYNC  26

#define SYS_DUP       32
#define SYS_DUP2      33
#define SYS_PAUSE     34
#define SYS_NANOSLEEP 35

#define SYS_ALARM 37

#define SYS_GETPID 39

#define SYS_SHUTDOWN 48

#define SYS_FORK   57
#define SYS_VFORK  58
#define SYS_EXECVE 59
#define SYS_EXIT   60
#define SYS_WAIT4  61
#define SYS_KILL   62
#define SYS_UNAME  63

#define SYS_GETDENTS 78
#define SYS_GETCWD   79
#define SYS_CHDIR    80
#define SYS_FCHDIR   81

#define SYS_MKDIR    83
#define SYS_RMDIR    84
#define SYS_CREAT    85
#define SYS_LINK     86
#define SYS_UNLINK   87
#define SYS_SYMLINK  88
#define SYS_READLINK 89
#define SYS_CHMOD    90
#define SYS_FCHMOD   91

#define SYS_MKNOD 133

#define SYS_ARCH_PRCTL 158

#define SYS_GETTID 186

#define SYS_SET_TID_ADDRESS 218

#define SYS_EXIT_GROUP 231

#define SYS_SET_ROBUST_LIST 273

#define SYS_RSEQ 334

typedef struct {
    vfs_node_t node;
    size_t     offset;
    size_t     fd;
    uint64_t   flags;
} fd_file_handle;

#define FD_SETSIZE           1024

typedef struct {
    unsigned long fds_bits[FD_SETSIZE / 8 / sizeof(long)];
} fd_set;

/*
// Proto (POSIX Edition)
int sys_read(uint32_t fd, uint8_t *buf, size_t count);
int sys_write(uint32_t fd, uint8_t *buf, size_t count);
int sys_readv(uint32_t fd, struct iovec *iovec, size_t count);
int sys_writev(uint32_t fd, struct iovec *iovec, size_t count);
int sys_open(const char *pathname, int flags, int mode);
int sys_close(uint32_t fd);
int sys_fstat(uint32_t fd, struct stat *stat);
int sys_execve(struct X64_REGS *regs);
int sys_exit(int status);
int sys_fork(struct X64_REGS *regs);
int sys_vfork(struct X64_REGS *regs);
uint64_t sys_wait4(unsigned long pid, long *status, int options, void *rusage);
uint64_t sys_brk(uint64_t addr);
*/

#define PROT_NONE  0x0
#define PROT_READ  0x1
#define PROT_WRITE 0x2
#define PROT_EXEC  0x4

#define MAP_SHARED  0x1
#define MAP_PRIVATE 0x2
#define MAP_FIXED   0x10



// uint64_t sys_mmap(uint64_t addr, uint64_t len, uint64_t prot, uint64_t flags, uint64_t fd, uint64_t offset);

// uint64_t sys_arch_prctl(uint64_t cmd, uint64_t arg);

uint64_t sys_chmod(const char *pathname, uint64_t mode);
uint64_t sys_fstatat(uint64_t dirfd, const char *pathname, struct stat *statbuf, uint64_t flags);
uint64_t sys_renameat2(uint64_t olddirfd, const char *oldpath, uint64_t newdirfd, const char *newpath, uint64_t flags);
uint64_t sys_mkdirat(uint64_t dirfd, const char *pathname, uint64_t mode);
uint64_t sys_creat(const char *pathname, uint64_t mode);