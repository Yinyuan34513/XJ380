#ifdef __cplusplus
extern "C" {
#endif
#include "../include/efi.h"
#ifdef __cplusplus
}
#endif
#include "../include/global/stdint.h"

char *strncpy(char *dest, const char *src, UINT64 length)
{
    char *r = dest;
    for (int i = 0; i < length; i++)
        *dest++ = *src++;
    return r;
}
char *strcpy(char *dest, const char *src)
{
    //assert(dest != NULL && src != NULL);
    char *r = dest;
    while ((*dest++ = *src++));
    return r;
}

UINT64 strlen(const char *str)
{
    //assert(str != NULL);
    const char *p = str;
    while (*p++);
    return p - str - 1;
}

UINT64 strcmp(char *from_str, char *cmp_str)
{
	while ((*from_str != '\0') && (*from_str == *cmp_str))
	{
		from_str++;
		cmp_str++;
	}
	return *from_str - *cmp_str;
}

UINT64 part_strcmp(char *from_str, char *cmp_str, UINT64 size)
{
   char temp[32], from[32];
   for (UINT64 i = 0; i < size; i++)
   {
       temp[i] = from_str[i];
       from[i] = cmp_str[i];
   }
   temp[size] = '\0';
   from[size] = '\0';
   UINT64 ret;
   ret = strcmp(from, temp);
   return ret;
}
