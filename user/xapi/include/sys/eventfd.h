#pragma once

#include "../xposix/fcntl.h"
#include "../xposix/stdint.h"

#define EFD_SEMAPHORE 0x1
#define EFD_CLOEXEC   O_CLOEXEC
#define EFD_NONBLOCK  O_NONBLOCK

#ifdef __cplusplus
extern "C" {
#endif

int eventfd(unsigned int initval, int flags);

#ifdef __cplusplus
}
#endif
