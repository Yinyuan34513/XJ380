#include "../include/include.h"
#include "math.h"
int printf(const char *str, ...) {
    if (str == NULL) return -1;

    unsigned int  ret_num   = 0;           // 返回打印字符的个数
    char         *pStr      = (char *)str; // 指向str
    int           ArgIntVal = 0;           // 接收整型
    unsigned long ArgHexVal = 0;           // 接十六进制
    char         *ArgStrVal = NULL;        // 接收字符型
    double        ArgFloVal = 0.0;         // 接受浮点型
    unsigned long val_seg   = 0;           // 数据切分
    unsigned long val_temp  = 0;           // 临时保存数据
    int           cnt       = 0;           // 数据长度计数
    int           i         = 0;

    __builtin_va_list pArgs;        // 定义__builtin_va_list类型指针，用于存储参数的地址
    __builtin_va_start(pArgs, str); // 初始化pArgs
    while (*pStr != '\0') {
        switch (*pStr) {
        case ' ':
            write_serial(*pStr);
            ret_num++;
            break;
        case '\t':
            write_serial(*pStr);
            ret_num += 4;
            break;
        case '\r':
            write_serial(*pStr);
            ret_num++;
            break;
        case '\n':
            write_serial(*pStr);
            ret_num++;
            break;
        case '%':
            pStr++;
            // % 格式解析
            switch (*pStr) {
            case '%':
                write_serial('%'); // %%，输出%
                ret_num++;
                pStr++;
                continue;
            case 'c':
                ArgIntVal = __builtin_va_arg(pArgs, int); // %c，输出char
                write_serial((char)ArgIntVal);
                ret_num++;
                pStr++;
                continue;
            case 'd':
                // 接收整型
                ArgIntVal = __builtin_va_arg(pArgs, int);
                if (ArgIntVal < 0) // 如果为负数打印，负号
                {
                    ArgIntVal = -ArgIntVal; // 取相反数

                    write_serial('-');
                    ret_num++;
                }
                val_seg = ArgIntVal; // 赋值给 val_seg处理数据
                // 计算ArgIntVal长度
                if (ArgIntVal) {
                    while (val_seg) {
                        cnt++;
                        val_seg /= 10;
                    }
                } else
                    cnt = 1; // 数字0的长度为1

                ret_num += cnt; // 字符个数加上整数的长度

                // 将整数转为单个字符打印
                while (cnt) {
                    val_seg    = ArgIntVal / pow(10, cnt - 1);
                    ArgIntVal %= (int)pow(10, cnt - 1);
                    write_serial((char)val_seg + '0');
                    cnt--;
                }
                pStr++;
                continue;
            case 'o':
                // 接收整型
                ArgIntVal = __builtin_va_arg(pArgs, int);
                if (ArgIntVal < 0) // 如果为负数打印，负号
                {
                    ArgIntVal = -ArgIntVal; // 取相反数

                    write_serial('-');
                    ret_num++;
                }
                val_seg = ArgIntVal; // 赋值给 val_seg处理数据
                // 计算ArgIntVal长度
                if (ArgIntVal) {
                    while (val_seg) {
                        cnt++;
                        val_seg /= 8;
                    }
                } else
                    cnt = 1; // 数字0的长度为1

                ret_num += cnt; // 字符个数加上整数的长度

                // 将整数转为单个字符打印
                while (cnt) {
                    val_seg    = ArgIntVal / pow(8, cnt - 1);
                    ArgIntVal %= (int)pow(8, cnt - 1);
                    write_serial((char)val_seg + '0');
                    cnt--;
                }
                pStr++;
                continue;
            case 'x':
                // 接收16进制
                ArgHexVal = __builtin_va_arg(pArgs, unsigned long);
                val_seg   = ArgHexVal;
                // 计算ArgIntVal长度
                if (ArgHexVal) {
                    while (val_seg) {
                        cnt++;
                        val_seg /= 16;
                    }
                } else
                    cnt = 1; // 数字0的长度为1

                ret_num += cnt; // 字符个数加上整数的长度
                // 将整数转为单个字符打印
                while (cnt) {
                    val_seg    = ArgHexVal / pow(16, cnt - 1);
                    ArgHexVal %= (int)pow(16, cnt - 1);
                    if (val_seg <= 9)
                        write_serial((char)val_seg + '0');
                    else {
                        //write_serial((char)val_seg - 10 + 'a'); //小写字母
                        write_serial((char)val_seg - 10 + 'A');
                    }
                    cnt--;
                }
                pStr++;
                continue;
            case 'b':
                // 接收整型
                ArgIntVal = __builtin_va_arg(pArgs, int);
                val_seg   = ArgIntVal;
                // 计算ArgIntVal长度
                if (ArgIntVal) {
                    while (val_seg) {
                        cnt++;
                        val_seg /= 2;
                    }
                } else
                    cnt = 1; // 数字0的长度为1

                ret_num += cnt; // 字符个数加上整数的长度
                // 将整数转为单个字符打印
                while (cnt) {
                    val_seg    = ArgIntVal / pow(2, cnt - 1);
                    ArgIntVal %= (int)pow(2, cnt - 1);
                    write_serial((char)val_seg + '0');
                    cnt--;
                }
                pStr++;
                continue;
            case 's':
                // 接收字符
                ArgStrVal  = __builtin_va_arg(pArgs, char *);
                ret_num   += (unsigned int)strlen(ArgStrVal);
                while (*ArgStrVal) {
                    write_serial(*ArgStrVal);
                    ArgStrVal++;
                }

                pStr++;
                continue;

            case 'f':
                // 接收浮点型 保留6为小数，不采取四舍五入
                ArgFloVal = __builtin_va_arg(pArgs, double);
                val_seg   = (unsigned long)ArgFloVal; // 取整数部分
                val_temp  = val_seg;                  // 临时保存整数部分数据
                ArgFloVal = ArgFloVal - val_seg;      // 得出余下的小数部分
                // 计算整数部分长度
                if (val_seg) {
                    while (val_seg) {
                        cnt++;
                        val_seg /= 10;
                    }
                } else
                    cnt = 1;    // 数字0的长度为1
                ret_num += cnt; // 字符个数加上整数的长度
                // 将整数转为单个字符打印
                while (cnt) {
                    val_seg   = val_temp / pow(10, cnt - 1);
                    val_temp %= (int)pow(10, cnt - 1);
                    write_serial((char)val_seg + '0');
                    cnt--;
                }
                // 打印小数点
                write_serial('.');
                ret_num++;
                // 开始输出小数部分
                ArgFloVal *= 1000000;
                // printf("\r\n %f\r\n", ArgFloVal);
                cnt      = 6;
                val_temp = (int)ArgFloVal; // 取整数部分
                while (cnt) {
                    val_seg   = val_temp / pow(10, cnt - 1);
                    val_temp %= (int)pow(10, cnt - 1);
                    write_serial((char)val_seg + '0');
                    cnt--;
                }
                ret_num += 6;
                pStr++;
                continue;
            default: // % 匹配错误，暂输出空格
                write_serial(' ');
                ret_num++;
                continue;
            }

        default:
            write_serial(*pStr);
            ret_num++;
            break;
        }
        pStr++;
    }
    __builtin_va_end(pArgs); // 结束取参数

    return ret_num;
}