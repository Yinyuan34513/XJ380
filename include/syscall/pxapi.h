#pragma once

#include <proto.hpp>

struct WINDOW_HANDLE {
    WINDOWLS* WindowNodePtr;
};

struct XWINDOW
{
    uint32_t 	width;
    uint32_t 	height;
    char   *title;
    uint8_t 	sets;
};
