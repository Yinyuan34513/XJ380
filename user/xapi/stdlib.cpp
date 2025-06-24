#include "./include/libsys.h"
#include "./include/xposix.h"

void exit(int status) {
    enter_syscall(SYS_EXIT, status, 0, 0, 0, 0, 0);
}
