#include <proto.hpp>
#include <font.h>
spin_t fb_print_lock;
auto operator new(size_t size, void *ptr) -> void *;
auto operator new[](size_t size, void *ptr) -> void *;
auto operator delete(void *ptr, size_t size) -> void;
auto operator delete[](void *ptr, size_t size) -> void;
extern const UINT8 _binary___font_hankaku_bin_start;
extern const UINT8 _binary___font_hankaku_bin_end;
extern const UINT8 _binary___font_hankaku_bin_size; // 将三个常量声明
typedef size_t uintptr_t;

const UINT8 *GetFont(char c)
{
    auto index = 16 * static_cast<unsigned int>(c); // 每一个字符要对应16个char作为字体
    if (index >= reinterpret_cast<uintptr_t>(&_binary___font_hankaku_bin_size))
        return nullptr;                               // 指针指到字体外面去，返回空指针
    return &_binary___font_hankaku_bin_start + index; // 取址获得首地址，加上索引返回
}

void WriteAscii(const FrameBufferConfig &fbc, int x, int y, char ch, const PixelColor &c)
{
    static char pixel_writer_buf[sizeof(RGBRWriter) > sizeof(BGRRWriter) ? sizeof(RGBRWriter) : sizeof(BGRRWriter)];
    static PixelWriter *pixel_writer = nullptr;

    switch (fbc.pixel_format)
    {
    case PixelFormat::kRGBR:
        pixel_writer = new (pixel_writer_buf) RGBRWriter(fbc); // 使用 placement new
        break;
    case PixelFormat::kBGRR:
        pixel_writer = new (pixel_writer_buf) BGRRWriter(fbc);
        break;
    default:
        break;
    }

    const UINT8 *font = GetFont(ch); // 获取字体
    if (font == nullptr)
        return; // 不存在字体，直接退出
    for (int dy = 0; dy < 16; dy++)
    { // 第几行
        for (int dx = 0; dx < 16; dx++)
        { // 第几列
            if ((font[dy] << dx) & 0x80u)
            {                                           // 判断第dx位是否为1
                pixel_writer->Write(x + dx, y + dy, c); // 在x和y的基础上，偏移dx,dy绘制像素
            }
        }
    }
}

void WriteDec(const FrameBufferConfig &fbc, int x, int y, unsigned long long dec, const PixelColor &c)
{
    char str[255];
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
    WriteString(fbc, x, y, str, c);
}

void WriteString(const FrameBufferConfig &fbc, int x, int y, const char *s, const PixelColor &c)
{
    for (int i = 0; s[i]; i++)
    {                                           // 遍历字符串每一个字符，看看是不是空字符
        WriteAscii(fbc, x + 8 * i, y, s[i], c); // 以(x,y)为基础，偏移i个字符（字体8*16，即i*8个像素）画字符
    }
}

void WriteHex(const FrameBufferConfig &fbc, int x, int y, unsigned long long hex, const PixelColor &c)
{
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
        {                          // 每次4位
            ch = (hex >> i) & 0xF; // 0~9, A~F
            ch += '0';             // 0~9 => '0'~'9'
            if (ch > '9')
            {
                ch += 7; // 'A' - '9' = 7
            }
            *p++ = ch; // 写入
        }
    }
    *p = '\0';                      // 结束符
    WriteString(fbc, x, y, buf, c); // 打印
}

// 下面是图层专属
void PrintFont(SHEET_INFO *sht, UINT16 *sheet_id, int x, int y, char ch, const SHEET_BUFFER &color)
{
    const UINT8 *font = GetFont(ch); // 获取字体
    if (font == nullptr)
        return; // 不存在字体，直接退出
    for (int dy = 0; dy < 16; dy++)
    { // 第几行
        for (int dx = 0; dx < 16; dx++)
        { // 第几列
            if ((font[dy] << dx) & 0x80u)
            {                                                     // 判断第dx位是否为1
                draw_point(sht, sheet_id, x + dx, y + dy, color); // 在x和y的基础上，偏移dx,dy绘制像素
            }
        }
    }
}

void PrintString(SHEET_INFO *sht, UINT16 *sheet_id, int x, int y, const char *s, const SHEET_BUFFER &color)
{
    spin_lock(fb_print_lock);
    for (int i = 0; s[i]; i++)
    {                                                        // 遍历字符串每一个字符，看看是不是空字符
        PrintFont(sht, sheet_id, x + 8 * i, y, s[i], color); // 以(x,y)为基础，偏移i个字符（字体8*16，即i*8个像素）画字符
    }
    spin_unlock(fb_print_lock);
}

void PrintHex(SHEET_INFO *sht, UINT16 *sheet_id, int x, int y, unsigned long long hex, const SHEET_BUFFER &color)
{
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
    *p = '\0';                                    // 结束符
    PrintString(sht, sheet_id, x, y, buf, color); // 打印
}

void PrintDec(SHEET_INFO *sht, UINT16 *sheet_id, int x, int y, unsigned long long dec, const SHEET_BUFFER &color)
{
    char str[255];
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
    PrintString(sht, sheet_id, x, y, str, color);
}
