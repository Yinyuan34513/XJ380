#define __STD_LIB__
#include "../include/include.h"

static char NUM_CHAR[] = "0123456789ABCDEF";
int vsprintf(char *out, const char *fmt, __builtin_va_list ap)
{
    int len = 0;
    char buf[128];
    while (*fmt != '\0')
    {
        switch (*fmt)
        {
        case '%':
            fmt++;
            switch (*fmt)
            {
                int num_len;
            case 'd':

            {
                int val = __builtin_va_arg(ap, int);
                if (val == 0)
                    out[len++] = '0';
                if (val < 0)
                {
                    out[len++] = '-';
                    val = 0 - val;
                }
                for (num_len = 0; val; val /= 10, num_len++)
                    buf[num_len] = NUM_CHAR[val % 10];
                for (int j = num_len - 1; j >= 0; j--)
                    out[len++] = buf[j];
                // out[len++] = '\0';
                break;
            }
            case 'u':
            {
                int unum32_len = 0;
                uint32_t unum32 = __builtin_va_arg(ap, uint32_t);
                if (unum32 == 0)
                    out[len++] = '0';
                else
                {
                    while (unum32 > 0)
                    {
                        buf[unum32_len++] = '0' + (unum32 % 10);
                        unum32 /= 10;
                    }
                }
                for (int i = unum32_len - 1; i >= 0; i--)
                    out[len++] = buf[i];
                break;
            }
            case 'c':
            {
                char c = (char)__builtin_va_arg(ap, int);
                out[len++] = c;
                break;
            }
            case 's':
            {
                char *s = __builtin_va_arg(ap, char *);
                for (int i = 0; s[i] != '\0'; i++)
                    out[len++] = s[i];
                break;
            }
            case 'x':
            {
                unsigned int unum = __builtin_va_arg(ap, unsigned int);
                if (unum == 0)
                {
                    out[len++] = '0';
                    break;
                }
                for (num_len = 0; unum; unum >>= 4, num_len++)
                    buf[num_len] = NUM_CHAR[unum & 0xf];
                for (int i = num_len - 1; i >= 0; i--)
                    out[len++] = buf[i];
                break;
            }
            case 'p':
            {
                out[len++] = '0';
                out[len++] = 'x';
                uint32_t address = __builtin_va_arg(ap, uint32_t);
                for (num_len = 0; address; address /= 16, num_len++)
                    buf[num_len] = NUM_CHAR[address % 16];
                for (int i = num_len - 1; i >= 0; i--)
                    out[len++] = buf[i];
                break;
            }
            break;
            }
        case '\n':
            out[len++] = '\n';
            break;
        case '\r':
            out[len++] = '\r';
            break;
        case '\t':
            out[len++] = '\t';
            break;
        case '\b':
            out[len++] = '\b';
            break;
        default:
            out[len++] = *fmt;
        }
        fmt++;
    }
    out[len] = '\0';
    return len;
}
int printf(const char *fmt, ...)
{
    char buf[2048] = {0};
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);

    int res = vsprintf(buf, fmt, ap);
    __builtin_va_end(ap);
    write_serial_string(buf);
    return res;
}