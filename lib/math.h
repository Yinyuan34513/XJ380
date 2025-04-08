#pragma once
#include "../include/include.h"
//哦，我们亲爱的派
#define PI (float64_t)3.1415926535 //搞笑版:3.14159265358979323846264338327950288419716939937510582097494459230781640628620899
#define TAU (float64_t)6.2831853071 //搞笑版:6.28318530717958647692528676655900576839433879875021164194988918553642212019584998
#define PI_DIV_2 (float64_t)1.5707963267 //搞笑版:1.570796326794896619231321691639751442098584699687552910487840454326648213393607260249141273724587006606315588174881520920962829254091715364367892590360011330530548820466521384146951941511609433057270365759591953092186117381932611793105118548074462379962749567351885752724891227938183011949128831426076896280457

int32_t equal(float64_t elem1, float64_t elem2)
{
	if ((elem1 - elem2 < 0.0000001) && (elem1 - elem2 > -0.0000001))
		return 1;	//相等
	else
		return 0;	//不相等
}
 
float64_t power_unsigned_exp(float64_t base, uint32_t exponent)
{
	float64_t result = 0.0;
	
	if (0 == exponent)
		return 1.0;
	if (1 == exponent)
		return base;
 
	result = power_unsigned_exp(base, exponent / 2);
	if (exponent & 1 == 1)
		result *= base;
	
	return result;
}
 
float64_t pow(float64_t base, int32_t exponent)
{
	uint32_t abs_exponent = exponent;
	float64_t result = 0.0;
	bool invalid_input = 0;
	
	if (equal(base, 0.0) && exponent < 0)
	{
		invalid_input = 1;
		return 0.0;
	}
 
	if (exponent < 0)
		abs_exponent = (uint32_t)(-exponent);
	
	result = power_unsigned_exp(base, abs_exponent >> 1);
	result *= result;
 
	if (result < 0)
		result = 1.0 / result;
	
	return result;
}

float32_t sqrt(float32_t x)  
{  
    float32_t xhalf = 0.5f*x;  
    int32_t i = *(int*)&x;  
    i = 0x5f375a86 - (i >> 1);  
    x = *(float32_t*)&i;  
    x = x*(1.5f - xhalf*x*x);  
    x = x*(1.5f - xhalf*x*x);  
    x = x*(1.5f - xhalf*x*x);  
    return 1 / x;  
} 

float sin(float x)  
{  
    int sign = 1;  
    int itemCnt = 4;  
    int k = 0;  
    float result = 0.0f;  
    float tx = 0.0f;  
    int factorial = 1;  
    if(x < 0)  
    {  
        x = -x;  
        sign *= -1;  
    }  
    while(x >= TAU)  
    {  
        x -= TAU;  
    }  
    if(x > PI)  
    {  
        x -= PI;  
        sign *= -1;  
    }  
    if(x > PI_DIV_2)  
    {  
        x = PI - x;  
    }  
    tx = x;  
    for (k = 0; k < itemCnt; k ++)  
    {  
        if(k%2 == 0)  
        {  
            result += (tx / factorial);  
        }  
        else  
        {  
            result -= (tx / factorial);  
        }  
        tx *= (x * x);  
        factorial *= (2*(k+1));  
        factorial *= (2*(k+1) + 1);  
    }  
    if (1 == sign)  
        return result;  
    else  
        return -result;  
}

#define cos(x) sin(PI_DIV_2 - x)

unsigned int abs(int value)
{
    unsigned int copyed_value = value;
    return (copyed_value > 0x80000000) ? -value : copyed_value;
}

double acos_taylor(double x) {

    double sum = 0.0;
    double term = 1.0;
    int n = 0;

    while (abs(term) > 1e-10) {
        term = ((2 * n - 1) * pow(x, 2 * n)) / (2 * n * pow(2, 2 * n) * (n - 1));
        sum += term;
        n++;
    }

    return PI_DIV_2 - sum;
}

double fabs(double x) {
    return (x < 0.0) ? -x : x;
}
double floor(double x) {
    if (x >= 0) {
        return (int)(x);
    } else {
        return (int)(x) - 1;
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
    return x - floor(x / y) * y;
}