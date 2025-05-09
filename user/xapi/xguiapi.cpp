#include "./include/xguiapi.h"
#include "./include/libsys.h"

// 4-1
void xapi_CreateWindow(HDLE	handle, XWINDOW* xwin) {
	enter_syscall(XAPI_CREATE_WINDOW, handle, (UINT64)(xwin), 0, 0, 0, 0);
}

void xapi_SetWindowTitle(HDLE handle, WSTR str) {
	enter_syscall(XAPI_SET_WINDOW_TITLE, handle, (UINT64)(str), 0, 0, 0, 0);
}

void xapi_CloseWindow(HDLE handle) {
	enter_syscall(XAPI_CLOSE_WINDOW, handle, 0, 0, 0, 0, 0);
}

void xapi_SetIcon(HDLE handle, WSTR path) {
	enter_syscall(XAPI_SET_ICON, handle, (UINT64)(path), 0, 0, 0, 0);
}

// 4-2
void xapi_DrawPoint(HDLE handle, UINT32 x, UINT32 y, UINT32 color) {
	enter_syscall(XAPI_DRAW_POINT, handle, x, y, color, 0, 0);
}

void xapi_DrawLine(
    HDLE  	handle,
	UINT32 	x1,
	UINT32 	y1,
	UINT32 	x2,
	UINT32 	y2,
	UINT32 	color
);

void xapi_DrawRect(
	HDLE 	handle,
	UINT32 	x1,
	UINT32  y1,
	UINT32  x2,
	UINT32 	y2,
	UINT32 	color,
	bool  	fill
);

void xapi_DrawCircle(
	HDLE  	handle,
	UINT32 	x1,
	UINT32 	y1,
	UINT32 	x2,
	UINT32 	y2,
	UINT32 	color,
	bool  	fill
);

void xapi_DrawText(
	HDLE  	handle,
	WSTR 	str,
	UINT32  color
);

// 4-3
void xapi_DrawBMP (
	HDLE  	handle,
	UINT32 	x,
	UINT32 	y,
	WSTR    path
);

void xapi_DrawPNG(
	HDLE  	handle,
	UINT32  x,
	UINT32  y,
	WSTR  	path
);

// 4-4
void SetMsgPrcor(MsgPrcor func);

// 4-5
void xapi_ReadBuffer(
	HDLE  	 	handle,
	UINT32  	x,
	UINT32  	y,
	UINT32  	width,
	UINT32		height,
	XCOLOR*  	buffer
);


