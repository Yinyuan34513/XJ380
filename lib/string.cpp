#ifdef __cplusplus
extern "C"
{
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
    // assert(dest != NULL && src != NULL);
    char *r = dest;
    while ((*dest++ = *src++))
        ;
    return r;
}

UINT64 strlen(const char *str)
{
    // assert(str != NULL);
    const char *p = str;
    while (*p++)
        ;
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

char *Hex2Char(unsigned long long hex)
{
    // 输出16进制
    char buf[32];
    char *re = buf;
    char *p = buf;
    char ch;
    int i, flag = 0;

    *p++ = '0';
    *p++ = 'x'; // 先存一个0x

    if (hex == 0)
        *p++ = '0'; // 如果是0，直接0x0趋势
    else
    {
        for (i = 60; i >= 0; i -= 4)
        {                          // 每次4位
            ch = (hex >> i) & 0xF; // 0~9, A~F
            // 28（冗余）
            if (flag || ch > 0)
            {              // 跳过前导0
                flag = 1;  // 没有前导0就把flag设为1，这样后面再有0也不会忽略
                ch += '0'; // 0~9 => '0'~'9'
                if (ch > '9')
                {
                    ch += 7; // 'A' - '9' = 7
                }
                *p++ = ch; // 写入
            }
        }
    }
    *p = '\0'; // 结束符
    return re;
}

char *Dec2Char(unsigned long long dec)
{
    char str[255];
    char *re = str;
    int radix = 10;
    char index[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"; // 索引表
    unsigned unum;                                         // 存放要转换的整数的绝对值,转换的整数可能是负数
    int i = 0, j, k;                                       // i用来指示设置字符串相应位，转换之后i其实就是字符串的长度；转换后顺序是逆序的，有正负的情况，k用来指示调整顺序的开始位置;j用来指示调整顺序时的交换。

    // 获取要转换的整数的绝对值
    if (radix == 10 && dec < 0) // 要转换成十进制数并且是负数
    {
        unum = (unsigned)-dec; // 将num的绝对值赋给unum
        str[i++] = '-';        // 在字符串最前面设置为'-'号，并且索引加1
    }
    else
        unum = (unsigned)dec; // 若是num为正，直接赋值给unum

    // 转换部分，注意转换后是逆序的
    do
    {
        str[i++] = index[unum % (unsigned)radix]; // 取unum的最后一位，并设置为str对应位，指示索引加1
        unum /= radix;                            // unum去掉最后一位

    } while (unum); // 直至unum为0退出循环

    str[i] = '\0'; // 在字符串最后添加'\0'字符，c语言字符串以'\0'结束。

    // 将顺序调整过来
    if (str[0] == '-')
        k = 1; // 如果是负数，符号不用调整，从符号后面开始调整
    else
        k = 0; // 不是负数，全部都要调整

    char temp;                         // 临时变量，交换两个值时用到
    for (j = k; j <= (i - 1) / 2; j++) // 头尾一一对称交换，i其实就是字符串的长度，索引最大值比长度少1
    {
        temp = str[j];               // 头部赋值给临时变量
        str[j] = str[i - 1 + k - j]; // 尾部赋值给头部
        str[i - 1 + k - j] = temp;   // 将临时变量的值(其实就是之前的头部值)赋给尾部
    }
    return re;
}
