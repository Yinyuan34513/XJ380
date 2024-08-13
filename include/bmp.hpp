#ifndef _USEFUL_FUNCTIONS_HPP_
#define _USEFUL_FUNCTIONS_HPP_
//理论来说需要stdint.h,但include.h里有了
//所以务必include include.h

//bmp支持
// 定义 BITMAPFILEHEADER 结构体
typedef struct BITMAPFILEHEADER {
    uint16_t bfType;        // 文件类型，通常是 'BM' (0x4D42)
    uint32_t bfSize;       // 文件大小
    uint16_t bfReserved1;   // 保留字段
    uint16_t bfReserved2;   // 保留字段
    uint32_t bfOffBits;    // 位图数据的偏移量
};

// 定义 BITMAPINFOHEADER 结构体
typedef struct BITMAPINFOHEADER {
    uint32_t biSize;       // 结构体的大小
    uint32_t  biWidth;      // 图像宽度
    uint32_t  biHeight;     // 图像高度
    uint16_t  biPlanes;     // 颜色平面数
    uint16_t  biBitCount;   // 每像素位数
    uint32_t biCompression;// 压缩方法
    uint32_t biSizeImage;  // 图像大小
    long  biXPelsPerMeter; // 水平分辨率
    long biYPelsPerMeter; // 垂直分辨率
    uint32_t biClrUsed;    // 使用的颜色数
    uint32_t biClrImportant;// 重要的颜色数
};
#endif