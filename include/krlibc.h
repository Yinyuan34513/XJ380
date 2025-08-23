#pragma once

#include <stdint.h>

#ifdef __cplusplus // Fuck you C++
extern "C" {
#endif
#define waitif(cond)                                                                               \
    ((void)({                                                                                      \
        while (cond) {}                                                                            \
    }))
static inline void empty() {};
#define ABS(x)    ((x) > 0 ? (x) : -(x))
#define MAX(x, y) ((x > y) ? (x) : (y))
#define MIN(x, y) ((x < y) ? (x) : (y))
#define streq(s1, s2)                                                                              \
    ({                                                                                             \
        const char *_s1 = (s1), *_s2 = (s2);                                                       \
        (_s1 && _s2) ? strcmp(_s1, _s2) == 0 : _s1 == _s2;                                         \
    })
#undef CHAR_BIT
#define CHAR_BIT __CHAR_BIT__

#ifndef MB_LEN_MAX
#    define MB_LEN_MAX 1
#endif

#undef SCHAR_MAX
#define SCHAR_MAX __SCHAR_MAX__
#undef SCHAR_MIN
#define SCHAR_MIN (-SCHAR_MAX - 1)

#undef UCHAR_MAX
#if __SCHAR_MAX__ == __INT_MAX__
#    define UCHAR_MAX (SCHAR_MAX * 2U + 1U)
#else
#    define UCHAR_MAX (SCHAR_MAX * 2 + 1)
#endif

#ifdef __CHAR_UNSIGNED__
#    undef CHAR_MAX
#    define CHAR_MAX UCHAR_MAX
#    undef CHAR_MIN
#    if __SCHAR_MAX__ == __INT_MAX__
#        define CHAR_MIN 0U
#    else
#        define CHAR_MIN 0
#    endif
#else
#    undef CHAR_MAX
#    define CHAR_MAX SCHAR_MAX
#    undef CHAR_MIN
#    define CHAR_MIN SCHAR_MIN
#endif

#undef SHRT_MAX
#define SHRT_MAX __SHRT_MAX__
#undef SHRT_MIN
#define SHRT_MIN (-SHRT_MAX - 1)

#undef USHRT_MAX
#if __SHRT_MAX__ == __INT_MAX__
#    define USHRT_MAX (SHRT_MAX * 2U + 1U)
#else
#    define USHRT_MAX (SHRT_MAX * 2 + 1)
#endif

#undef INT_MAX
#define INT_MAX __INT_MAX__
#undef INT_MIN
#define INT_MIN (-INT_MAX - 1)

#undef UINT_MAX
#define UINT_MAX (INT_MAX * 2U + 1U)

#undef LONG_MAX
#define LONG_MAX __LONG_MAX__
#undef LONG_MIN
#define LONG_MIN (-LONG_MAX - 1L)

#undef ULONG_MAX
#define ULONG_MAX (LONG_MAX * 2UL + 1UL)
int    memcmp(const void *a_, const void *b_, size_t size);
void  *memcpy(void *dest, const void *src, size_t n);
void  *memset(void *dst, int val, size_t size);
char  *strcpy(char *dest, const char *src);
size_t strlen(const char *str);
char *strcat(char *dest, const char *src);
char *strchrnul(const char *s, int c);
int strncmp(const char *s1, const char *s2, size_t n);
char *strchr(const char *s, int c);
int strcmp(const char *s1, const char *s2) ;
char *strcpy(char *dest, const char *src);
int isspace(int c);
char *strtok(char *str, const char *delim) ;
int64_t strtol(const char *str, char **endptr, int base);
char *strdup(const char *str) ;
char *pathacat(char *p1, char *p2);
int fls(unsigned int x) ;
int isdigit(int c);
char *strrchr(const char *s, int c) ;
char *strncpy(char *dest, const char *src, size_t n) ;

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