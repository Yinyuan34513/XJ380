#include "./include/xposix/poll.h"
#include "./include/libsys.h"

int poll(struct pollfd *fds, uint64_t nfds, uint64_t timeout_ms)
{
    return enter_syscall(SYS_POLL, (uint64_t)fds, nfds, timeout_ms, 0, 0, 0);
}
