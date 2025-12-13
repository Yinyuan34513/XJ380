#include "./include/xguiapi.h"
#include "./include/libsys.h"

// 4-1
void xapi_CreateWindow(HDLE* handle, XWINDOW* xwin) {
	enter_syscall(XAPI_CREATE_WINDOW, (UINT64)(handle), (UINT64)(xwin), 0, 0, 0, 0);
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

void xapi_GetWindowSize(HDLE handle, UINT64* width, UINT64* height) {
	enter_syscall(XAPI_GET_WIN_SZIE, handle, (UINT64)(width), (UINT64)(height), 0, 0, 0);
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
) {
	enter_syscall(XAPI_DRAW_LINE, handle, x1, y1, x2, y2, color);
}

void xapi_DrawRect(
	HDLE 	handle,
	UINT32 	x1,
	UINT32  y1,
	UINT32  x2,
	UINT32 	y2,
	UINT32 	color,
	bool  	fill
) {
	if (fill) {
		enter_syscall(XAPI_DRAW_RECT_FILL, handle, x1, y1, x2, y2, color);
	} else {
		enter_syscall(XAPI_DRAW_RECT, handle, x1, y1, x2, y2, color);
	}
}

// void xapi_DrawCircle(
// 	HDLE  	handle,
// 	UINT32 	x,
// 	UINT32 	y,
// 	UINT32 	radius,
// 	UINT32 	color,
// 	bool  	fill
// ) {
// 	if (fill) {
// 		enter_syscall(XAPI_DRAW_CIRCLE_FILL, handle, x, y, radius, color, 0);
// 	} else {
// 		enter_syscall(XAPI_DRAW_CIRCLE, handle, x, y, radius, color, 0);
// 	}
// }

void xapi_DrawText(
	HDLE  	handle,
	UINT32 	x,
	UINT32 	y,
	WSTR 	str,
	UINT32	size,
	UINT32  color
) {
	enter_syscall(XAPI_DRAW_TEXT, handle, x, y, (UINT64)(str), size, color);
}

void xapi_DrawTextl(
	HDLE  	handle,
	UINT32 	x,
	UINT32 	y,
	WSTR 	str,
	UINT32	size,
	UINT32  color,
	UINT32 *width
) {
	UINT64 tx = x;
	enter_syscall(XAPI_DRAW_TEXT_L, handle, (tx << 32) | y, (UINT64)(str), size, color, (UINT64)width);
}

void xapi_DrawSWText(
	HDLE  	handle,
	UINT32 	x,
	UINT32 	y,
	WSTR 	str,
	UINT32  color
) {
	enter_syscall(XAPI_DRAW_TEXT_SW, handle, x, y - 2, (UINT64)(str), 10, color);
}


// 4-3
void xapi_DrawBMP (
	HDLE  	handle,
	UINT32 	x,
	UINT32 	y,
	UINT32	width,
	UINT32	height,
	WSTR    path
) {
	enter_syscall(XAPI_DRAW_BMP, handle, x, y, width, height, (UINT64)(path));
}

void xapi_DrawPNG(
	HDLE  	handle,
	UINT32  x,
	UINT32  y,
	UINT32	width,
	UINT32	height,
	WSTR  	path
) {
	enter_syscall(XAPI_DRAW_PNG, handle, x, y, width, height, (UINT64)(path));
}

void xapi_DrawPicture(
	HDLE  	handle,
	UINT32  x,
	UINT32  y,
	UINT32	width,
	UINT32	height,
	WSTR  	path
) {
	enter_syscall(XAPI_DRAW_PICTURE, handle, x, y, width, height, (UINT64)(path));
}

// 4-4
void SetMsgPrcor(HDLE handle, MsgPrcor func) {
	enter_syscall(XAPI_SET_MSH_PROCOR, handle, (UINT64)(func), 0, 0, 0, 0);
}

// 4-5
void xapi_ReadBuffer(
	HDLE  	 	handle,
	UINT32  	x,
	UINT32  	y,
	UINT32  	width,
	UINT32		height,
	XCOLOR*  	buffer
) {
	enter_syscall(XAPI_READBUFFER, handle, x, y, width, height, (UINT64)(buffer));
}

void xapi_WriteBuffer(
	HDLE  	 	handle,
	UINT32  	x,
	UINT32  	y,
	UINT32  	width,
	UINT32  	height,
	XCOLOR*  	buffer
) {
	enter_syscall(XAPI_WRITEBUFFER, handle, x, y, width, height, (UINT64)(buffer));
}

void xapi_ReadBufferA(
	HDLE  	 	handle,
	UINT32  	x,
	UINT32  	y,
	UINT32  	width,
	UINT32		height,
	XCOLOR*  	buffer
) {
	enter_syscall(XAPI_READBUFFERA, handle, x, y, width, height, (UINT64)(buffer));
}

void xapi_WriteBufferA(
	HDLE  	 	handle,
	UINT32  	x,
	UINT32  	y,
	UINT32  	width,
	UINT32  	height,
	XCOLORA*  	buffer
) {
	enter_syscall(XAPI_WRITEBUFFERA, handle, x, y, width, height, (UINT64)(buffer));
}

void xapi_RefreshWindow(HDLE handle) {
	enter_syscall(XAPI_REFRESH_WINDOW, handle, 0, 0, 0, 0, 0);
}

// 4-6
void xapi_Button(
	HDLE  	handle,
	UINT64  CRLid,
	WSTR  	text
) {
	enter_syscall(XAPI_WRITEBUFFERA, handle, CRLid, (UINT64)(text), 0, 0, 0);
}

void xapi_StyleButton(
	HDLE  	handle,
	UINT64  CRLid,
	WSTR  	text,
	WSTR  	path
) {
	enter_syscall(XAPI_WRITEBUFFERA, handle, CRLid, (UINT64)(text), (UINT64)(path), 0, 0);
}
