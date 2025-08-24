#ifndef _WINDOW_H_
#define _WINDOW_H_

#include <proto.hpp>

typedef struct WINDOWLS *WINDOWLSP;
typedef void (*MsgPrcor)(
	UINT64 Type, 
	UINT64 hData, 
	UINT64 lData
);  // 别问为什么放在这儿 不放这儿会爆炸

struct WINDOWLS
{
    UINT16    w_sheet;
    UINT8     title_length;
    CHAR8    *title;
    UINT16    number;
    UINT32    width;
    UINT32    height;
    MsgPrcor  WinMPf;
    WINDOWLSP next;
};

typedef struct
{
    UINT16    win_num;
    WINDOWLS *start;
} XWM_INFO;

#endif
