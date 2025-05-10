#include "./include/xtuiapi.h"
#include "./include/libsys.h"

// 3-1
void xapi_Output(WSTR str) {
    enter_syscall(XAPI_OUTPUT, (UINT64)(str), 0, 0, 0, 0, 0);
}

WSTR xapi_InputW() {
    return (WSTR)(enter_syscall(XAPI_INPUTW, 0, 0, 0, 0, 0, 0));
}

WSTR xapi_Input() {
    return (WSTR)(enter_syscall(XAPI_INPUT, 0, 0, 0, 0, 0, 0));
}

void xapi_EndLine() {
    enter_syscall(XAPI_ENDLINE, 0, 0, 0, 0, 0, 0);
}

void xapi_PrintLine(WSTR str) {
    enter_syscall(XAPI_PRINTLINE, (UINT64)(str), 0, 0, 0, 0, 0);
}

// 3-2
void xapi_OpenFile(
	WSTR	path,
	XFILE*	fsptr
) {
    enter_syscall(XAPI_OPEN_FILE, (UINT64)(path), (UINT64)(fsptr), 0, 0, 0, 0);
}

void xapi_CloseFile(XFILE* fsptr) {
    enter_syscall(XAPI_CLOSE_FILE, (UINT64)(fsptr), 0, 0, 0, 0, 0);
} 

// 3-3
UINT64 xcr_char2int(WSTR str) {
    char *p = str;
    UINT64 num = 0;
    int w = 10;
    int len = 0;
    while (*p != '\0') {
        len++;
        p++;
    }
    num += *p - '0';
    p--;
    for (int i = 1; i < len; i++, p--, w *= 10) {
        num += (*p - '0') * w;
    }
}

WSTR xcr_int2char(UINT64 dec) {
    char str[255];
	int radix = 10;
	char index[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"; // 索引表
	unsigned unum;										   // 存放要转换的整数的绝对值,转换的整数可能是负数
	int i = 0, j, k;									   // i用来指示设置字符串相应位，转换之后i其实就是字符串的长度；转换后顺序是逆序的，有正负的情况，k用来指示调整顺序的开始位置;j用来指示调整顺序时的交换。

	// 获取要转换的整数的绝对值
	if (radix == 10 && dec < 0) // 要转换成十进制数并且是负数
	{
		unum = (unsigned)-dec; // 将num的绝对值赋给unum
		str[i++] = '-';		   // 在字符串最前面设置为'-'号，并且索引加1
	}
	else
		unum = (unsigned)dec; // 若是num为正，直接赋值给unum

	// 转换部分，注意转换后是逆序的
	do
	{
		str[i++] = index[unum % (unsigned)radix]; // 取unum的最后一位，并设置为str对应位，指示索引加1
		unum /= radix;							  // unum去掉最后一位

	} while (unum); // 直至unum为0退出循环

	str[i] = '\0'; // 在字符串最后添加'\0'字符，c语言字符串以'\0'结束。

	// 将顺序调整过来
	if (str[0] == '-')
		k = 1; // 如果是负数，符号不用调整，从符号后面开始调整
	else
		k = 0; // 不是负数，全部都要调整

	char temp;						   // 临时变量，交换两个值时用到
	for (j = k; j <= (i - 1) / 2; j++) // 头尾一一对称交换，i其实就是字符串的长度，索引最大值比长度少1
	{
		temp = str[j];				 // 头部赋值给临时变量
		str[j] = str[i - 1 + k - j]; // 尾部赋值给头部
		str[i - 1 + k - j] = temp;	 // 将临时变量的值(其实就是之前的头部值)赋给尾部
	}

	return str;
} 
WSTR xcr_hex2char(UINT64 hex) {
	// 输出16进制
	char buf[32];
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
		{						   // 每次4位
			ch = (hex >> i) & 0xF; // 0~9, A~F
			// 28（冗余）
			if (flag || ch > 0)
			{			   // 跳过前导0
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

	return buf;
}

UINT32 xapi_toRGB(UINT8 r, UINT8 g, UINT8 b) {
	return r << 24 | g << 16 | b << 8 | 0xff;
}

// 3-4
UINT64  xapi_fork() {
	enter_syscall(XAPI_FORK, 0, 0, 0, 0, 0, 0);
}

UINT64  xapi_execve(WSTR filename, WSTR argv[], WSTR envp[]) {
	enter_syscall(XAPI_EXECVE, (uint64_t)filename, (uint64_t)argv, (uint64_t)envp, 0, 0, 0);
}
