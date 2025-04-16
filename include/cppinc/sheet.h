#ifndef _SHEET_H_
#define _SHEET_H_

#include "../include.h"

enum SystemSheetType
{
    FixedSheetType,
    MovableSheetType,
    TopSheetType
};

typedef struct SHEET *SHEETP;

struct SHEET
{
    SHEETP front;
    VOID *buffer;
    UINT32 bx, by;
    UINT32 width, height;
    UINT32 type;
    UINT16 index;
    BOOLEAN is_change;
    SHEETP next;
};

typedef struct
{
    SHEET *start;
    VOID *temp_buffer;
    UINT32 scrx, scry;
    UINT16 sheet_num; // 图层数量 - 1
    const FrameBufferConfig *fbc;
} SHEET_INFO;

typedef struct
{
    UINT8 r;
    UINT8 g;
    UINT8 b;
    UINT8 a; // 在TempBuffer里，这个没用
} SHEET_BUFFER;

typedef struct
{
    UINT8 r;
    UINT8 g;
    UINT8 b;
} __attribute__((packed)) SHEET_BUFFER_WOA;

#endif
