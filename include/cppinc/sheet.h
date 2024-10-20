#ifndef _SHEET_H_
#define _SHEET_H_

#include "../include.h"

#define MAX_SHEET       256

enum SystemSheetType {
    FixedSheetType,
    MovableSheetType,
    TopSheetType
};

typedef struct {
    VOID *buffer;
    UINT32 bx, by;
    UINT32 width, height;
    UINT32 flag;
    UINT32 type;
    UINT16 *number;
} SHEET;

typedef struct
{
    SHEET sheet[MAX_SHEET];
    VOID *temp_buffer;
    UINT32 scrx, scry;
    INT32 sheet_num;
    const FrameBufferConfig *fbc;
} SHEET_INFO;

typedef struct
{
    UINT8 r;
    UINT8 g;
    UINT8 b;
    UINT8 a;    // 在TempBuffer里，这个没用
} SHEET_BUFFER;


#endif
