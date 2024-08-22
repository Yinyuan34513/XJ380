#include "../include/include.h"
#include "../include/efi.h"
struct EFI_GUID gEfiSimpleFileSystemProtocolGuid={0x824e5c22, 0x6469, 0x1002, {0x81, 0x19, 0x20, 0xe1, 0xc0, 0x66, 0xe2, 0x3a}};

auto operator new(size_t size, void *ptr) -> void * {
  return ptr;
}

auto operator new[](size_t size, void *ptr) -> void * {
  return ptr;
}

auto operator delete(void *ptr, size_t size) -> void { 
}

auto operator delete[](void *ptr, size_t size) -> void { 
}


void dot(const FrameBufferConfig &fbc, int x, int y, const PixelColor& c){
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
   
    pixel_writer->Write(x, y, c);
    return;
}

void rect(const FrameBufferConfig &fbc, int x1, int y1, int x2, int y2, const PixelColor& c)
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
        // ... 处理其他像素格式 ...  
        default:  
            // 错误处理  
            break;  
    }
    for (int x = x1; x < x2; x++) {  
        for (int y = y1; y < y2; y++) {  
            pixel_writer->Write(x, y, c);
        }  
    }  
    return;
}
void line(const FrameBufferConfig &fbc, int x0, int y0, int x1, int y1, const PixelColor& c){
    int dx = x1 - x0;  
    int dy = y1 - y0;  
    int k;  
    int xIncrement;  
    int err;  
  
    // 确定迭代方向和步数  
    if (dx < 0) {  
        dx = -dx;  
        xIncrement = -1;  
    } else {  
        xIncrement = 1;  
    }  
  
    // 初始化斜率（2*dy，因为我们后面会乘以2来避免浮点运算）  
    k = 2 * dy;  
  
    // 初始化误差项和y的增量  
    err = k - dx;  
    int y = y0; 
  
    // 绘制直线上的点  
    for (int x = x0; x != x1 + xIncrement; x += xIncrement) {  
        // 这里可以添加代码来绘制点(x, y)  
        dot(fbc,x,y,c);
  
        // 更新误差项  
        if (err < 0) {  
            err += 2 * dy;  
        } else {  
            y += (y1 > y0) ? 1 : -1;  
            err += 2 * (dy - dx);  
        }  
    }  
}



void circle(const FrameBufferConfig &fbc,int centerX, int centerY, int radius,int solid,const PixelColor& c) {  
    static char pixel_writer_buf[sizeof(RGBRWriter) > sizeof(BGRRWriter) ? sizeof(RGBRWriter) : sizeof(BGRRWriter)];  
    static PixelWriter* pixel_writer = nullptr;  
  
    switch (fbc.pixel_format) {  
        case PixelFormat::kRGBR:  
            pixel_writer = new(pixel_writer_buf) RGBRWriter(fbc); // 使用 placement new  
            break;  
        case PixelFormat::kBGRR:  
            pixel_writer = new(pixel_writer_buf) BGRRWriter(fbc);  
            break;  
        // ... 处理其他像素格式 ...  
        default:  
            // 错误处理  
            break;  
    } 
    if (solid){
        int x = 0;  
        int y = radius;  
        int p = 1 - radius;  
    
        while (x <= y) {  
            // 绘制八个对称的点（考虑到圆的八对称性）  
            for (int dx = -x; dx <= x; ++dx) {  
                for (int dy = -y; dy <= y; dy += (dy == 0) ? 1 : 2) { // 避免在y=0时重复绘制  
                    pixel_writer->Write(centerX + dx, centerY + dy, c);  
                    pixel_writer->Write(centerX - dx, centerY + dy, c);  
                    pixel_writer->Write(centerX + dx, centerY - dy, c);  
                    pixel_writer->Write(centerX - dx, centerY - dy, c);  
                    
                    // 如果不是圆的最上方或最下方，则还需要绘制另外两个对称点  
                    if (dy != y && dy != -y) {  
                        pixel_writer->Write(centerX + dy, centerY + dx, c);  
                        pixel_writer->Write(centerX - dy, centerY + dx, c);  
                        pixel_writer->Write(centerX + dy, centerY - dx, c);  
                        pixel_writer->Write(centerX - dy, centerY - dx, c);  
                    }  
                }  
            }  
    
            // 更新p, x, y的值  
            if (p < 0) {  
                p += 2 * x + 3;  
            } else {  
                p += 2 * (x - y) + 5;  
                y--;  
            }  
            x++;  
        }    
    }
    else{
        int x = 0;  
        int y = radius;  
        int p = 1 - radius; 
        while (x <= y) {  
                // 根据圆的对称性，我们只需要计算第一象限的点，然后镜像到其他象限  
                pixel_writer->Write(centerX + x, centerY + y, c); // 第一象限  
                pixel_writer->Write(centerX - x, centerY + y, c); // 第二象限  
                pixel_writer->Write(centerX + x, centerY - y, c); // 第四象限  
                pixel_writer->Write(centerX - x, centerY - y, c); // 第三象限  
                pixel_writer->Write(centerX + y, centerY + x, c); // 第一象限的45度线  
                pixel_writer->Write(centerX - y, centerY + x, c); // 第二象限的45度线  
                pixel_writer->Write(centerX + y, centerY - x, c); // 第四象限的45度线  
                pixel_writer->Write(centerX - y, centerY - x, c); // 第三象限的45度线  
    
            // 根据中点圆算法更新 p 和 x, y 的值  
            if (p < 0) {  
                p += 2 * x + 3;  
            } else {  
                p += 2 * (x - y) + 5;  
                y--;  
            }  
            x++;  
        }  
    }
}  

#define NULL 0

// BMP 像素数据结构
struct BMP_PIXEL {
    UINT8 b;
    UINT8 g;
    UINT8 r;
};

EFI_STATUS LoadBMPFile(CHAR16* filename, BITMAPINFOHEADER* infoHeader, BMP_PIXEL** pixels, struct EFI_SYSTEM_TABLE* ST)
{
    EFI_STATUS Status;
    struct EFI_FILE_PROTOCOL* file;
    struct EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* fs;
    VOID* buffer = NULL;
    UINT64 bufferSize = 0;
    BITMAPFILEHEADER fileHeader;

    Status = ST->BootServices->LocateProtocol(&gEfiSimpleFileSystemProtocolGuid, NULL, (VOID**)&fs);
    if (Status != EFI_SUCCESS) {
        return Status;
    }
    
    Status = fs->OpenVolume(fs, &file);
    if (Status != EFI_SUCCESS) {
        return Status;
    }
 
    Status = file->Open(file, &file, filename, EFI_FILE_MODE_READ, 0);
    if (Status != EFI_SUCCESS) {
        return Status;
    }

    // 读取文件头
    Status = file->Read(file, &bufferSize, &fileHeader);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    // 读取信息头
    Status = file->Read(file, &bufferSize, infoHeader);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    // 重置缓冲区大小
    bufferSize = 0;

    // 读取像素数据
    Status = ST->BootServices->AllocatePool(EfiLoaderData, infoHeader->biSizeImage, (VOID**)&buffer);
    if (Status != EFI_SUCCESS) {
        return Status;
    }

    Status = file->Read(file, &bufferSize, buffer);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    *pixels = (BMP_PIXEL*)buffer;
    file->Close(file);

    return EFI_SUCCESS;
}

EFI_STATUS ShowBMP(BMP_PIXEL* pixels, BITMAPINFOHEADER* infoHeader, const FrameBufferConfig& fbc, UINT32 offsetX, UINT32 offsetY)
{
    // 绘制像素
    UINT32 pixel;
    UINTN i, j;

    for (j = 0; j < infoHeader->biHeight; j++) {
        for (i = 0; i < infoHeader->biWidth; i++) {
            BMP_PIXEL* pixelData = &pixels[(infoHeader->biHeight - j - 1) * infoHeader->biWidth + i];
            // 创建 PixelColor 结构体
            PixelColor color = {
                .r = pixelData->r,
                .g = pixelData->g,
                .b = pixelData->b
            };
            // 调用 dot 函数绘制单个像素
            dot(fbc, i + offsetX, j + offsetY, color);
        }
    }

    return EFI_SUCCESS;
}

void bmp(const FrameBufferConfig& fbc, struct EFI_SYSTEM_TABLE* ST, CHAR16 filename[], const UINT32 offsetX_, const UINT32 offsetY_)
{
    BMP_PIXEL* pixels = nullptr;
    BITMAPINFOHEADER infoHeader;

    EFI_STATUS status = LoadBMPFile(filename, &infoHeader, &pixels, ST);
    if (status != EFI_SUCCESS) {
        // Handle error
        return;
    }

    ShowBMP(pixels, &infoHeader, fbc, offsetX_, offsetY_);

    // Free allocated memory
    ST->BootServices->FreePool(pixels);
}