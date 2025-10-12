#pragma once

#include <proto.hpp>

struct WINDOW_HANDLE
{
    WINDOWLS *WindowNodePtr;
};

struct XWINDOW
{
    uint32_t width;
    uint32_t height;
    char    *title;
    uint8_t  sets;
};

typedef  struct {
	char	    filename[256];
	uint64_t	length;
	uint64_t	filetype;
}  XAPIT_DirNode;



// XJ380API XAPI Edition
#define XAPI_OFFEST 380

// 一些隐藏的API，不对外开放，严禁泄露！
#define SXAH_SYSCALL_RETURN         3801
#define SXAH_CREATE_KERNEL_TERMINAL 3802
#define SXAH_CHECK_USER_PASSWORD    3803

// define
typedef struct {
    char            name[64];   // 用户名
    UserType        user_type;  // 用户等级
} xapi_type_UserInfo;

// P3.1
#define XAPI_OUTPUT    381
#define XAPI_INPUT     382
#define XAPI_GETLINE   418
#define XAPI_GETCH     383
#define XAPI_ENDLINE   384
#define XAPI_PRINTLINE 385
#define XAPI_PRINTF    386

// P3.2
#define XAPI_OPEN_FILE      387
#define XAPI_CLOSE_FILE     388
#define XAPI_SEARCH_FILE    416

// P3.4
#define XAPI_FORK   389
#define XAPI_EXECVE 390

// P3.5
#define XAPI_GET_VERSION        391
#define XAPI_GET_TIME           412
#define XAPI_GET_CURRENT_USER   413

// P4.1
#define XAPI_CREATE_WINDOW    392
#define XAPI_SET_WINDOW_TITLE 393
#define XAPI_CLOSE_WINDOW     394
#define XAPI_SET_ICON         395

// P4.2
#define XAPI_DRAW_POINT       396
#define XAPI_DRAW_LINE        397
#define XAPI_DRAW_RECT        398
#define XAPI_DRAW_RECT_FILL   399
#define XAPI_DRAW_CIRCLE      400
#define XAPI_DRAW_CIRCLE_FILL 401
#define XAPI_DRAW_TEXT        402
#define XAPI_DRAW_TEXT_L      414
#define XAPI_DRAW_TEXT_SW     415

// P4.3
#define XAPI_DRAW_BMP       403
#define XAPI_DRAW_PNG       404
#define XAPI_DRAW_PICTURE   419

// P4.4
#define XAPI_SET_MSH_PROCOR 405

#define MSG_CHAR    0
#define MSG_MOVE    1
#define MSG_LBUTTON 2
#define MSG_RBUTTON 3
#define MSG_MBUTTON 4
#define MSG_ROLLER  5
#define MSG_CRL     6

// P4.5
#define XAPI_READBUFFER     406
#define XAPI_WRITEBUFFER    407
#define XAPI_READBUFFERA    417
#define XAPI_WRITEBUFFERA   408
#define XAPI_REFRESH_WINDOW 409

// P4.6
#define XAPI_BUTTON       410
#define XAPI_BUTTON_STYLE 411

// 当前最大号：419

// Proto (XAPI Edition)
void do_xapi_SearchFile(uint64_t path, uint64_t count, uint64_t dir);
int do_xapi_CreateWindow(uint64_t handle, uint64_t xwin);
int do_xapi_SetWindowTitle(uint64_t handle, uint64_t str);
int do_xapi_CloseWindow(uint64_t handle);
int do_xapi_SetIcon(uint64_t handle, uint64_t path);
int do_xapi_DrawPoint(uint64_t handle, uint32_t x, uint32_t y, uint32_t color);
int do_xapi_DrawLine(uint64_t handle, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t color);
int do_xapi_DrawRect_fill(uint64_t handle, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t color);
int do_xapi_DrawRect(uint64_t handle, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t color);
// int do_xapi_DrawCircle_fill(uint64_t handle, uint32_t x, uint32_t y, uint32_t radius, uint32_t color);
// int do_xapi_DrawCircle(uint64_t handle, uint32_t x, uint32_t y, uint32_t radius, uint32_t color);
int do_xapi_DrawText(uint64_t handle, uint32_t x, uint32_t y, uint64_t str, uint32_t size, uint32_t color);
int do_xapi_DrawTextl(uint64_t handle, uint64_t place, uint64_t str, uint32_t size, uint32_t color, uint64_t i_width);
int do_xapi_DrawSWText(uint64_t handle, uint32_t x, uint32_t y, uint64_t str, uint32_t size, uint32_t color);
void do_xapi_ReadBuffer(uint64_t handle, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint64_t buffer);
void do_xapi_WriteBuffer(uint64_t handle, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint64_t buffer);
void do_xapi_ReadBufferA(uint64_t handle, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint64_t buffer);
void do_xapi_WriteBufferA(uint64_t handle, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint64_t buffer);
int do_xapi_RefreshWindow(uint64_t handle);
int do_xapi_SetMsgProc(uint64_t handle, uint64_t func);
void do_xapi_GetSystemVersion(uint64_t str);
uint64_t do_xapi_GetTime();
void do_xapi_GetCurrentUser(uint64_t dst);
void do_xapi_DrawBMP(uint64_t handle, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint64_t path);
void do_xapi_DrawPNG(uint64_t handle, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint64_t path);



void init_syscall();

// Proto (XAPI Edition)
void do_signal(uint64_t msg_type, uint64_t data1, uint64_t data2, MsgPrcor WinMpf, tcb_t ftask);
void init_signal();
void signal_revert();
void signal_end();
