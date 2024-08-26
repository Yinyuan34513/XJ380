#define DRAW_CPP
#include "../include/include.h"
#include "../include/efi.h"
struct EFI_GUID gEfiSimpleFileSystemProtocolGuid={0x824e5c22, 0x6469, 0x1002, {0x81, 0x19, 0x20, 0xe1, 0xc0, 0x66, 0xe2, 0x3a}};
struct EFI_GUID gEfiFileInfoGuid=                {0x5c4e2f81, 0x7b4a, 0x4d03, {0xa3, 0x9f, 0x4d, 0x56, 0x9e, 0x1a, 0x3f, 0x78}};
#define NULL 0
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



EFI_STATUS ReadBmpFile(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *gST, CHAR16 *FileName, struct BITMAPFILEHEADER *FileHeader, struct BITMAPINFOHEADER *InfoHeader, RGBQUAD **PixelData, UINT32 *BufferSize)
{
  EFI_STATUS Status = EFI_SUCCESS;
  EFI_FILE_PROTOCOL *RootDir = NULL;
  EFI_FILE_PROTOCOL *FileHandle = NULL;
  UINT32 FileSize = 0;
  void *Buffer = NULL;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem = NULL;

  // 打开根目录
  Status = gST->BootServices->HandleProtocol(ImageHandle, &gEfiSimpleFileSystemProtocolGuid, (VOID **)&FileSystem);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  Status = FileSystem->OpenVolume(FileSystem, &RootDir);
  if (EFI_ERROR(Status)) {
    FileSystem->Close(FileSystem);
    return Status;
  }

  // 打开文件
  Status = RootDir->Open(RootDir, &FileHandle, FileName, EFI_FILE_MODE_READ, 0);
  if (EFI_ERROR(Status)) {
    FileHandle->Close(FileHandle);
    FileSystem->Close(FileSystem);
    return Status;
  }

  // 获取文件大小
  Status = FileHandle->GetInfo(FileHandle, &gEfiFileInfoGuid, &FileSize, NULL, NULL);
  if (EFI_ERROR(Status)) {
    FileHandle->Close(FileHandle);
    FileSystem->Close(FileSystem);
    return Status;
  }

  // 读取文件头部
  Buffer = AllocatePages(PageAllocationGranularity, EfiLoaderData, EFI_SIZE_TO_PAGES(FileSize));
  if (Buffer == NULL) {
    FileHandle->Close(FileHandle);
    FileSystem->Close(FileSystem);
    return EFI_OUT_OF_RESOURCES;
  }

  Status = FileHandle->Read(FileHandle, &FileSize, Buffer);
  if (EFI_ERROR(Status)) {
    FreePages((EFI_PHYSICAL_ADDRESS)Buffer, EFI_SIZE_TO_PAGES(FileSize));
    FileHandle->Close(FileHandle);
    FileSystem->Close(FileSystem);
    return Status;
  }

  // 解析文件头
  CopyMem(FileHeader, Buffer, sizeof(BITMAPFILEHEADER));
  CopyMem(InfoHeader, ((UINT8 *)Buffer + sizeof(BITMAPFILEHEADER)), sizeof(BITMAPINFOHEADER));

  // 计算像素数据的偏移量
  *BufferSize = FileHeader->bfSize - FileHeader->bfOffBits;

  // 分配像素数据的内存
  *PixelData = (RGBQUAD *)AllocatePages(PageAllocationGranularity, EfiLoaderData, EFI_SIZE_TO_PAGES(*BufferSize));
  if (*PixelData == NULL) {
    FreePages((EFI_PHYSICAL_ADDRESS)Buffer, EFI_SIZE_TO_PAGES(FileSize));
    FileHandle->Close(FileHandle);
    FileSystem->Close(FileSystem);
    return EFI_OUT_OF_RESOURCES;
  }

  // 复制像素数据
  CopyMem(*PixelData, ((UINT8 *)Buffer + FileHeader->bfOffBits), *BufferSize);

  // 清理资源
  FreePages((EFI_PHYSICAL_ADDRESS)Buffer, EFI_SIZE_TO_PAGES(FileSize));
  FileHandle->Close(FileHandle);
  FileSystem->Close(FileSystem);

  return EFI_SUCCESS;
}

EFI_STATUS DisplayBmpImage(EFI_GRAPHICS_OUTPUT_PROTOCOL *GraphicsOutput, struct BITMAPINFOHEADER *InfoHeader, RGBQUAD *PixelData, UINT32 XPos, UINT32 YPos,const FrameBufferConfig &fbc)
{
  EFI_STATUS Status = EFI_SUCCESS;
  UINT32 Row, Col;
  
  // Copy the image data using the dot function
  for (Row = 0; Row < InfoHeader->biHeight; Row++) {
    for (Col = 0; Col < InfoHeader->biWidth; Col++) {
      UINT32 DestRow = (InfoHeader->biHeight - Row - 1) + YPos; // Adjust row to bottom-up coordinate system
      UINT32 DestCol = Col + XPos;
      
      // Check if the destination is within the frame buffer bounds
      if (DestRow < fbc.height && DestCol < fbc.width) {
        // Convert RGBQUAD to PixelColor
        PixelColor c(PixelData[Row * InfoHeader->biWidth + Col].r, 
                     PixelData[Row * InfoHeader->biWidth + Col].g, 
                     PixelData[Row * InfoHeader->biWidth + Col].b);

        // Draw the pixel at the specified position
        dot(fbc, DestCol, DestRow, c);
      }
    }
  }

  return Status;
}

