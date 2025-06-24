#define DRAW_CPP
#include "../include/efi.h"
#include "../include/include.h"

#if BUILD_VERSION == DEBUG_VERSION
extern bool MemoryWasInit;
#endif

EFI_GUID gEfiSimpleFileSystemProtocolGuid = {
    0x824e5c22, 0x6469, 0x1002, {0x81, 0x19, 0x20, 0xe1, 0xc0, 0x66, 0xe2, 0x3a}
};
EFI_GUID gEfiFileInfoGuid = {
    0x5c4e2f81, 0x7b4a, 0x4d03, {0xa3, 0x9f, 0x4d, 0x56, 0x9e, 0x1a, 0x3f, 0x78}
};
#define NULL 0
auto operator new(size_t size, void *ptr) -> void * {
#if BUILD_VERSION == DEBUG_VERSION
    if (MemoryWasInit) {
#endif

        ptr = malloc(size);

#if BUILD_VERSION == DEBUG_VERSION
    }
#endif
    return ptr;
}

auto operator new[](size_t size, void *ptr) -> void * {
#if BUILD_VERSION == DEBUG_VERSION
    if (MemoryWasInit) {
#endif

        ptr = malloc(size);

#if BUILD_VERSION == DEBUG_VERSION
    }
#endif
    return ptr;
}

auto operator delete(void *ptr, size_t size) -> void {
#if BUILD_VERSION == DEBUG_VERSION
    if (MemoryWasInit) {
#endif

        free(ptr);

#if BUILD_VERSION == DEBUG_VERSION
    }
#endif
}

auto operator delete[](void *ptr, size_t size) -> void {
#if BUILD_VERSION == DEBUG_VERSION
    if (MemoryWasInit) {
#endif

        free(ptr);

#if BUILD_VERSION == DEBUG_VERSION
    }
#endif
}

void dot(const FrameBufferConfig &fbc, int x, int y, const PixelColor &c) {
    static char pixel_writer_buf[sizeof(RGBRWriter) > sizeof(BGRRWriter) ? sizeof(RGBRWriter)
                                                                         : sizeof(BGRRWriter)];
    static PixelWriter *pixel_writer = nullptr;

    switch (fbc.pixel_format) {
    case PixelFormat::kRGBR:
        pixel_writer = new (pixel_writer_buf) RGBRWriter(fbc); // 使用 placement new
        break;
    case PixelFormat::kBGRR: pixel_writer = new (pixel_writer_buf) BGRRWriter(fbc); break;
    default: break;
    }

    pixel_writer->Write(x, y, c);
    return;
}

void rect(const FrameBufferConfig &fbc, int x1, int y1, int x2, int y2, const PixelColor &c) {
    static char pixel_writer_buf[sizeof(RGBRWriter) > sizeof(BGRRWriter) ? sizeof(RGBRWriter)
                                                                         : sizeof(BGRRWriter)];
    static PixelWriter *pixel_writer = nullptr;

    switch (fbc.pixel_format) {
    case PixelFormat::kRGBR:
        pixel_writer = new (pixel_writer_buf) RGBRWriter(fbc); // 使用 placement new
        break;
    case PixelFormat::kBGRR: pixel_writer = new (pixel_writer_buf) BGRRWriter(fbc); break;
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
