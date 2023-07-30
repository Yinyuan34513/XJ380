#include "include.h"

// 将一个数字转换成16进制
// *str 目标字符串首地址
// num 源数字

PRIVATE char *tohex(char *str, int num)
{
    char *p = str;
    char ch;
    int i, flag = 0;

    *p++ = '0';
    *p++ = 'x';

    if (num == 0) *p++ = '0';
    else {
        for (i = 28; i >= 0; i -= 4) {
            ch = (num >> i) & 0xf;
            if (flag || (ch > 0)) {
                flag = 1;
                ch += '0';
                if (ch > '9') ch += 7;
                *p++ = ch;
            }
        }
    }

    *p = 0;
    return str;
}

// 将一个数字转换成16进制并输出
// i 源数字

PUBLIC void put_int(int i)
{
    char str[16];
    tohex(str, i);
    put_str(str);
}
