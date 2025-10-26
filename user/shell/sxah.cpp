#include "../xapi/include/x3api.h"

#define SXAH_CREATE_KERNEL_TERMINAL 3802    // 创建内核态进程
#define SXAH_CHECK_USER_PASSWORD    3803    // 检查用户密码
#define SXAH_MARK_IS_TERMINAL       3804    // 标记为命令行
#define SXAH_WRITE_INPUT_BUFFER     3805    // 写入XTTTP输入缓冲区
#define SXAH_READ_OUTPUT_BUFFER     3806    // 读取XTTTP输出缓冲区

uint64_t enter_syscall(uint64_t syscall_number, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4,
                       uint64_t arg5, uint64_t arg6);

void mark_terminal()
{
    enter_syscall(SXAH_MARK_IS_TERMINAL, 0, 0, 0, 0, 0, 0);
}

void write_xttp_buffer(char *str)
{
    enter_syscall(SXAH_WRITE_INPUT_BUFFER, (uint64_t)str, 0, 0, 0, 0, 0);
}

void read_xttp_buffer(char *str)
{
    enter_syscall(SXAH_READ_OUTPUT_BUFFER, (uint64_t)str, 0, 0, 0, 0, 0);
}
