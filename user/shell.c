#include "./xlibc/include/stdint.h"
#include "./xlibc/include/libsys.h"

int main(int argc, char *argv[], char *envp[])
{
    enter_syscall(SYS_WRITE, 1, (uint64_t)"shell process is running!!!\n", 0, 0, 0, 0);

    enter_syscall(SYS_EXIT, 0, 0, 0, 0, 0, 0);
}
