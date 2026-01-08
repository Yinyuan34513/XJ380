#define DRAW_CPP
#include <efi/efi.h>
#include <efi/fbc.h>
#include <graphics/GOP.hpp>
#include <graphics/sheet.h>
#include <mm/alloc/alloc.h>
#include <proto.hpp>

EFI_GUID gEfiSimpleFileSystemProtocolGuid = {
    0x824e5c22, 0x6469, 0x1002, {0x81, 0x19, 0x20, 0xe1, 0xc0, 0x66, 0xe2, 0x3a}
};
EFI_GUID gEfiFileInfoGuid = {
    0x5c4e2f81, 0x7b4a, 0x4d03, {0xa3, 0x9f, 0x4d, 0x56, 0x9e, 0x1a, 0x3f, 0x78}
};
#define NULL 0
// auto operator new(size_t size, void *ptr) -> void *
// {
//     ptr = malloc(size);

//     return ptr;
// }

// auto operator new[](size_t size, void *ptr) -> void *
// {
//     ptr = malloc(size);

//     return ptr;
// }

// auto operator delete(void *ptr, size_t size) -> void
// {

//     free(ptr);
// }

// auto operator delete[](void *ptr, size_t size) -> void
// {

//     free(ptr);
// }

// auto operator delete(void *ptr) -> void
// {

//     free(ptr);
// }

void dot(const FrameBufferConfig &fbc, int x, int y, const PixelColor &c)
{
    bool USERGB = true;
    switch (fbc.pixel_format)
    {
    case PixelFormat::kRGBR: USERGB = true; break;
    case PixelFormat::kBGRR: USERGB = false; break;
    default: break;
    }

    if (USERGB)
        WriteRGBR(x, y, c, fbc);
    else
        WriteBGRR(x, y, c, fbc);
    return;
}

void rect(const FrameBufferConfig &fbc, int x1, int y1, int x2, int y2, const PixelColor &c)
{
    bool USERGB = true;
    switch (fbc.pixel_format)
    {
    case PixelFormat::kRGBR: USERGB = true; break;
    case PixelFormat::kBGRR: USERGB = false; break;
    default: break;
    }
    for (int x = x1; x < x2; x++)
    {
        for (int y = y1; y < y2; y++)
        {
            if (USERGB)
                WriteRGBR(x, y, c, fbc);
            else
                WriteBGRR(x, y, c, fbc);
        }
    }
    return;
}
