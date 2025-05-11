/*
 *
 *      XINGJI XJ380 Application Compile Tools (XACT) 
 *      XACT C\C++ Header
 *      XJ380API XAPI Edition GUI Part
 *      Release Version 1.0.0 - 2025/5/10
 *      Copyright(C) XINGJI Interactive Software 2017 - 2025 All rights reserved.
 * 
 *      A XINGJI Interactive Software Production
 * 
 */

#pragma once

#include "stdint.h"

// define
typedef UINT64 		HDLE;
typedef void 		(*MsgPrcor)();

struct XWINDOW
{
    UINT32 	width;
    UINT32 	height;
    WSTR 	title;
    UINT8 	sets;
};

struct XCOLOR
{
	UINT8 	Red;
	UINT8 	Green;
	UINT8 	Blue;
};

struct XCOLORA
{
	UINT8 	Red;
	UINT8 	Green;
	UINT8 	Blue;
	UINT8	Alpha;
};


// 4-1
void  xapi_CreateWindow(
	HDLE*		handle,
	XWINDOW* 	xwin
);

void  xapi_SetWindowTitle(
	HDLE  	handle,
	WSTR  	str
);

void  xapi_CloseWindow(
    HDLE  	handle
);

void  xapi_SetIcon(
	HDLE  	handle,
	WSTR  	path
);

// 4-2
void  xapi_DrawPoint (
	HDLE  	handle,
	UINT32  x,
	UINT32  y,
	UINT32  color
);

void  xapi_DrawLine(
    HDLE  	handle,
	UINT32 	x1,
	UINT32 	y1,
	UINT32 	x2,
	UINT32 	y2,
	UINT32 	color
);

void  xapi_DrawRect(
	HDLE 	handle,
	UINT32 	x1,
	UINT32  y1,
	UINT32  x2,
	UINT32 	y2,
	UINT32 	color,
	bool  	fill
);

void  xapi_DrawCircle(
	HDLE  	handle,
	UINT32 	x1,
	UINT32 	y1,
	UINT32 	x2,
	UINT32 	y2,
	UINT32 	color,
	bool  	fill
);

void  xapi_DrawText(
	HDLE  	handle,
	WSTR 	str,
	UINT32  color
);

// 4-3
void  xapi_DrawBMP (
	HDLE  	handle,
	UINT32  x,
	UINT32  y,
	UINT32	width,
	UINT32	height,
	WSTR  	path
);

void  xapi_DrawPNG(
	HDLE  	handle,
	UINT32  x,
	UINT32  y,
	UINT32	width,
	UINT32	height,
	WSTR  	path
);

// 4-4
void SetMsgPrcor(MsgPrcor func);

// 4-5
void  xapi_ReadBuffer(
	HDLE  	 	handle,
	UINT32  	x,
	UINT32  	y,
	UINT32  	width,
	UINT32		height,
	XCOLOR*  	buffer
);

void xapi_WriteBuffer(
	HDLE  	 	handle,
	UINT32  	x,
	UINT32  	y,
	UINT32  	width,
	UINT32  	height,
	XCOLOR*  	buffer
);

void xapi_WriteBufferA(
	HDLE  	 	handle,
	UINT32  	x,
	UINT32  	y,
	UINT32  	width,
	UINT32  	height,
	XCOLORA*  	buffer
);

// 4-6
void xapi_Button(
	HDLE  	handle,
	UINT64  CRLid,
	WSTR  	text
);

void xapi_StyleButton(
	HDLE  	handle,
	UINT64  CRLid,
	WSTR  	text,
	WSTR  	path
);
