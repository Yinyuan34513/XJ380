#pragma once
#include <proto.hpp>

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

// XJ380API XAPI Edition
#define XAPI_OFFEST 380

// 一些隐藏的API，不对外开放，严禁泄露！
#define SXAH_SYSCALL_RETURN         3801
#define SXAH_CREATE_KERNEL_TERMINAL 3802

// P3.1
#define XAPI_OUTPUT    381
#define XAPI_INPUTW    382
#define XAPI_INPUT     383
#define XAPI_ENDLINE   384
#define XAPI_PRINTLINE 385
#define XAPI_PRINTF    386

// P3.2
#define XAPI_OPEN_FILE  387
#define XAPI_CLOSE_FILE 388

// P3.4
#define XAPI_FORK   389
#define XAPI_EXECVE 390

// P3.5
#define XAPI_GET_VERSION    391

// P4.1
#define XAPI_CREATE_WINDOW    392
#define XAPI_SET_WINDOW_TITLE 393
#define XAPI_CLOSE_WINDOW     394
#define XAPI_SET_ICON         395

// P4.2
#define XAPI_DRAW_POINT       396
#define XAPI_DRAW_LINE        397
#define XAPI_DRAW_RECT        398
#define XAPI_DRAW_RECT_FILL   399
#define XAPI_DRAW_CIRCLE      400
#define XAPI_DRAW_CIRCLE_FILL 401
#define XAPI_DRAW_TEXT        402

// P4.3
#define XAPI_DRAW_BMP 403
#define XAPI_DRAW_PNG 404

// P4.4
#define XAPI_SET_MSH_PROCOR 405

#define MSG_CHAR    0
#define MSG_MOVE    1
#define MSG_LBUTTON 2
#define MSG_RBUTTON 3
#define MSG_MBUTTON 4
#define MSG_ROLLER  5
#define MSG_CRL     6

// P4.5
#define XAPI_READBUFFER   406
#define XAPI_WRITEBUFFER  407
#define XAPI_WRITEBUFFERA 408
#define XAPI_REFRESH_WINDOW 409

// P4.6
#define XAPI_BUTTON       410
#define XAPI_BUTTON_STYLE 411

// Proto (XAPI Edition)
int do_xapi_CreateWindow(uint64_t handle, uint64_t xwin);
int do_xapi_SetWindowTitle(uint64_t handle, uint64_t str);
int do_xapi_CloseWindow(uint64_t handle);
int do_xapi_SetIcon(uint64_t handle, uint64_t path);
int do_xapi_DrawPoint(uint64_t handle, uint32_t x, uint32_t y, uint32_t color);
int do_xapi_DrawLine(uint64_t handle, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t color);
int do_xapi_DrawRect_fill(uint64_t handle, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t color);
int do_xapi_DrawRect(uint64_t handle, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t color);
// int do_xapi_DrawCircle_fill(uint64_t handle, uint32_t x, uint32_t y, uint32_t radius, uint32_t color);
// int do_xapi_DrawCircle(uint64_t handle, uint32_t x, uint32_t y, uint32_t radius, uint32_t color);
int do_xapi_DrawText(uint64_t handle, uint32_t x, uint32_t y, uint64_t str, uint32_t size, uint32_t color);
int do_xapi_RefreshWindow(uint64_t handle);
int do_xapi_SetMsgProc(uint64_t handle, uint64_t func);

// // Proto (POSIX Edition)
// int sys_read(uint32_t fd, uint8_t *buf, size_t count);
// int sys_write(uint32_t fd, uint8_t *buf, size_t count);
// int sys_readv(uint32_t fd, struct iovec *iovec, size_t count);
// int sys_writev(uint32_t fd, struct iovec *iovec, size_t count);
// int sys_open(const char *pathname, int flags, int mode);
// int sys_close(uint32_t fd);
// int sys_fstat(uint32_t fd, struct stat *stat);
// int sys_execve(struct X64_REGS *regs);
// int sys_exit(int status);
// int sys_fork(struct X64_REGS *regs);
// int sys_vfork(struct X64_REGS *regs);
// uint64_t sys_wait4(unsigned long pid, long *status, int options, void *rusage);
// uint64_t sys_brk(uint64_t addr);

#define PROT_NONE  0x0
#define PROT_READ  0x1
#define PROT_WRITE 0x2
#define PROT_EXEC  0x4

#define MAP_SHARED  0x1
#define MAP_PRIVATE 0x2
#define MAP_FIXED   0x10
// uint64_t sys_mmap(uint64_t addr, uint64_t len, uint64_t prot, uint64_t flags, uint64_t fd, uint64_t offset);

// uint64_t sys_arch_prctl(uint64_t cmd, uint64_t arg);

void init_syscall();

// Proto (XAPI Edition)
