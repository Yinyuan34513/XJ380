#include "../include/include.h"
auto operator new(size_t size, void *ptr) -> void*;
auto operator new[](size_t size, void *ptr) -> void*;
auto operator delete(void *ptr, size_t size) -> void;
auto operator delete[](void *ptr, size_t size) -> void;
extern const uint8_t _binary_hankaku_bin_start;
extern const uint8_t _binary_hankaku_bin_end;
extern const uint8_t _binary_hankaku_bin_size; // 将三个常量声明
typedef size_t uintptr_t;
const uint8_t *GetFont(char c)
{
    auto index = 16 * static_cast<unsigned int>(c); // 每一个字符要对应16个char作为字体
    if (index >= reinterpret_cast<uintptr_t>(&_binary_hankaku_bin_size)) return nullptr; // 指针指到字体外面去，返回空指针
    return &_binary_hankaku_bin_start + index; // 取址获得首地址，加上索引返回
}

void WriteAscii(const FrameBufferConfig &fbc, int x, int y, char ch, const PixelColor &c)
{
    static char pixel_writer_buf[sizeof(RGBRWriter) > sizeof(BGRRWriter) ? sizeof(RGBRWriter) : sizeof(BGRRWriter)];  
    static PixelWriter* pixel_writer = nullptr;  
  
    switch (fbc.pixel_format) {  
        case PixelFormat::kRGBR:  
            pixel_writer = new(pixel_writer_buf) RGBRWriter(fbc); // 使用 placement new  
            break;  
        case PixelFormat::kBGRR:  
            pixel_writer = new(pixel_writer_buf) BGRRWriter(fbc);  
            break;  
        default:  
            break;  
    } 

    const uint8_t *font = GetFont(ch); // 获取字体
    if (font == nullptr) return; // 不存在字体，直接退出
    for (int dy = 0; dy < 16; dy++) { // 第几行
        for (int dx = 0; dx < 16; dx++) { // 第几列
            if ((font[dy] << dx) & 0x80u) { // 判断第dx位是否为1
                pixel_writer->Write(x + dx, y + dy, c); // 在x和y的基础上，偏移dx,dy绘制像素
            }
        }
    }
}

void WriteString(const FrameBufferConfig &fbc, int x, int y, const char *s, const PixelColor &c)
{
    for (int i = 0; s[i]; i++) { // 遍历字符串每一个字符，看看是不是空字符
        WriteAscii(fbc, x + 8 * i, y, s[i], c); // 以(x,y)为基础，偏移i个字符（字体8*16，即i*8个像素）画字符
    }
}

void WriteHex(const FrameBufferConfig &fbc, int x, int y, unsigned long long hex, const PixelColor &c)
{
    // 输出16进制
    unsigned short  buf[20];
    unsigned short *p = buf;
    unsigned short  ch;
    int             i, flag = 0;

    *p++ = L'0';
    *p++ = L'x'; // 先存一个0x

    if (hex == 0)
        *p++ = L'0'; // 如果是0，直接0x0趋势
    else {
        for (i = 28; i >= 0; i -= 4) { // 每次4位
        ch = (hex >> i) & 0xF;       // 0~9, A~F
        // 28（冗余）
        if (flag || ch > 0) { // 跳过前导0
            flag  = 1;          // 没有前导0就把flag设为1，这样后面再有0也不会忽略
            ch   += L'0';       // 0~9 => '0'~'9'
            if (ch > L'9') {
            ch += 7; // 'A' - '9' = 7
            }
            *p++ = ch; // 写入
        }
        }
    }
    *p = L'\0'; // 结束符
    WriteString(fbc, x, y, (char *)buf, c);  // 打印
}

