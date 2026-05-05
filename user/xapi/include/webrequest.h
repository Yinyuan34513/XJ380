#pragma once

#include "stdint.h"
#include "xposix/stdbool.h"

char *xapi_httpGet(const char *url, uint16_t port, bool tls);
