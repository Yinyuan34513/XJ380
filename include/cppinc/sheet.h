#ifndef _SHEET_H_
#define _SHEET_H_

#include "../include.h"

#define MAX_SHEET       256

enum SystemSheetType {
    FixedSheetType,
    MovableSheetType,
    TopSheetType
};

enum SystemSheetColorType {
    NoAlpha,
    NeedAlpha
};

struct SHEET {
    VOID *buffer;
    UINT32 bx, by;
    UINT32 width, height;
    UINT32 flag;
    UINT32 type;
    UINT32 col_type;
    UINT16 *number;
};

struct SHEET_INFO
{
    SHEET sheet[MAX_SHEET];
    VOID *temp_buffer;
    UINT32 scrx, scry;
    UINT32 sheet_num;
    const FrameBufferConfig *fbc;
};

struct SHEET_BUFFER
{
    UINT8 r;
    UINT8 g;
    UINT8 b;
    UINT8 a;    // 在TempBuffer里，这个没用
};


#endif
