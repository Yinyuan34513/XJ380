#ifndef _WINDOW_H_
#define _WINDOW_H_

#include "../../include.h"

typedef struct WINDOWLS *WINDOWLSP;

struct WINDOWLS
{
    UINT16    w_sheet;
    UINT16    id;
    UINT8     title_length;
    CHAR8    *title;
    WINDOWLSP next;
};

typedef struct
{
    WINDOWLS *start;
} XWM_INFO;

#endif
