#ifndef _SHEET_H_
#define _SHEET_H_

#include <efi/fbc.h>
#include <graphics/GOP.hpp>
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
void    init_sheet(const FrameBufferConfig &fbc, SHEET_INFO *sht);
void    create_sheet(SHEET_INFO *sht, UINT32 bx, UINT32 by, UINT32 x, UINT32 y, UINT32 type, INT16 index,
                     UINT16 *sheet_id);
UINT32  getBX(SHEET_INFO *sht, UINT16 *sheet_id);
UINT32  getBY(SHEET_INFO *sht, UINT16 *sheet_id);
UINT32  getXsize(SHEET_INFO *sht, UINT16 *sheet_id);
UINT32  getYsize(SHEET_INFO *sht, UINT16 *sheet_id);
UINT16 *found_sheet(SHEET_INFO *sht, UINT32 x, UINT32 y);
UINT16 *found_sheetmb(SHEET_INFO *sht, UINT32 x, UINT32 y);
UINT16 *found_sheet_movable(SHEET_INFO *sht, UINT32 x, UINT32 y);
SHEET  *get_sheet(SHEET_INFO *sht, UINT16 *sheet_id);
void    setBX(SHEET_INFO *sht, UINT16 *sheet_id, UINT32 num);
void    setBY(SHEET_INFO *sht, UINT16 *sheet_id, UINT32 num);
void    lift_sheet(SHEET_INFO *sht, UINT16 *sheet_id);
void    delete_sheet(SHEET_INFO *sht, UINT16 *sheet_id);
void    refresh_sheet(SHEET_INFO *sht);
void    refresh_part_sheet(SHEET_INFO *sht, int bex1, int bey1, int bex2, int bey2);
UINT32  get_hor();
UINT32  get_ver();
// graphics/draw.cpp
auto operator new(size_t size, void *ptr) -> void *;
auto operator new[](size_t size, void *ptr) -> void *;
auto operator delete(void *ptr, size_t size) -> void;
auto operator delete[](void *ptr, size_t size) -> void;
auto operator delete(void *ptr) -> void;
void rect(const FrameBufferConfig &fbc, int x1, int y1, int x2, int y2, const PixelColor &c);
void dot(const FrameBufferConfig &fbc, int x, int y, const PixelColor &c);

// graphics/draw_sheet.cpp
void draw_point(SHEET_INFO *sht, UINT16 *sheet_id, int x, int y, const SHEET_BUFFER &color);
void draw_rect(SHEET_INFO *sht, UINT16 *sheet_id, int x1, int y1, int x2, int y2, const SHEET_BUFFER &color);
void draw_line(SHEET_INFO *sht, UINT16 *sheet_id, int x0, int y0, int x1, int y1, const SHEET_BUFFER &color);

// graphics/desktop.cpp
void init_desktop(SHEET_INFO *sht, UINT16 *sheet_id, UINT16 *dock_sheet_id);
void init_dock(SHEET_INFO *sht, UINT16 *sheet_id);
void init_shortcut_dock(SHEET_INFO *sht, UINT16 *sheet_id);
void draw_logo(SHEET_INFO *sht, UINT16 *sheet_id, int xi, int yi);
void draw_studio_logo(SHEET_INFO *sht, UINT16 *sheet_id, int xi, int yi);
#endif
