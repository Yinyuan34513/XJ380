#include "include.h"

PUBLIC void *memcpy(void *dest, const void *src, int size)
{
    //assert(dest != NULL && src != NULL);
    u8 *dst = (u8 *) dest;
    u8 *source = (u8 *) src;
    while (size-- > 0) *dst++ = *source++;
    return dest;
}

PUBLIC void memset(void *dest, u8 value, u32 size)
{
    //assert(dest != NULL);
    u8 *dst = (u8 *) dest;
    while (size-- > 0) *dst++ = value;
}

PUBLIC char *strcpy(char *dest, const char *src)
{
    //assert(dest != NULL && src != NULL);
    char *r = dest;
    while ((*dest++ = *src++));
    return r;
}

PUBLIC u32 strlen(const char *str)
{
    //assert(str != NULL);
    const char *p = str;
    while (*p++);
    return p - str - 1;
}

PUBLIC u32 strcmp(const char *from_str, const char *cmp_str)
{
	//assert(from_str != NULL && cmp_str != NULL);
    const char *fs,*cs;
    while(*fs == *cs){
        if(*fs == '\0') return 0;
        fs++;
        cs++;
    }
	return *(u8*)fs - *(u8*)cs;
}
