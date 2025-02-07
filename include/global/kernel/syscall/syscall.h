#pragma once

#include "../../../include.h"

#define SYS_READ 0
#define SYS_WRITE 1
#define SYS_OPEN 2
#define SYS_CLOSE 3
#define SYS_STAT 4
#define SYS_FSTAT 5
#define SYS_LSTAT 6
#define SYS_POLL 7
#define SYS_LSEEK 8
#define SYS_MMAP 9
#define SYS_MPROTECT 10
#define SYS_MUNMAP 11
#define SYS_BRK 12

#define SYS_IOCTL 16

#define SYS_PIPE 22

#define SYS_MREMAP 25
#define SYS_MSYNC 26

#define SYS_DUP 32
#define SYS_DUP2 33
#define SYS_PAUSE 34
#define SYS_NANOSLEEP 35

#define SYS_ALARM 37

#define SYS_GETPID 39

#define SYS_SHUTDOWN 48

#define SYS_FORK 57
#define SYS_VFORK 58
#define SYS_EXECVE 59
#define SYS_EXIT 60
#define SYS_WAIT4 61
#define SYS_KILL 62
#define SYS_UNAME 63

#define SYS_GETDENTS 78
#define SYS_GETCWD 79
#define SYS_CHDIR 80
#define SYS_FCHDIR 81

#define SYS_MKDIR 83
#define SYS_RMDIR 84
#define SYS_CREAT 85
#define SYS_LINK 86
#define SYS_UNLINK 87
#define SYS_SYMLINK 88
#define SYS_READLINK 89
#define SYS_CHMOD 90
#define SYS_FCHMOD 91

#define SYS_MKNOD 133

int sys_read(uint32_t fd, uint8_t *buf, size_t count);
int sys_write(uint32_t fd, uint8_t *buf, size_t count);
int sys_open(const char *pathname, int flags, int mode);
int sys_close(uint32_t fd);
int sys_execve(struct X64_REGS_SYSCALL *regs);
int sys_exit(int status);
int sys_fork(struct X64_REGS_SYSCALL *regs);
uint64_t sys_wait4(unsigned long pid, int *status, int options, void *rusage);

void init_syscall();
