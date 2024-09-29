#ifndef _SHEET_H_
#define _SHEET_H_

#include "../include.h"

#define MAX_SHEET       256

enum SystemSheetType {
    FixedSheetType,
    MovableSheetType,
    TopSheetType
};

struct SHEET {
    VOID *buffer;
    UINT32 bx, by;
    UINT32 width, height;
    UINT32 flag;
    UINT32 type;
};

struct SHEET_INFO
{
    SHEET sheet[MAX_SHEET];
    UINT32 scrx, scry;
    UINT32 sheet_num;
    const FrameBufferConfig *fbc;
};

#endif
