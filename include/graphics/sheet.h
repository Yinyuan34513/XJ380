#ifndef _SHEET_H_
#define _SHEET_H_

#include <efi/fbc.h>
#include <graphics/GOP.hpp>
enum SystemSheetType
{
    FixedSheetType,
    MovableSheetType,
    NoEdgeWindowSheetType,
    TopWindowSheetType,
    TopSheetType
};

typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a; // 在TempBuffer里，这个没用
} SHEET_BUFFER;

typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} __attribute__((packed)) SHEET_BUFFER_WOA;

typedef struct
{
    uint8_t a;
    uint8_t r;
    uint8_t g;
    uint8_t b;
} SHEET_BUFFER_ARGB;

struct FoxLandPeek
{
    SHEET_BUFFER    *fl_peek_buffer;
    uint8_t         *peek_sign;
};

typedef struct SHEET *SHEETP;

struct SHEET
{
    SHEETP   front;
    void    *buffer;
    int      bx, by;
    uint32_t width, height;
    uint32_t type;
    uint16_t index;
    bool     is_change;
    SHEETP   next;
};

typedef struct
{
    SHEET                   *start;
    void                    *temp_buffer;
    uint32_t                 scrx, scry;
    uint16_t                 sheet_num; // 图层数量 - 1
    uint16_t                 sht_numed; // 图层数量 - 1，用于编号
    FoxLandPeek              foxland;
    const FrameBufferConfig *fbc;
} SHEET_INFO;

#include <graphics/window/window.h>
#include <task/pcb.h>

typedef struct TASK_DOCK_BLOCK *TDB_t;

struct TASK_DOCK_BLOCK
{
    char        path[256]; // icon path
    int         mcount;
    WINDOWLSP   windowls;
    bool        in_focus;
    TDB_t       next;
};


void      init_sheet(const FrameBufferConfig &fbc, SHEET_INFO *sht);
void      create_sheet(SHEET_INFO *sht, uint32_t bx, uint32_t by, uint32_t x, uint32_t y, uint32_t type, int16_t index,
                       uint16_t *sheet_id);
uint32_t  getBX(SHEET_INFO *sht, uint16_t *sheet_id);
uint32_t  getBY(SHEET_INFO *sht, uint16_t *sheet_id);
uint32_t  getXsize(SHEET_INFO *sht, uint16_t *sheet_id);
uint32_t  getYsize(SHEET_INFO *sht, uint16_t *sheet_id);
uint16_t *found_sheet(SHEET_INFO *sht, uint32_t x, uint32_t y);
uint16_t *found_sheetmb(SHEET_INFO *sht, uint32_t x, uint32_t y);
uint16_t *found_sheet_movable(SHEET_INFO *sht, uint32_t x, uint32_t y);
void      change_sheet_type(SHEET_INFO *sht, uint16_t *sheet_id, uint32_t new_type);
SHEET    *get_sheet(SHEET_INFO *sht, uint16_t *sheet_id);
void      setBX(SHEET_INFO *sht, uint16_t *sheet_id, uint32_t num);
void      setBY(SHEET_INFO *sht, uint16_t *sheet_id, uint32_t num);
void      lift_sheet(SHEET_INFO *sht, uint16_t *sheet_id);
void      delete_sheet(SHEET_INFO *sht, uint16_t *sheet_id);
void      refresh_sheet(SHEET_INFO *sht);
void      refresh_part_sheet(SHEET_INFO *sht, int bex1, int bey1, int bex2, int bey2);
uint32_t  get_hor();
uint32_t  get_ver();
void     *get_sheet_buffer(SHEET_INFO *sht, uint16_t *sheet_id);
SHEET *found_sheet_byid(SHEET_INFO *sht, uint16_t *sheet_id);

// graphics/draw.cpp
auto      operator new(size_t size, void *ptr) -> void *;
auto      operator new[](size_t size, void *ptr) -> void *;
auto      operator delete(void *ptr, size_t size) -> void;
auto      operator delete[](void *ptr, size_t size) -> void;
auto      operator delete(void *ptr) -> void;
void      rect(const FrameBufferConfig &fbc, int x1, int y1, int x2, int y2, const PixelColor &c);
void      dot(const FrameBufferConfig &fbc, int x, int y, const PixelColor &c);

// graphics/draw_sheet.cpp
void draw_point(SHEET_INFO *sht, uint16_t *sheet_id, int x, int y, const SHEET_BUFFER &color);
void draw_rect(SHEET_INFO *sht, uint16_t *sheet_id, int x1, int y1, int x2, int y2, const SHEET_BUFFER &color);
void draw_line(SHEET_INFO *sht, uint16_t *sheet_id, int x0, int y0, int x1, int y1, const SHEET_BUFFER &color);

// graphics/desktop.cpp
void init_desktop(SHEET_INFO *sht, uint16_t *sheet_id, uint16_t *dock_sheet_id);
void init_dock(SHEET_INFO *sht, uint16_t *sheet_id);
void init_shortcut_dock(SHEET_INFO *sht, uint16_t *sheet_id);
void draw_logo(SHEET_INFO *sht, uint16_t *sheet_id, int xi, int yi);
void draw_studio_logo(SHEET_INFO *sht, uint16_t *sheet_id, int xi, int yi);

void flush_task_dock();
void register_task_dock(WINDOWLSP window, tcb_t task);
void focus_window_dock(WINDOWLSP win);

#endif
