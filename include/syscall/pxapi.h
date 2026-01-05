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

typedef struct {
	uint64_t    length;
	void*	    buffer;
} XFILE;


// XJ380API XAPI Edition
#define XAPI_OFFEST 380

// 一些隐藏的API，不对外开放，严禁泄露！
#define SXAH_SYSCALL_RETURN         128956723895689201      // syscall返回 
#define SXAH_CREATE_KERNEL_TERMINAL 128956723895689202      // 创建内核态进程
#define SXAH_CHECK_USER_PASSWORD    128956723895689203      // 检查用户密码
#define SXAH_MARK_IS_TERMINAL       128956723895689204      // 标记为命令行
#define SXAH_WRITE_INPUT_BUFFER     128956723895689205      // 写入XTTTP输入缓冲区
#define SXAH_READ_OUTPUT_BUFFER     128956723895689206      // 读取XTTTP输出缓冲区
#define SXAH_CHECK_INPUT_BUFFER     128956723895689207      // 检查是否需要输入
#define SXAH_UNLOCK_OUTPUT_LOCK     128956723895689208      // 完成输出，关闭输出锁

#define SXAH_CHECK_TERMINAL_INIT_STATUS  3801    // 查询终端初始化状态

// define
typedef struct {
    char            name[64];   // 用户名
    UserType        user_type;  // 用户等级
} xapi_type_UserInfo;

// P3.1
#define XAPI_OUTPUT    381
#define XAPI_INPUT     382

#define XAPI_GETLINE   418

#define XAPI_GETCH          383
#define XAPI_ENDLINE        384
#define XAPI_PRINTLINE      385
#define XAPI_PRINTF         386
#define XAPI_OUTPUT_SERIAL  427

// P3.2
#define XAPI_OPEN_FILE      387
#define XAPI_CLOSE_FILE     388

#define XAPI_SEARCH_FILE    416

#define XAPI_MAKEDIR        425
#define XAPI_CREATE_FILE    420
#define XAPI_DELETE_FILE    421  
#define XAPI_RENAME_FILE    422
#define XAPI_READ_FILE      423
#define XAPI_WRITE_FILE     424

// P3.4
#define XAPI_FORK   389
#define XAPI_EXECVE 390

// P3.5
#define XAPI_GET_VERSION        391

#define XAPI_GET_TIME           412
#define XAPI_GET_CURRENT_USER   413

#define XAPI_GET_TIME_X         433
#define XAPI_GET_CPU_MODEL      434
#define XAPI_GET_MEMORY_SIZE    435

// P3.6
#define XAPI_BROKEN         428
#define XAPI_SEND_APP_MSG   429
#define XAPI_SLEEP          430

// P4.1
#define XAPI_CREATE_WINDOW    392
#define XAPI_SET_WINDOW_TITLE 393
#define XAPI_CLOSE_WINDOW     394
#define XAPI_SET_ICON         395

#define XAPI_GET_WIN_SZIE     426

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
#define XAPI_CALC_TEXT_WIDTH  431

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
#define MSG_SPCHAR  7

// P4.5
#define XAPI_READBUFFER     406
#define XAPI_WRITEBUFFER    407

#define XAPI_READBUFFERA    417

#define XAPI_WRITEBUFFERA   408
#define XAPI_REFRESH_WINDOW 409

// P4.6
#define XAPI_BUTTON         410
#define XAPI_BUTTON_EMP     411
#define XAPI_DELETE_BUTTON  432

#define XAPI_REG_RB_MENU    436
#define XAPI_URG_RB_MENU    437

// 4-1
#define XWIN_NORMAL				0
#define XWIN_FRAME_OFF			1
#define XWIN_FULL_SCR			2

// 当前最大号：437

// Proto (XAPI Edition)
void do_xapi_Output(char *str);
void do_xapi_Input(char *str);
char do_xapi_Getch();
void do_xapi_Endline();
void do_xapi_Printline(char *str);
void do_xapi_OutputSerial(char *str);
void do_xapi_SearchFile(uint64_t path, uint64_t count, uint64_t dir);
int do_xapi_CreateWindow(uint64_t handle, uint64_t xwin);
int do_xapi_SetWindowTitle(uint64_t handle, uint64_t str);
int do_xapi_CloseWindow(uint64_t handle);
int do_xapi_SetIcon(uint64_t handle, uint64_t path);
int do_xapi_GetWindowSize(uint64_t handle, uint64_t w, uint64_t h);
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
void do_xapi_DrawPicture(uint64_t handle, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint64_t path);
uint64_t do_xapi_ReadFile(struct X64_REGS *regs);
uint64_t do_xapi_WriteFile(struct X64_REGS *regs);
void do_xapi_Printf(char *str);
void do_xapi_Sleep(uint64_t ms);
void do_xapi_Button(uint64_t handle, uint64_t x, uint64_t y, uint64_t CRLid, uint64_t text);
void do_xapi_ButtonEmp(uint64_t handle, uint64_t x, uint64_t y, uint64_t CRLid, uint64_t text);
void do_xapi_DeleteButton(uint64_t handle, uint64_t CRLid);
uint64_t do_xapi_OpenFile(uint64_t path);
void do_xapi_CloseFile(uint64_t ptr);
void do_xapi_Broken(char *info);
void do_xapi_GetTimeX(uint64_t tt);
uint64_t do_xapi_GetMemorySize();
void do_xapi_RegisterRightButtonMenu(uint64_t handle, uint64_t items, uint64_t count);
void do_xapi_DeleteRightButtonMenu(uint64_t handle);



void init_syscall();

// Proto (XAPI Edition)
void do_message(uint64_t msg_type, uint64_t hData, uint64_t lData, MsgPrcor WinMpf, tcb_t ftask);
void init_message();
void message_revert();
void message_end();
