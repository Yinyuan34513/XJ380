#ifndef _WINDOW_H_
#define _WINDOW_H_

#include <graphics/sheet.h>
#include <task/pcb.h>

typedef struct WINDOWLS *WINDOWLSP;
typedef void (*MsgPrcor)(uint64_t Type, uint64_t hData,
                         uint64_t lData); // 别问为什么放在这儿 不放这儿会爆炸

typedef struct WINDOWLS
{
    uint16_t  w_sheet;
    uint8_t   title_length;
    CHAR8    *title;
    uint16_t  number;
    uint32_t  width;
    uint32_t  height;
    MsgPrcor  WinMPf;
    tcb_t     w_task;
    WINDOWLSP next;
} WINDOWLS;

typedef struct XWM_INFO
{
    uint16_t  win_num;
    WINDOWLS *start;
} XWM_INFO;

void init_xwm(XWM_INFO *xwmi);
void create_window(XWM_INFO *xwmi, SHEET_INFO *sht, WINDOWLS **windowls, CHAR8 *title, uint32_t width, uint32_t height);
void change_title(XWM_INFO *xwmi, SHEET_INFO *sht, WINDOWLS *windowls, CHAR8 *title);
void draw_text(XWM_INFO *xwmi, SHEET_INFO *sht, WINDOWLS *windowls, CHAR8 *title, int x, int y, int size,
               SHEET_BUFFER color);
void delete_window(XWM_INFO *xwmi, SHEET_INFO *sht, WINDOWLS *windowls);
WINDOWLS *sht_found_win(XWM_INFO *xwmi, SHEET_INFO *sht, SHEET *csheet);

#endif
