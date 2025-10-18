#include "./include/stdint.h"
#include "./include/krlibc.h"
#include "./include/xposix/stdarg.h"
int memcmp(const void *a_, const void *b_, size_t size)
{
    const char *a = (const char *)(a_);
    const char *b = (const char *)(b_);
    while (size-- > 0)
    {
        if (*a != *b) return *a > *b ? 1 : -1;
        a++, b++;
    }
    return 0;
}

void *memcpy(void *dest, const void *src, size_t n)
{
    char       *d = (char *)(dest);
    const char *s = (const char *)(src);

    void *ret = dest;

    if (n < 8)
    {
        while (n--)
        {
            *d++ = *s++;
        }
        return ret;
    }

    size_t align = (size_t)d & (sizeof(size_t) - 1);
    if (align)
    {
        align  = sizeof(size_t) - align;
        n     -= align;
        while (align--)
        {
            *d++ = *s++;
        }
    }

    size_t       *dw = (size_t *)d;
    const size_t *sw = (const size_t *)s;
    for (size_t i = 0; i < n / sizeof(size_t); i++)
    {
        *dw++ = *sw++;
    }

    d             = (char *)dw;
    s             = (const char *)sw;
    size_t remain = n & (sizeof(size_t) - 1);
    while (remain--)
    {
        *d++ = *s++;
    }

    return ret;
}

void *memset(void *dst, int val, size_t size)
{
    unsigned char *d = (unsigned char *)(dst);
    unsigned char  v = (unsigned char)(val);

    while (size && ((size_t)d & 7))
    {
        *d++ = v;
        size--;
    }

    size_t v8 = v * 0x0101010101010101ULL;
    while (size >= 8)
    {
        *(size_t *)d  = v8;
        d            += 8;
        size         -= 8;
    }

    while (size--)
        *d++ = v;

    return dst;
}

char *strcpy(char *dest, const char *src)
{
    do
    {
        *dest++ = *src++;
    } while (*src != 0);
    *dest = 0;
    return dest;
}

size_t strlen(const char *str)
{
    const char *s = str;
    while (*s)
    {
        s++;
    }
    return s - str;
}

/**
 * 把字符串数字转成整数
 * @param pstr 字符串
 * @return 整数
 */
int atoi(const char *pstr)
{
    int ret_integer  = 0;
    int integer_sign = 1;

    if (*pstr == '-') integer_sign = -1;
    if (*pstr == '-' || *pstr == '+') pstr++;

    while (*pstr >= '0' && *pstr <= '9')
    {
        ret_integer = ret_integer * 10 + *pstr - '0';
        pstr++;
    }
    ret_integer = integer_sign * ret_integer;
    return ret_integer;
}

char *strcat(char *dest, const char *src)
{
    char *ret = dest;
    while (*dest)
        dest++;
    while ((*dest++ = *src++))
        ;
    return ret;
}

char *strchrnul(const char *s, int c)
{
    while (*s)
    {
        if ((*s++) == c) break;
    }
    return (char *)s;
}

int strncmp(const char *s1, const char *s2, size_t n)
{
    const unsigned char *p1 = (const unsigned char *)s1, *p2 = (const unsigned char *)s2;
    while (n-- > 0)
    {
        if (*p1 != *p2) return *p1 - *p2;
        if (*p1 == '\0') return 0;
        p1++, p2++;
    }
    return 0;
}

char *strchr(const char *s, int c)
{
    while (*s)
    {
        if (*s == (char)c) { return (char *)s; }
        s++;
    }
    return (*s == (char)c) ? (char *)s : NULL;
}

char *strstr(const char *str1, const char *str2)
{
    const char *s1, *s2, *p = str1;
    while (*p != '\0')
    {
        s1 = p;
        s2 = str2;
        while (*s1 != '\0' && *s2 != '\0' && *s1 == *s2)
        {
            s1++;
            s2++;
        }
        if (*s2 == '\0') { return (char *)p; }
        p++;
    }
    return NULL;
}

int strcmp(const char *s1, const char *s2)
{
    char is_equal = 1;

    for (; (*s1 != '\0') && (*s2 != '\0'); s1++, s2++)
    {
        if (*s1 != *s2)
        {
            is_equal = 0;
            break;
        }
    }

    if (is_equal)
    {
        if (*s1 != '\0') { return 1; }
        else if (*s2 != '\0') { return -1; }
        else { return 0; }
    }
    else { return (int)(*s1 - *s2); }
}

int isspace(int c)
{
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v');
}

char *strtok(char *str, const char *delim)
{
    static char *last = NULL;
    if (str) { last = str; }
    else if (!last) { return NULL; }

    char *start = last;
    while (*start && strchr(delim, *start))
    {
        start++;
    }

    if (*start == '\0')
    {
        last = NULL;
        return NULL;
    }

    char *end = start;
    while (*end && !strchr(delim, *end))
    {
        end++;
    }

    if (*end)
    {
        *end = '\0';
        last = end + 1;
    }
    else { last = NULL; }

    return start;
}

int64_t strtol(const char *str, char **endptr, int base)
{
    const char *s;
    uint64_t    acc;
    char        c;
    uint64_t    cutoff;
    uint64_t    neg, any, cutlim;
    s = str;
    do
    {
        c = *s++;
    } while (isspace((unsigned char)c));
    if (c == '-')
    {
        neg = 1;
        c   = *s++;
    }
    else
    {
        neg = 0;
        if (c == '+') c = *s++;
    }
    if ((base == 0 || base == 16) && c == '0' && (*s == 'x' || *s == 'X') &&
        ((s[1] >= '0' && s[1] <= '9') || (s[1] >= 'A' && s[1] <= 'F') || (s[1] >= 'a' && s[1] <= 'f')))
    {
        c     = s[1];
        s    += 2;
        base  = 16;
    }
    if (base == 0) base = c == '0' ? 8 : 10;
    acc = any = 0;
    if (base < 2 || base > 36) goto noconv;

    cutoff  = neg ? (unsigned long)-(LONG_MIN + LONG_MAX) + LONG_MAX : LONG_MAX;
    cutlim  = cutoff % base;
    cutoff /= base;
    for (;; c = *s++)
    {
        if (c >= '0' && c <= '9')
            c -= '0';
        else if (c >= 'A' && c <= 'Z')
            c -= 'A' - 10;
        else if (c >= 'a' && c <= 'z')
            c -= 'a' - 10;
        else
            break;
        if (c >= base) break;
        if (any < 0 || acc > cutoff || (acc == cutoff && ((uint64_t)c) > cutlim))
            any = -1;
        else
        {
            any  = 1;
            acc *= base;
            acc += c;
        }
    }
    if (any < 0) { acc = neg ? LONG_MIN : LONG_MAX; }
    else if (!any)
    {
    noconv: {
    }
    }
    else if (neg)
        acc = -acc;
    if (endptr != NULL) *endptr = (char *)(any ? s - 1 : str);
    return (acc);
}


char *strncpy(char *dest, const char *src, size_t n)
{
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++)
    {
        dest[i] = src[i];
    }
    for (; i < n; i++)
    {
        dest[i] = '\0';
    }
    return dest;
}

char *strrchr(const char *s, int c)
{
    char *last = NULL;
    while (*s)
    {
        if (*s == (char)c) { last = (char *)s; }
        s++;
    }
    return (c == '\0') ? (char *)s : last;
}

int isdigit(int c)
{
    return (c >= '0' && c <= '9');
}

int fls(unsigned int x)
{
    if (x == 0) return 0;
    return 32 - __builtin_clz(x);
}

/**
 * 跳过字符串中的数字，并返回连续的数字
 * @param s 指向字符串的指针（用于跳过）
 * @return 整数
 */
int skip_atoi(const char **s)
{
    int i = 0;
    while (IS_DIGIT(**s))
        i = i * 10 + *((*s)++) - '0';
    return i;
}

/**
 * 把整数转成字符串数字，并输出到`Writer`
 * @param str 字符串
 * @param fmter 整数格式化器
 * @param type 
 */
size_t wnumber(Writer *writer, num_formatter_t fmter, num_fmt_type type) // NOLINT
{
    char         c = 0;
    char         tmp[65];
    int          sign      = 0;
    const char  *digits    = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int          i         = 0; // index for tmp
    int64_t      size      = (int64_t)fmter.size;
    int64_t      precision = (int64_t)fmter.precision;
    size_t       base      = fmter.base;
    WriteHandler write     = writer->handler;
    size_t       result    = 0;

    if (type.small) digits = "0123456789abcdefghijklmnopqrstuvwxyz";
    if (type.left) type.zeropad = false; // if left adjust, zero padding is not allowed
    if (base < 2 || base > 36) return 0; // Invalid base

    c = (type.zeropad) ? '0' : ' ';

    /* Check sign */
    if (type.sign && (int64_t)fmter.num < 0)
    {
        sign      = '-';
        fmter.num = -(int64_t)fmter.num;
    }
    else if (type.plus) { sign = '+'; }
    else if (type.space) { sign = ' '; }
    else { sign = 0; }

    if (sign) size--;

    /* Special like 0x, 0 */
    if (type.special)
    {
        if (base == 16) { size -= 2; }
        else if (base == 8) { size--; }
    }

    i = 0;
    if (fmter.num == 0) { tmp[i++] = '0'; }
    else
    {
        while (fmter.num != 0)
        {
            tmp[i++]  = digits[(uint64_t)fmter.num % (uint64_t)base];
            fmter.num = (uint64_t)fmter.num / (uint64_t)fmter.base;
        }
    }
    if (i > precision) precision = i; // precision = max(precision, i);
    size -= precision;

    /* If type no include LEFT or ZEROPAD */
    if (!(type.zeropad || type.left))
    {
        /* Fill in the space */
        while (size-- > 0)
            write(writer, ' '), result++;
    }

    /* Write the sign */
    if (sign) write(writer, (char)sign), result++;

    /* Write the prefix */
    if (type.special)
    {
        if (base == 8) { write(writer, '0'), result++; }
        else if (base == 16)
        {
            write(writer, '0'), result++;
            write(writer, digits[33]), result++; // 33 is 'x' or 'X'
        }
    }

    if (!(type.left))
    {
        /* Write the padding */
        while (size-- > 0)
            write(writer, c), result++;
    }

    /* Write the zero padding */
    while (i < precision--)
        write(writer, '0'), result++;
    /* Write the number */
    while (i-- > 0)
        write(writer, tmp[i]), result++;

    /* LEFT adjust */
    while (size-- > 0)
        write(writer, ' '), result++;

    return result;
}

// 在 krlibc.cpp 末尾添加以下代码

// 安全的缓冲区写入处理器
typedef struct SafeBufWriterData
{
    size_t idx;      // 当前写入位置
    size_t max_size; // 缓冲区最大大小
    char  *buf;      // 缓冲区指针
} SafeBufWriterData;

uint8_t safe_buf_writer_handler(Writer *writer, char ch)
{
    SafeBufWriterData *data = (SafeBufWriterData *)writer->data;
    
    // 检查是否还有空间（为null终止符保留1字节）
    if (data->idx < data->max_size - 1) 
    {
        data->buf[data->idx] = ch;
        ++(data->idx);
        data->buf[data->idx] = '\0'; // 保持null终止
        return 0; // 成功
    }
    
    return 1; // 缓冲区已满
}
typedef struct UnsafeBufWriterData
{
    size_t idx; // index of buffer
    char  *buf; // Unsafe, because unknown size
} UnsafeBufWriterData;

uint8_t unsafe_buf_writer_handler(Writer *writer, char ch)
{
    UnsafeBufWriterData *data = (UnsafeBufWriterData *)writer->data;
    data->buf[data->idx]      = ch;
    ++(data->idx);
    data->buf[data->idx] = '\0'; // EOF (Must do, because the buffer may be full of dirty datas)
    return 0;                    // Right... Always success...?
}

// 完整的 snprintf 实现
int snprintf(char *str, size_t size, const char *format, ...)
{
    if (str == NULL || format == NULL || size == 0) 
        return -1;

    // 初始化缓冲区为空字符串
    str[0] = '\0';

    va_list args;
    va_start(args, format);
    
    SafeBufWriterData data = {
        .idx = 0,
        .max_size = size,
        .buf = str,
    };
    
    Writer SafeBufWriter = {
        .data = &data,
        .handler = safe_buf_writer_handler,
    };
    
    size_t result = vwprintf(&SafeBufWriter, format, args);
    
    va_end(args);
    
    // 确保字符串以null结尾
    if (data.idx < size) 
    {
        str[data.idx] = '\0';
    } 
    else if (size > 0) 
    {
        str[size - 1] = '\0';
    }
    
    return (int)result;
}

// 标准 sprintf 实现（不安全，但兼容）
int sprintf(char *str, const char *format, ...)
{
    if (str == NULL || format == NULL) 
        return -1;

    va_list args;
    va_start(args, format);
    
    // 使用足够大的缓冲区（假设最大4096字节）
    // 在实际系统中应该使用动态分配，这里简化处理
    char buffer[4096];
    UnsafeBufWriterData data = {
        .idx = 0,
        .buf = buffer,
    };
    
    Writer UnsafeBufWriter = {
        .data = &data,
        .handler = unsafe_buf_writer_handler,
    };
    
    size_t result = vwprintf(&UnsafeBufWriter, format, args);
    
    va_end(args);
    
    // 复制到目标缓冲区
    size_t copy_len = result;
    if (copy_len >= 4095) copy_len = 4095;
    memcpy(str, buffer, copy_len);
    str[copy_len] = '\0';
    
    return (int)result;
}

// vsnprintf 实现
int vsnprintf(char *str, size_t size, const char *format, va_list ap)
{
    if (str == NULL || format == NULL || size == 0) 
        return -1;

    // 初始化缓冲区为空字符串
    str[0] = '\0';

    SafeBufWriterData data = {
        .idx = 0,
        .max_size = size,
        .buf = str,
    };
    
    Writer SafeBufWriter = {
        .data = &data,
        .handler = safe_buf_writer_handler,
    };
    
    size_t result = vwprintf(&SafeBufWriter, format, ap);
    
    // 确保字符串以null结尾
    if (data.idx < size) 
    {
        str[data.idx] = '\0';
    } 
    else if (size > 0) 
    {
        str[size - 1] = '\0';
    }
    
    return (int)result;
}
size_t wfmt_arg(Writer *writer, const char **fmt_ptr, va_list args) //NOLINT // `fmt_ptr` is a pointer to `fmt`
{
    char        *str           = 0; // for `%s`
    size_t       write_counter = 0;
    size_t       str_len       = 0; // for align `%s`
    size_t       result        = 0; // Same as write_counter?
    WriteHandler write         = writer->handler;

    num_formatter_t num_fmter = {};
    num_fmt_type    num_flag  = {};
    int64_t         size_cnt  = INT;

    // Error args
    if (!writer || !write || !fmt_ptr || !(*fmt_ptr) || **fmt_ptr != '%') { return 0; }

    while (true)
    {
        ++(*fmt_ptr); // Skip '%' or any flags
        switch (**fmt_ptr)
        {
        case '-': num_flag.left = true; break;
        case '+': num_flag.plus = true; break;
        case ' ': num_flag.space = true; break;
        case '#': num_flag.special = true; break;
        case '0': num_flag.zeropad = true; break;
        default: break;
        }

        /* Calc num_fmter.size */
        if (IS_DIGIT(**fmt_ptr)) { num_fmter.size = skip_atoi(fmt_ptr); }
        else if (**fmt_ptr == '*')
        {
            /* by the following argument */
            ++(*fmt_ptr); // Skip '*'
            num_fmter.size = (size_t)va_arg(args, int);
        }

        /* Calc num_fmter.precision */
        if (**fmt_ptr == '.')
        {
            ++(*fmt_ptr); // Skip '.'
            if (IS_DIGIT(**fmt_ptr)) { num_fmter.precision = skip_atoi(fmt_ptr); }
            else if (**fmt_ptr == '*')
            {
                /* by the following argument */
                ++(*fmt_ptr); // Skip '*'
                num_fmter.precision = (size_t)va_arg(args, int);
            }
        }

        /* Calc size_cnt */
        switch (**fmt_ptr)
        {
        case 'h':
            size_cnt--;
            if (size_cnt < HALF_2) size_cnt = HALF_2; // hh
            continue;
        case 'L':       // += 2
            size_cnt++; // fallthrough
        case 'l':
            size_cnt++;
            if (size_cnt > LONG_2) size_cnt = LONG_2; // ll
            continue;
        case 'z':
            size_cnt = SIZE_T; // z
            continue;
        default: break;
        }

        /* Read argument */
        switch (**fmt_ptr)
        {
        case 'c': num_fmter.num = va_arg(args, int); break;
        case 's':
            str                    = va_arg(args, char *);
            static char null_str[] = "(null)";
            if (str == 0) str = null_str;
            break;
        case 'd':
        case 'i':
            // NOLINTBEGIN
            switch (size_cnt)
            {
            case HALF_2: num_fmter.num = (size_t)(char)va_arg(args, int); break;
            case HALF_1: num_fmter.num = (size_t)(short)va_arg(args, int); break;
            case INT: num_fmter.num = (size_t)(int)va_arg(args, int); break;
            case LONG_1: num_fmter.num = (size_t)(long)va_arg(args, long); break;
            case LONG_2: num_fmter.num = (size_t)(long long)va_arg(args, long long); break;
            case SIZE_T: // fallthrough
            default: num_fmter.num = va_arg(args, size_t); break;
            }
            // NOLINTEND
            break;
        case 'o':
        case 'x':
        case 'X':
        case 'b':
        case 'u':
            switch (size_cnt)
            {
            case HALF_2: num_fmter.num = (size_t)(unsigned char)va_arg(args, int); break;
            case HALF_1: num_fmter.num = (size_t)(unsigned short)va_arg(args, int); break;
            case INT: num_fmter.num = (size_t)(unsigned int)va_arg(args, int); break;
            case LONG_1: num_fmter.num = (size_t)(unsigned long)va_arg(args, long); break;
            case LONG_2: num_fmter.num = (size_t)(unsigned long long)va_arg(args, long long); break;
            case SIZE_T: // fallthrough
            default: num_fmter.num = va_arg(args, size_t); break;
            }
            break;
        case 'p': num_fmter.num = (size_t)va_arg(args, void *); break;
        default: // may no data
            break;
        }

        /* Calc length of `%s` and set num_flag */
        switch (**fmt_ptr)
        {
        case 'c': break;
        case 's':
            str_len = strlen(str);
            if (num_fmter.size < str_len) num_fmter.size = str_len;
            break;
        case 'o': num_fmter.base = 8; break;
        case 'p':
            num_flag.small   = true;
            num_flag.special = true;
            num_flag.zeropad = true;
            if (num_fmter.size < 16) num_fmter.size = 16;
            num_fmter.base = 16;
            break;
        case 'x': num_flag.small = true; // fallthrough
        case 'X': num_fmter.base = 16; break;
        case 'd':
        case 'i': num_flag.sign = true;
        case 'u': num_fmter.base = 10; break;
        case 'b': num_fmter.base = 2; break;
        case 'n': va_arg(args, void *); break;
        case '%': break;
        default:
            // Unexpected
            return 0;
        }

        /* Write to arg space */
        switch (**fmt_ptr)
        {
        case 'c':
            // Right align
            if (!(num_flag.left))
            {
                while (write_counter < num_fmter.size - 1)
                {
                    write(writer, ' ');
                    ++write_counter;
                }
            }
            // Write char
            write(writer, (char)num_fmter.num);
            // Left align
            if (num_flag.left)
            {
                while (write_counter < num_fmter.size - 1)
                {
                    write(writer, ' ');
                    ++write_counter;
                }
            }
            break;
        case 's':
            // Right align
            if (!(num_flag.left))
            {
                while (write_counter < num_fmter.size - str_len)
                {
                    write(writer, ' ');
                    ++write_counter;
                }
                str_len = num_fmter.size;
            }
            // Write string
            while (write_counter < str_len)
            {
                write(writer, *str);
                ++str;
                ++write_counter;
            }
            // Left align
            if (num_flag.left)
            {
                while (write_counter < num_fmter.size - str_len)
                {
                    write(writer, ' ');
                    ++write_counter;
                }
            }
            break;
        case 'o':                                                               // fallthrough
        case 'p':                                                               // fallthrough
        case 'x':                                                               // fallthrough
        case 'X':                                                               // fallthrough
        case 'd':                                                               // fallthrough
        case 'i':                                                               // fallthrough
        case 'u':                                                               // fallthrough
        case 'b': write_counter += wnumber(writer, num_fmter, num_flag); break; // Format number with `Writer`
        case '%': write(writer, '%'); break;
        default: break;
        }
        break;
    }
    result = write_counter;
    return result;
}
size_t vwprintf(Writer *writer, const char *fmt, va_list args)
{
    const char  *fmt_ptr = fmt;
    WriteHandler write   = writer->handler;
    size_t       result  = 0;
    // TODO
    while (*fmt_ptr != '\0')
    {
        if (*fmt_ptr != '%')
        {
            write(writer, *fmt_ptr); // TODO: Catch Error
            fmt_ptr++;
            result++;
            continue;
        }
        // *fmt_ptr == '%'
        result += wfmt_arg(writer, &fmt_ptr, args);
        fmt_ptr++;
    }
    return result;
}
// 简单的字符串格式化辅助函数（用于shell命令）
int simple_snprintf(char *str, size_t size, const char *format, ...)
{
    if (size == 0) return 0;
    
    va_list args;
    va_start(args, format);
    int result = vsnprintf(str, size, format, args);
    va_end(args);
    
    return result;
}

// 在 shell.cpp 中添加辅助函数
void uint64_to_string(uint64_t num, char *buffer, int buffer_size)
{
    if (buffer_size <= 0) return;
    
    char *p = buffer;
    char temp[32];
    char *temp_p = temp;
    
    if (num == 0) {
        *p++ = '0';
    } else {
        // 将数字逆序存入临时缓冲区
        while (num > 0 && temp_p < temp + sizeof(temp) - 1) {
            *temp_p++ = '0' + (num % 10);
            num /= 10;
        }
        
        // 将数字正序复制到目标缓冲区
        while (temp_p > temp && p < buffer + buffer_size - 1) {
            *p++ = *(--temp_p);
        }
    }
    
    *p = '\0';
}

void int_to_string(int num, char *buffer, int buffer_size)
{
    if (buffer_size <= 0) return;
    
    char *p = buffer;
    
    if (num < 0) {
        *p++ = '-';
        num = -num;
    }
    
    uint64_to_string((uint64_t)num, p, buffer_size - (p - buffer));
}

// static inline void *memmove(void *dest, const void *src, size_t n)
// {
//     uint8_t       *pdest = (uint8_t *)dest;
//     const uint8_t *psrc  = (const uint8_t *)src;

//     if (src > dest)
//     {
//         for (size_t i = 0; i < n; i++)
//         {
//             pdest[i] = psrc[i];
//         }
//     }
//     else if (src < dest)
//     {
//         for (size_t i = n; i > 0; i--)
//         {
//             pdest[i - 1] = psrc[i - 1];
//         }
//     }

//     return dest;
// }

void merge_string(char *dst, char *src)
{
    char *s1 = dst;
    char *s2 = src;

    while (*s1)
        s1++;

    while (*s2)
    {
        if (*(s1 - 1) == '/' && *s2 == '/')
        {
            s1++;
            s2++;
            continue;
        }
        *s1 = *s2;
        s1++;
        s2++;
    }

    *s1 = '\0';
}

void revert_path(char *path)
{
    char *s = path;
    while (*s)
        s++;

    while (*s != '/')
        s--;

    if (s == path) s++;

    *s = '\0';
}