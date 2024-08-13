// 版权所有©XINGJI Studios 2017-2024 保留所有权利。
// XJ380 stdint头文件
#ifndef _STDINT_H_
#define _STDINT_H_

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

#ifndef _EFI_H_
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef unsigned long int size_t; // compatible to ELF toolchain

typedef __UINT8_TYPE__ UINT8;
typedef __UINT16_TYPE__ UINT16;
typedef __UINT32_TYPE__ UINT32;
typedef unsigned char UINT8;
typedef unsigned short UINT16;
typedef unsigned int UINT32;
typedef unsigned long long UINT64;
typedef UINT64 UINTN; // 我们在64位下
typedef UINTN EFI_STATUS;
typedef void VOID;
typedef UINT16 CHAR16;
typedef UINTN EFI_PHYSICAL_ADDRESS;
typedef UINTN EFI_VIRTUAL_ADDRESS;
typedef VOID *EFI_HANDLE;
typedef VOID *EFI_EVENT;

#endif

#endif