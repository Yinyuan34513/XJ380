/*
 *
 *      XINGJI XJ380 Application Compile Tools (XACT) 
 *      XACT C\C++ Header
 *      XJ380API Define Header
 *      Release Version 1.0.0 - 2025/5/9
 *      Copyright(C) XINGJI Interactive Software 2017 - 2025 All rights reserved.
 * 
 *      A XINGJI Interactive Software Production
 * 
 */

#ifndef _STDINT_H_
#define _STDINT_H_

#ifdef __cplusplus
extern "C"
{
#endif
#ifdef __cplusplus
}
#endif

// 规范类型
// 无符号整型
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
// 有符号整型
typedef char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long long int64_t;
// 浮点型
typedef float float32_t;
typedef double float64_t;

// efi\Windows风格
// 无符号整型
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
// 有符号整型
typedef int8_t INT8;
typedef int16_t INT16;
typedef int32_t INT32;
typedef int64_t INT64;
// 布尔类型
typedef char BOOLEAN;

// 自定义类型
// 无符号整型
typedef UINT8 u8;
typedef UINT16 u16;
typedef UINT32 u32;
typedef UINT64 u64;
// 有符号整型
typedef INT8 i8;
typedef INT16 i16;
typedef INT32 i32;
typedef INT64 i64;
// 字符
typedef const char CHAR8;
typedef char* WSTR;

#define NULL 0

typedef unsigned long int size_t; // compatible to ELF toolchain

#endif