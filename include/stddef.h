#pragma once

#include <stdint.h>

typedef __PTRDIFF_TYPE__ ptrdiff_t;

#ifndef NULL
#define NULL 0
#endif

#ifndef offsetof
#define offsetof(type, member) __builtin_offsetof(type, member)
#endif
