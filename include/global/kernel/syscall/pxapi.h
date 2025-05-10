#pragma once

#include "../../../include.h"

struct WINDOW_HANDLE {
    WINDOWLS* WindowNodePtr;
};

struct XWINDOW
{
    UINT32 	width;
    UINT32 	height;
    char   *title;
    UINT8 	sets;
};
