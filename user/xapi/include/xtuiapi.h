/*
 *
 *      XINGJI XJ380 Application Compile Tools (XACT) 
 *      XACT C\C++ Header
 *      XJ380API XAPI Edition TUI Part
 *      Release Version 1.0.0 - 2025/5/10
 *      Copyright(C) XINGJI Interactive Software 2017 - 2025 All rights reserved.
 * 
 *      A XINGJI Interactive Software Production
 * 
 */

#pragma once

#include "stdint.h"

// define
typedef enum {
    XUT_Root,
    XUT_System,
    XUT_Admin,
    XUT_Visitor,
    XUT_Custom
} UserType;

typedef struct {
	UINT64	length;
	void*	buffer;
} XFILE;

typedef struct {
    char            name[64];   // 用户名
    UserType        user_type;  // 用户等级
} UserInfo;

typedef struct {
	char	 filename[256];
	UINT64	 length;
	UINT64	 filetype;
}  DirNode;

// 3-1
void xapi_Output(WSTR str);
void xapi_Input(WSTR str);
char xapi_Getch();
void xapi_EndLine();
void xapi_PrintLine(WSTR str);
void xapi_Printf(WSTR fmt, ...);
void xapi_OutputSerial(WSTR str);

// 3-2
XFILE *xapi_OpenFile(
	WSTR	path
);

void xapi_CloseFile(XFILE* fsptr); 

void  xapi_SearchFile(
	WSTR	 path,
	UINT32	*count,
	DirNode	*dir
);

UINT64 xapi_Mkdir(WSTR path);
UINT64 xapi_CreateFile(WSTR filename);
UINT64 xapi_DeleteFile(WSTR path);
UINT64 xapi_RenameFile(WSTR old_path, WSTR new_path);
UINT64 xapi_ReadFile(WSTR filename, char *buffer, UINT64 size, UINT64 offset);
UINT64 xapi_WriteFile(WSTR filename, char *buffer, UINT64 size, UINT64 offset);

// 3-3
UINT64 	xcr_char2int(WSTR str);
WSTR 	xcr_int2char(UINT64 dec); 
WSTR 	xcr_hex2char(UINT64  hex);
UINT32 	toRGB(UINT8 r, UINT8 g, UINT8 b);
UINT32 	toRGBA(UINT32 color);

// 3-4
UINT64  xapi_Fork();
UINT64  xapi_Execve(WSTR filename, WSTR argv[], WSTR envp[]);

// 3-5
void	xapi_GetSystemVersion(WSTR version);
UINT64	xapi_GetTime();
void	xapi_GetCurrentUser(UserInfo *user_info);

// 3-6
void	xapi_Broken(WSTR broken_info);
void	xapi_SendAppMessage(WSTR title, WSTR text);
void 	xapi_Sleep(UINT64 ms);
