#ifndef _SHEET_H_
#define _SHEET_H_

#include <proto.hpp>
#include <efi/fbc.h>
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
    int bx, by;
    UINT32 width, height;
    UINT32 type;
    UINT16 index;
    bool is_change;
    SHEETP next;
};

typedef struct
{
    SHEET *start;
    VOID *temp_buffer;
    UINT32 scrx, scry;
    UINT16 sheet_num; // 图层数量 - 1
    UINT16 sht_numed; // 图层数量 - 1，用于编号
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

typedef struct
{
    UINT8 a;
    UINT8 r;
    UINT8 g;
    UINT8 b;
} SHEET_BUFFER_ARGB;

#endif
