#pragma once

#include <stdint.h>

#ifdef __cplusplus // Fuck you C++
extern "C" {
#endif

int    memcmp(const void *a_, const void *b_, size_t size);
void  *memcpy(void *dest, const void *src, size_t n);
void  *memset(void *dst, int val, size_t size);
char  *strcpy(char *dest, const char *src);
size_t strlen(const char *str);

typedef struct num_fmt_type
{
    bool zeropad : 1; // 补0
    bool sign    : 1; // 是否带符号 (用于区分signed和unsigned)
    bool plus    : 1; // 是否显示正号
    bool space   : 1; // 以空格代替正号
    bool left    : 1; // 左对齐
    bool special : 1; // 特殊前缀（比如：0x）
    bool small   : 1; // 使用小写字母代替大写字母（0X -> 0x, 1F -> 1f）
} num_fmt_type;

typedef struct num_formatter
{
    size_t num;       // 整数
    size_t base;      // 进制
    size_t size;      // 输出的字符串的最小大小
    size_t precision; // 精度（整数情况下约等于补0）
} num_formatter_t;

/**
 * 把字符串数字转成整数
 * @param pstr 字符串
 * @return 整数
 */
int atoi(const char *pstr);

/**
 * 跳过字符串中的数字，并返回连续的数字
 * @param s 指向字符串的指针（用于跳过）
 * @return 整数
 */
int skip_atoi(const char **s);

/**
 * 把整数转成字符串数字
 * @param str 字符串
 * @param fmter 整数格式化器
 * @param type 
 */
char *number(char *str, num_formatter_t fmter, num_fmt_type type);

/**
 * 计算函数 number() 返回的字符串数字的大小（用于提前分配好足够的空间）
 * @param fmter 整数格式化器
 * @param type 格式化类型
 * @return 数字格式化后的大小
 */
uint64_t number_length(num_formatter_t fmter, num_fmt_type type);

static inline bool are_interrupts_enabled()
{
    uint64_t rflags = 0;
    __asm__ volatile("pushfq\n\t"
                     "pop %0"
                     : "=r"(rflags));
    return (rflags & (1 << 9)) != 0;
}

#ifdef __cplusplus
} // extern "C"
#endif

#define PADDING_DOWN(size, to) ((size_t)(size) / (size_t)(to) * (size_t)(to))
#define PADDING_UP(size, to)   PADDING_DOWN((size_t)(size) + (size_t)(to) - (size_t)1, to)
#define IS_DIGIT(c)            ((c) >= '0' && (c) <= '9')
#define IS_ALPHA(a)            (((a) >= 'A' && (a) <= 'Z') || ((a) >= 'a' && (a) <= 'z'))