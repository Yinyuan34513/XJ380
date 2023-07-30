#include "include.h"

// 将一块地址中的内容复制至另一块地址
// *dest 目标起始地址
// *src 源起始地址
// size 内容长度

PUBLIC void *memcpy(void *dest, const void *src, int size)
{
    //assert(dest != NULL && src != NULL);
    u8 *dst = (u8 *) dest;
    u8 *source = (u8 *) src;
    while (size-- > 0) *dst++ = *source++;
    return dest;
}

// 将一段内存填充为一个数字
// *dest 填充起始地址
// value 填充数字
// size 填充长度

PUBLIC void memset(void *dest, u8 value, u32 size)
{
    //assert(dest != NULL);
    u8 *dst = (u8 *) dest;
    while (size-- > 0) *dst++ = value;
}

// 将一个c风格字符串复制至另一个c风格字符串
// *dest 目标字符串首地址
// *src 源字符串首地址

PUBLIC char *strcpy(char *dest, const char *src)
{
    //assert(dest != NULL && src != NULL);
    char *r = dest;
    while ((*dest++ = *src++));
    return r;
}

// 获取字符串的长度
// *str 源字符串的首地址

PUBLIC u32 strlen(const char *str)
{
    //assert(str != NULL);
    const char *p = str;
    while (*p++);
    return p - str - 1;
}
