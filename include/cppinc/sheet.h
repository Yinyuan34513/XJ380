#ifndef _SHEET_H_
#define _SHEET_H_

#include "../include.h"

#define MAX_SHEET       256

typedef struct SHEET {
    UINT64 *buffer;
    UINT32 bx, by;
    UINT32 width, height;
};

typedef struct SHEET_INFO
{
    SHEET sheet[MAX_SHEET];
    UINT32 scrx, scry;
    UINT32 sheet_num;
    FrameBufferConfig *fbc;
};

#endif
