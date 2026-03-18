#pragma once

extern "C"{
#include "../../../include/proto.hpp"
}

typedef uint32_t nfds_t;

struct pollfd {
    int fd;
    short events;
    short revents;
};
