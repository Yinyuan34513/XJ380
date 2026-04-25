#ifndef _WINDOW_H_
#define _WINDOW_H_

#include <task/pcb.h>

typedef struct WINDOWLS *WINDOWLSP;
typedef void (*MsgPrcor)(uint64_t Type, uint64_t hData,
                         uint64_t lData); // 别问为什么放在这儿 不放这儿会爆炸
                         
#include <graphics/sheet.h>
#include <graphics/components/rb_menu.h>

typedef struct WINDOWLS
{
    SHEET  *w_sheet;
    CHAR8     title[256];
    uint16_t  number;
    uint32_t  width;
    uint32_t  height;
    MsgPrcor  WinMPf;
    tcb_t     w_task;
    rb_menu_regt_p w_menu;
    WINDOWLSP next;
    uint64_t  type;
} WINDOWLS;

typedef struct WIN_THEME
{
    SHEET_BUFFER *top;
    SHEET_BUFFER *bottom;
    SHEET_BUFFER *left;
    SHEET_BUFFER *right;
    SHEET_BUFFER *close;
    SHEET_BUFFER *min;
    SHEET_BUFFER *close_light;
    SHEET_BUFFER *min_light;
} WIN_THEME;

typedef struct XWM_INFO
{
    uint16_t  win_num;
    WINDOWLS *start;
    WIN_THEME theme;
} XWM_INFO;

void init_xwm(XWM_INFO *xwmi);
void create_window(XWM_INFO *xwmi, SHEET_INFO *sht, WINDOWLS **windowls, CHAR8 *title, uint32_t width, uint32_t height);
void create_window_fmoff(XWM_INFO *xwmi, SHEET_INFO *sht, WINDOWLS **windowls, uint32_t width, uint32_t height);
void create_window_fscr(XWM_INFO *xwmi, SHEET_INFO *sht, WINDOWLS **windowls);
void change_title(XWM_INFO *xwmi, SHEET_INFO *sht, WINDOWLS *windowls, CHAR8 *title);
void draw_text(XWM_INFO *xwmi, SHEET_INFO *sht, WINDOWLS *windowls, CHAR8 *title, int x, int y, int size,
               SHEET_BUFFER color);
void draw_textl(XWM_INFO *xwmi, SHEET_INFO *sht, WINDOWLS *windowls, CHAR8 *title, int x, int y, int size,
                SHEET_BUFFER color, uint32_t *i_width);
void draw_text_sw(XWM_INFO *xwmi, SHEET_INFO *sht, WINDOWLS *windowls, CHAR8 *title, int x, int y, int size,
                  SHEET_BUFFER color);
void delete_window(XWM_INFO *xwmi, SHEET_INFO *sht, WINDOWLS *windowls);
void delete_process_windows(XWM_INFO *xwmi, SHEET_INFO *sht, pcb_t process);
WINDOWLS *sht_found_win(XWM_INFO *xwmi, SHEET_INFO *sht, SHEET *csheet);
WINDOWLS *thread_found_win(XWM_INFO *xwmi, tcb_t thread);
WINDOWLS *mpf_found_win(MsgPrcor mpf);

void resize_theme_width(SHEET_BUFFER *dst, SHEET_BUFFER *src, uint32_t width, uint32_t swidth, uint32_t height);
void resize_theme_height(SHEET_BUFFER *dst, SHEET_BUFFER *src, uint32_t width, uint32_t height, uint32_t sheight);
void resize_theme_unheight(SHEET_BUFFER *dst, SHEET_BUFFER *src, uint32_t width, uint32_t height, uint32_t sheight);

#endif
