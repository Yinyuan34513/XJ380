#pragma once
#include "../include/include.h"
// 哦，我们亲爱的派
#define PI                                                                                         \
    (float64_t)3.1415926535 // 搞笑版:3.14159265358979323846264338327950288419716939937510582097494459230781640628620899
#define TAU                                                                                        \
    (float64_t)6.2831853071 // 搞笑版:6.28318530717958647692528676655900576839433879875021164194988918553642212019584998
#define PI_DIV_2                                                                                   \
    (float64_t)1.5707963267 // 搞笑版:1.570796326794896619231321691639751442098584699687552910487840454326648213393607260249141273724587006606315588174881520920962829254091715364367892590360011330530548820466521384146951941511609433057270365759591953092186117381932611793105118548074462379962749567351885752724891227938183011949128831426076896280457
#define LONG_LONG_MAX (0x7FFFFFFFFFFFFFFFLL)      // 最大的long long整数
#define LONG_LONG_MIN (-0x7FFFFFFFFFFFFFFFLL - 1) // 最小的long long整数Add commentMore actions
#define UINT64_MAX    (0xFFFFFFFFFFFFFFFFULL)     // 最大的unsigned long long整数
#define UINT64_MIN    (0x0000000000000000ULL)     // 最小的unsigned long long整数
#define INT64_MAX     (0x7FFFFFFFFFFFFFFFLL)      // 最大的signed long long整数
#define INT64_MIN     (-0x7FFFFFFFFFFFFFFFLL - 1) // 最小的signed long long整数
#define UINT32_MAX    (0xFFFFFFFFUL)              // 最大的unsigned int整数
#define UINT32_MIN    (0x00000000UL)              // 最小的unsigned int整数
#define INT32_MAX     (0x7FFFFFFFUL)              // 最大的signed int整数
#define INT32_MIN     (-0x7FFFFFFFUL - 1)         // 最小的signed int整数
#define UINT16_MAX    (0xFFFFU)                   // 最大的unsigned short整数
#define UINT16_MIN    (0x0000U)                   // 最小的unsigned short整数
#define INT16_MAX     (0x7FFF)                    // 最大的signed short整数
#define INT16_MIN     (-0x7FFF - 1)               // 最小的signed short整数
#define UINT8_MAX     (0xFFU)                     // 最大的unsigned char整数
#define UINT8_MIN     (0x00U)                     // 最小的unsigned char整数
#define INT8_MAX      (0x7F)                      // 最大的signed char整数
#define INT8_MIN      (-0x7F - 1)                 // 最小的signed char整数
#define SHRT_MAX      (0x7FFF)                    // 最大的short整数
#define SHRT_MIN      (-0x7FFF - 1)               // 最小的short整数
#define INT_MAX       (0x7FFFFFFF)                // 最大的int整数
#define INT_MIN       (-0x7FFFFFFF - 1)           // 最小的int整数
#define UINT_MAX      (0xFFFFFFFFU)               // 最大的unsigned int整数
#define UINT_MIN      (0x00000000U)               // 最小的unsigned int整数
#define LONG_MAX      (0x7FFFFFFFFFFFFFFFL)       // 最大的long整数
#define LONG_MIN      (-0x7FFFFFFFFFFFFFFFL - 1)  // 最小的long整数

#define FORCE_EVAL(x)                                                                              \
    do {                                                                                           \
        if (sizeof(x) == sizeof(float)) {                                                          \
            volatile float __x __attribute__((unused));                                            \
            __x = (x);                                                                             \
        } else if (sizeof(x) == sizeof(double)) {                                                  \
            volatile double __x __attribute__((unused));                                           \
            __x = (x);                                                                             \
        } else {                                                                                   \
            volatile long double __x __attribute__((unused));                                      \
            __x = (x);                                                                             \
        }                                                                                          \
    } while (0)

int32_t equal(float64_t elem1, float64_t elem2) {
    if ((elem1 - elem2 < 0.0000001) && (elem1 - elem2 > -0.0000001))
        return 1; // 相等
    else
        return 0; // 不相等
}

float64_t power_unsigned_exp(float64_t base, uint32_t exponent) {
    float64_t result = 0.0;

    if (0 == exponent) return 1.0;
    if (1 == exponent) return base;

    result = power_unsigned_exp(base, exponent / 2);
    if (exponent & 1 == 1) result *= base;

    return result;
}

float64_t pow(float64_t base, int32_t exponent) {
    uint32_t  abs_exponent  = exponent;
    float64_t result        = 0.0;
    bool      invalid_input = 0;

    if (equal(base, 0.0) && exponent < 0) {
        invalid_input = 1;
        return 0.0;
    }

    if (exponent < 0) abs_exponent = (uint32_t)(-exponent);

    result  = power_unsigned_exp(base, abs_exponent >> 1);
    result *= result;

    if (result < 0) result = 1.0 / result;

    return result;
}

double sqrt(double x) {
    double result;
    __asm__("fsqrt" : "=t"(result) : "0"(x));
    return result;
}

double sin(double x) {
    double result;
    __asm__("fsin" : "=t"(result) : "0"(x));
    return result;
}

double cos(double x) {
    double result;
    __asm__("fcos" : "=t"(result) : "0"(x));
    return result;
}
double acos(double x) {
    double result;
    __asm__("fld1;"        // 将1加载到栈顶
            "fld %1;"      // 将x加载到栈顶
            "fsubp;"       // 计算栈顶两数的差，即 1 - x
            "fsqrt;"       // 计算平方根
            "fld %1;"      // 再次将x加载到栈顶
            "fpatan;"      // 计算反正切
            : "=t"(result) // 输出
            : "m"(x)       // 输入
            : "st(1)"      // 栈顶寄存器被修改
    );
    return result;
}
unsigned int abs(int value) {
    unsigned int copyed_value = value;
    return (copyed_value > 0x80000000) ? -value : copyed_value;
}

double fabs(double x) {
    return (x < 0.0) ? -x : x;
}
double floor(double x) {
    if (x >= 0) {
        return (int)(x);
    } else {
        return (int)(x)-1;
    }
}
double ceil(double x) {
    if (x >= 0) {
        return (int)(x) + (x != (int)(x));
    } else {
        return (int)(x);
    }
}

double fmod(double x, double y) {
    double result;
    __asm__("fld %2;"        // 将y加载到栈顶
            "fld %1;"        // 将x加载到栈顶
            "fprem;"         // 计算x除以y的余数
            "fstp %0;"       // 将结果存储到result并弹出栈顶
            : "=m"(result)   // 输出
            : "m"(x), "m"(y) // 输入
    );
    return result;
}

double ldexp(double x, int exp) {
    int n = 2;
    for (int i = 0; i < exp - 1; i++) {
        n *= 2;
    }
    return x * (double)(n);
}
