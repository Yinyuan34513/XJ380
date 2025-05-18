#ifndef _WINDOW_H_
#define _WINDOW_H_

#include "../../include.h"

typedef struct WINDOWLS *WINDOWLSP;

struct WINDOWLS
{
    UINT16    w_sheet;
    UINT8     title_length;
    CHAR8    *title;
    UINT16    number;
    WINDOWLSP next;
};

typedef struct
{
    UINT16    win_num;
    WINDOWLS *start;
} XWM_INFO;

#endif
