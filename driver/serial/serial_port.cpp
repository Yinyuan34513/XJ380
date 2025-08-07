#include "krlibc.h" // For memset
#include "mm/alloc/alloc.h"
#include "stdarg.h"
#include <cpu/lock.h>
#include <proto.h>

spin_t serial_lock;
spin_t fmt_lock; // For write_serial_fmt

#if BUILD_VERSION == DEBUG_VERSION
int serial_x     = 0;
int serial_y     = 0;
int serial_x_max = 0;
#endif

#define PORT 0x3f8 // COM1

int init_serial()
{
    serial_lock = SPIN_INIT;
    fmt_lock    = SPIN_INIT;

    outb(PORT + 1, 0x00); // Disable all interrupts
    outb(PORT + 3, 0x80); // Enable DLAB (set baud rate divisor)
    outb(PORT + 0, 0x03); // Set divisor to 3 (lo byte) 38400 baud
    outb(PORT + 1, 0x00); //                  (hi byte)
    outb(PORT + 3, 0x03); // 8 bits, no parity, one stop bit
    outb(PORT + 2, 0xC7); // Enable FIFO, clear them, with 14-byte threshold
    outb(PORT + 4, 0x0B); // IRQs enabled, RTS/DSR set
    outb(PORT + 4, 0x1E); // Set in loopback mode, test the serial chip
    outb(PORT + 0, 0xAE); // Test serial chip (send byte 0xAE and check if serial returns same byte)

    // Check if serial is faulty (i.e: not same byte as sent)
    if (inb(PORT + 0) != 0xAE)
    {
        // 不支持
        return 1;
    }

    // If serial is not faulty set it in normal operation mode
    // (not-loopback with IRQs enabled and OUT#1 and OUT#2 bits enabled)
    outb(PORT + 4, 0x0F);
    // #if BUILD_VERSION == DEBUG_VERSION
    // 	WriteString(*fbc_addr, serial_x, serial_y, "XJ380 Operating System - Debug Mode (Serial Outputer)", {0xFF, 0xFF, 0xFF});
    // 	serial_y += 16;
    // 	WriteString(*fbc_addr, serial_x, serial_y, "Copyright(C) XINGJI Interactive Software 2017 - 2025 All rights reserved.", {0xFF, 0xFF, 0xFF});
    // 	serial_y += 16;
    // #endif
    return 0;
}

int is_transmit_empty()
{
    return inb(PORT + 5) & 0x20;
}

void write_serial(char a)
{
    while (is_transmit_empty() == 0)
        ;

    outb(PORT, a);
}

void write_serial_string(const char *str)
{
    close_interrupt; // Avoid deadlock
    spin_lock(serial_lock);
    while (*str)
    {
        // #if BUILD_VERSION == DEBUG_VERSION
        // 		if (*str == '\n') {
        // 			serial_x = 0;
        // 			serial_y += 16;
        // 		} else {
        // 			rect(*fbc_addr, serial_x, serial_y, serial_x + 8, serial_y + 16, {0, 0, 0});
        // 			WriteAscii(*fbc_addr, serial_x, serial_y, *str, {0xff, 0xff, 0xff});
        // 			serial_x += 8;
        // 		}

        // 		if (serial_y >= fbc_addr->vertical_resolution) {
        // 			rect(*fbc_addr, 0, 32, serial_x_max, fbc_addr->vertical_resolution, {0, 0, 0});
        // 			serial_x_max = 0;
        // 			serial_y = 32;
        // 		}
        // 		if (serial_x > serial_x_max) {
        // 			serial_x_max = serial_x;
        // 		}
        // #endif
        write_serial(*str++);
    }
    spin_unlock(serial_lock);
    open_interrupt;
}

void write_serial_dec(unsigned long long dec)
{
    char     str[255];
    int      radix   = 10;
    char     index[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"; // 索引表
    unsigned unum;                                             // 存放要转换的整数的绝对值,转换的整数可能是负数
    int      i = 0, j,
        k; // i用来指示设置字符串相应位，转换之后i其实就是字符串的长度；转换后顺序是逆序的，有正负的情况，k用来指示调整顺序的开始位置;j用来指示调整顺序时的交换。

    // 获取要转换的整数的绝对值
    if (radix == 10 && dec < 0) // 要转换成十进制数并且是负数
    {
        unum     = (unsigned)-dec; // 将num的绝对值赋给unum
        str[i++] = '-';            // 在字符串最前面设置为'-'号，并且索引加1
    }
    else
        unum = (unsigned)dec; // 若是num为正，直接赋值给unum

    // 转换部分，注意转换后是逆序的
    do
    {
        str[i++]  = index[unum % (unsigned)radix]; // 取unum的最后一位，并设置为str对应位，指示索引加1
        unum     /= radix;                         // unum去掉最后一位

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
        temp               = str[j];             // 头部赋值给临时变量
        str[j]             = str[i - 1 + k - j]; // 尾部赋值给头部
        str[i - 1 + k - j] = temp;               // 将临时变量的值(其实就是之前的头部值)赋给尾部
    }

    write_serial_string(str);
}

void write_serial_hex(unsigned long long hex)
{
    // 输出16进制
    char  buf[32];
    char *p = buf;
    char  ch;
    int   i, flag = 0;

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
            {                // 跳过前导0
                flag  = 1;   // 没有前导0就把flag设为1，这样后面再有0也不会忽略
                ch   += '0'; // 0~9 => '0'~'9'
                if (ch > '9')
                {
                    ch += 7; // 'A' - '9' = 7
                }
                *p++ = ch; // 写入
            }
        }
    }
    *p = '\0'; // 结束符

    write_serial_string(buf);
}

typedef enum
{
    FMT_START = 0,
    FMT_PROCESSING,
    FMT_OVERFLOW_FMTARG,
    FMT_OVERFLOW_FMTSTR,
    FMT_FINISHED,
} fmt_state;

typedef enum
{
    HEAPLESS = 0, // Unsafe, because it uses static buffer (and may overflow)
    HEAP,
} fmt_type;

struct fmt_arg
{
    size_t size;       // Size of the arg string
    char  *arg_str;    // Pointer to the formatted arg string
    char  *last_write; // Pointer to the last written character of arg_str
};

typedef struct fmt_arg fmt_arg_t;

struct formatter
{
    const char **fmt;
    char        *write_ptr;
    char        *buf;
    size_t       buf_size;
    va_list      args;
    fmt_state    state;
    fmt_arg_t    fmt_arg;
    fmt_type     type;
};

typedef struct formatter formatter_t;

#define HEAPLESS_FMT_ARG_LIMIT 2048                   // Maximum size of fmt_arg_t
static char fmt_arg_buf[HEAPLESS_FMT_ARG_LIMIT] = {}; // Static buffer for fmt_arg_t

typedef enum num_size
{
    HALF_2 = 0, // char
    HALF_1 = 1, // short
    INT    = 2, // int
    LONG_1 = 3, // long
    LONG_2 = 4, // long long
    SIZE_T = 5, // size_t
} num_size_t;

/**
 * Notice: May use heap memory
 */
static void read_fmtarg(formatter_t *fmter) // NOLINT
{
    fmt_arg_t  *fmt_arg = &fmter->fmt_arg;
    const char *fmt_ptr = *fmter->fmt;
    char       *str     = 0; // for `%s`
    char       *arg_str = 0;
    char       *arg_ptr = 0;
    size_t      str_len = 0; // length of the string
    size_t      arg_len = 0; // length of the argument will be written

    num_formatter_t num_fmter = {};
    num_fmt_type    num_flag  = {};
    int64_t         size_cnt  = INT;

    if (*fmt_ptr != '%')
    {
        // Unexpected character
        *fmter->fmt = fmt_ptr;
        return;
    }

    while (true)
    {
        ++fmt_ptr; // Skip '%' or any flags
        switch (*fmt_ptr)
        {
        case '-': num_flag.left = true; break;
        case '+': num_flag.plus = true; break;
        case ' ': num_flag.space = true; break;
        case '#': num_flag.special = true; break;
        case '0': num_flag.zeropad = true; break;
        default: break;
        }

        /* Calc num_fmter.size */
        if (IS_DIGIT(*fmt_ptr)) { num_fmter.size = skip_atoi(&fmt_ptr); }
        else if (*fmt_ptr == '*')
        {
            /* by the following argument */
            ++fmt_ptr; // Skip '*'
            num_fmter.size = (size_t)va_arg(fmter->args, int);
        }

        /* Calc num_fmter.precision */
        if (*fmt_ptr == '.')
        {
            ++fmt_ptr; // Skip '.'
            if (IS_DIGIT(*fmt_ptr)) { num_fmter.precision = skip_atoi(&fmt_ptr); }
            else if (*fmt_ptr == '*')
            {
                /* by the following argument */
                ++fmt_ptr; // Skip '*'
                num_fmter.precision = (size_t)va_arg(fmter->args, int);
            }
        }

        /* Calc size_cnt */
        switch (*fmt_ptr)
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
        switch (*fmt_ptr)
        {
        case 'c': num_fmter.num = va_arg(fmter->args, int); break;
        case 's': str = va_arg(fmter->args, char *); break;
        case 'd':
        case 'i':
            // NOLINTBEGIN
            switch (size_cnt)
            {
            case HALF_2: num_fmter.num = (size_t)(char)va_arg(fmter->args, int); break;
            case HALF_1: num_fmter.num = (size_t)(short)va_arg(fmter->args, int); break;
            case INT: num_fmter.num = (size_t)(int)va_arg(fmter->args, int); break;
            case LONG_1: num_fmter.num = (size_t)(long)va_arg(fmter->args, long); break;
            case LONG_2: num_fmter.num = (size_t)(long long)va_arg(fmter->args, long long); break;
            case SIZE_T: // fallthrough
            default: num_fmter.num = va_arg(fmter->args, size_t); break;
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
            case HALF_2: num_fmter.num = (size_t)(unsigned char)va_arg(fmter->args, int); break;
            case HALF_1: num_fmter.num = (size_t)(unsigned short)va_arg(fmter->args, int); break;
            case INT: num_fmter.num = (size_t)(unsigned int)va_arg(fmter->args, int); break;
            case LONG_1: num_fmter.num = (size_t)(unsigned long)va_arg(fmter->args, long); break;
            case LONG_2: num_fmter.num = (size_t)(unsigned long long)va_arg(fmter->args, long long); break;
            case SIZE_T: // fallthrough
            default: num_fmter.num = va_arg(fmter->args, size_t); break;
            }
            break;
        case 'p': num_fmter.num = (size_t)va_arg(fmter->args, void *); break;
        default: // may no data
            break;
        }

        /* Calc the size of argument and set num_flag */
        switch (*fmt_ptr)
        {
        case 'c':
            arg_len = 1;
            if (num_fmter.size > arg_len) num_fmter.size = arg_len;
            break;
        case 's':
            static char null_str[] = "(null)";
            if (str == 0) str = null_str;
            arg_len = str_len = strlen(str);
            if (num_fmter.size > arg_len) arg_len = num_fmter.size;
            break;
        case 'o':
            num_fmter.base = 8;
            arg_len        = number_length(num_fmter, num_flag);
            break;
        case 'p':
            num_flag.small   = true;
            num_flag.special = true;
            num_flag.zeropad = true;
            if (num_fmter.size < 16) num_fmter.size = 16;
            num_fmter.base = 16;
            arg_len        = number_length(num_fmter, num_flag);
            break;
        case 'x': num_flag.small = true; // fallthrough
        case 'X':
            num_fmter.base = 16;
            arg_len        = number_length(num_fmter, num_flag);
            break;
        case 'd':
        case 'i': num_flag.sign = true;
        case 'u':
            num_fmter.base = 10;
            arg_len        = number_length(num_fmter, num_flag);
            break;
        case 'b':
            num_fmter.base = 2;
            arg_len        = number_length(num_fmter, num_flag);
            break;
        case 'n': va_arg(fmter->args, void *); break;
        case '%': arg_len = 1; break;
        default:
            // Unexpected
            --fmt_ptr;
            fmter->fmt_arg.arg_str = 0; // Caller will skip this argument
            return;
        }
        if (arg_len < 1) arg_len = 1;
        if (num_fmter.size < arg_len) num_fmter.size = arg_len;

        /* Alloc space */
        switch (fmter->type)
        {
        case HEAP: fmter->fmt_arg.arg_str = (char *)malloc(arg_len + 1); break;
        case HEAPLESS:
            if (arg_len > HEAPLESS_FMT_ARG_LIMIT)
            {
                fmter->fmt_arg.arg_str = 0; // Caller will skip this argument
                return;
            }
            fmter->fmt_arg.arg_str = fmt_arg_buf;
        }

        /* Init arg space */
        arg_str                   = fmter->fmt_arg.arg_str;
        arg_str[arg_len]          = '\0';
        arg_ptr                   = arg_str;
        fmter->fmt_arg.size       = arg_len;
        fmter->fmt_arg.last_write = fmter->fmt_arg.arg_str;

        /* Write to arg space */
        switch (*fmt_ptr)
        {
        case 'c':
            // Right align
            if (!(num_flag.left))
            {
                while (arg_ptr < arg_str + num_fmter.size - 1)
                {
                    *arg_ptr = ' ';
                    ++arg_ptr;
                }
            }
            // Write char
            *arg_ptr = (char)num_fmter.num;
            // Left align
            if (num_flag.left)
            {
                while (arg_ptr < arg_str + num_fmter.size - 1)
                {
                    *arg_ptr = ' ';
                    ++arg_ptr;
                }
            }
            break;
        case 's':
            // Right align
            if (!(num_flag.left))
            {
                while (arg_ptr < arg_str + num_fmter.size - str_len)
                {
                    *arg_ptr = ' ';
                    ++arg_ptr;
                }
                str_len = num_fmter.size;
            }
            // Write string
            while (arg_ptr < arg_str + str_len)
            {
                *arg_ptr = *str;
                ++arg_ptr;
                ++str;
            }
            // Left align
            if (num_flag.left)
            {
                while (arg_ptr < arg_str + arg_len)
                {
                    *arg_ptr = ' ';
                    arg_ptr++;
                }
            }
            break;
        case 'o': // fallthrough
        case 'p': // fallthrough
        case 'x': // fallthrough
        case 'X': // fallthrough
        case 'd': // fallthrough
        case 'i': // fallthrough
        case 'u': // fallthrough
        case 'b': number(arg_str, num_fmter, num_flag); break;
        case '%': *arg_ptr = '%'; break;
        default: break;
        }
        break;
    }
    // Update fmter->fmt
    *fmter->fmt = fmt_ptr;
}

/**
 * Notice: May use heap memory
 */

static void write_fmt_arg(formatter_t *fmter)
{
    char       *write_ptr = fmter->write_ptr;
    const char *fmt_ptr   = *fmter->fmt;
    fmt_arg_t   fmt_arg   = fmter->fmt_arg;
    /* Write buffer of fmt_arg to buffer */
    while (fmt_arg.last_write < fmt_arg.size + fmt_arg.arg_str)
    {
        *write_ptr = *fmt_arg.last_write;
        write_ptr++;
        fmt_arg.last_write++;
        if (write_ptr >= fmter->buf + fmter->buf_size - 1) // 1 for '\0'
        {
            *write_ptr = '\0';
            write_ptr++;
            fmter->write_ptr = write_ptr;
            fmter->state     = FMT_OVERFLOW_FMTARG;
            fmter->fmt_arg   = fmt_arg; // Save fmt_arg for next call
            return;
        }
    }
    /* Write finished */
    if (fmter->type == HEAP)
    {
        free(fmt_arg.arg_str); // Free heap memory
    }
    fmt_arg.arg_str    = 0;
    fmt_arg.last_write = 0;
    fmt_arg.size       = 0;
    // update fmt_arg in formatter
    fmter->fmt_arg   = fmt_arg;
    fmter->state     = FMT_PROCESSING;
    fmter->write_ptr = write_ptr;
}

/**
 * Notice: May use heap memory
 */
static void vsprintfmtr(formatter_t *fmter)
{
    char *write_ptr     = fmter->buf;
    fmter->write_ptr    = write_ptr;
    const char *fmt_ptr = *fmter->fmt;
    fmt_arg_t   fmt_arg = fmter->fmt_arg;
    switch (fmter->state)
    {
    case FMT_START: fmter->state = FMT_PROCESSING; // fallthrough
    case FMT_PROCESSING:                           // fallthrough
    case FMT_OVERFLOW_FMTSTR: break;
    case FMT_OVERFLOW_FMTARG:
        fmter->write_ptr = write_ptr;
        write_fmt_arg(fmter); // May update fmter->write_ptr
        if (fmter->state == FMT_OVERFLOW_FMTARG)
        {
            return; // Overflow, wait for next call
        }
        break;
    case FMT_FINISHED: return; // Do nothing
    }
    write_ptr          = fmter->write_ptr;
    fmt_arg.size       = 0;
    fmt_arg.arg_str    = 0;
    fmt_arg.last_write = 0;
    while (*fmt_ptr != '\0')
    {
        if (write_ptr >= fmter->buf + fmter->buf_size - 1)
        {
            *write_ptr   = '\0';
            *fmter->fmt  = fmt_ptr;             // Update
            fmter->state = FMT_OVERFLOW_FMTSTR; // Overflow, wait for next call
            return;
        }
        if (*fmt_ptr != '%' && write_ptr < fmter->buf + fmter->buf_size - 1)
        {
            *write_ptr = *fmt_ptr;
            write_ptr++;
            fmt_ptr++;
            continue;
        }
        // *fmt_ptr == '%'
        // Update fmter->fmt
        *fmter->fmt = fmt_ptr; // Update fmt_ptr to next position
        read_fmtarg(fmter);
        if (fmter->fmt_arg.arg_str == 0)
        {
            // Skip
            continue;
        }
        ++(*fmter->fmt);
        fmt_ptr          = *fmter->fmt;
        fmter->write_ptr = write_ptr;
        write_fmt_arg(fmter); // May update fmter->buf
        write_ptr = fmter->write_ptr;
        if (fmter->state == FMT_OVERFLOW_FMTARG)
        {
            return; // Overflow, wait for next call
        }
    }
    // Finish
    fmter->state = FMT_FINISHED;
    *write_ptr   = '\0';
}

/**
 * Notice: Must use heap memory
 */
void write_serial_fmt(const char *fmt, ...)
{
    close_interrupt; // Avoid deadlock
    spin_lock(fmt_lock);
    formatter_t formatter;
    char        fmt_buff[2048] = {};
    formatter.buf              = fmt_buff;
    formatter.state            = FMT_START;
    formatter.fmt              = &fmt;
    formatter.buf_size         = 2048;
    formatter.type             = HEAP;
    va_start(formatter.args, fmt);
    while (formatter.state != FMT_FINISHED)
    {
        vsprintfmtr(&formatter);
        write_serial_string(formatter.buf);
    }
    va_end(formatter.args);
    spin_unlock(fmt_lock);
    open_interrupt;
}

/**
 * Heapless
 */

void write_serial_fmt_heapless(const char *fmt, ...)
{
    close_interrupt; // Avoid deadlock
    spin_lock(fmt_lock);
    formatter_t formatter;
    char        fmt_buff[2048] = {};
    formatter.buf              = fmt_buff;
    formatter.state            = FMT_START;
    formatter.fmt              = &fmt;
    formatter.buf_size         = 2048;
    formatter.type             = HEAPLESS;
    va_start(formatter.args, fmt);
    while (formatter.state != FMT_FINISHED)
    {
        vsprintfmtr(&formatter);
        write_serial_string(formatter.buf);
    }
    va_end(formatter.args);
    spin_unlock(fmt_lock);
    open_interrupt;
}