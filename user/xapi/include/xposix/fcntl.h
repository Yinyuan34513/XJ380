#pragma once

#include "../stdint.h"

#define F_DUPFD         0
#define F_GETFD         1
#define F_SETFD         2
#define F_GETFL         3
#define F_SETFL         4
#define F_DUPFD_CLOEXEC 1030

#define O_APPEND   02000
#define O_NONBLOCK 04000
#define O_NDELAY   O_NONBLOCK

#ifdef __cplusplus
extern "C" {
#endif

int fcntl(int fd, int cmd, uint64_t arg);

#ifdef __cplusplus
}
#endif
