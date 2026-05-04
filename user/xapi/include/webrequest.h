#pragma once

#include "stdint.h"
#include "xposix/stdbool.h"

char *get(const char *url, uint16_t port, bool tls);
