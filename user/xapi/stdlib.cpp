#include "./include/xposix.h"
#include "./include/libsys.h"

void exit(int status)
{
    enter_syscall(SYS_EXIT_GROUP, status, 0, 0, 0, 0, 0);
}
