#pragma once
#ifndef _MOUSE_H_
#    define _MOUSE_H_

#    include <graphics/sheet.h>
#    include <proto.hpp>

typedef struct
{
    uint8_t   buf[3], phase;
    int       x, y, btn;
    char      roll;
    bool      left;
    bool      center;
    bool      right;
    bool      win_move_lock;
    bool      need_flush;
    int       win_x_offset;
    int       win_y_offset;
    uint16_t *sheet_id;
    SHEET    *sht_now;
    int     scroll;
} mouse_dec;
typedef enum mouse_type {
    Standard   = 0,
    OnlyScroll = 1,
    FiveButton = 2,
} MouseType;

#    define KB_SEND2MOUSE 0xd4
#    define MOUSE_EN      0xf4

#    define KB_EN_MOUSE_INTFACE 0xa8

#endif
