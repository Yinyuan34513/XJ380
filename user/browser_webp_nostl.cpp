#include "./xapi/include/x3api.h"
#include "browser_css_nostl_internal.h"
#include "browser/third_party/libwebp/src/webp/decode.h"

static void browsercss_swap_bytes(unsigned char *lhs, unsigned char *rhs, size_t size)
{
    size_t i;

    for (i = 0; i < size; ++i) {
        unsigned char tmp = lhs[i];
        lhs[i] = rhs[i];
        rhs[i] = tmp;
    }
}

extern "C" void qsort(void *base,
                       size_t count,
                       size_t size,
                       int (*compare)(const void *, const void *))
{
    unsigned char *bytes;
    size_t i;

    if (base == NULL || compare == NULL || size == 0 || count < 2) {
        return;
    }

    bytes = (unsigned char *)base;
    for (i = 1; i < count; ++i) {
        size_t j = i;
        while (j > 0) {
            unsigned char *current = bytes + j * size;
            unsigned char *previous = current - size;
            if (compare(previous, current) <= 0) {
                break;
            }
            browsercss_swap_bytes(previous, current, size);
            --j;
        }
    }
}

int browsercss_decode_webp_image(const char *data,
                                 int data_len,
                                 XCOLORA **pixels_out,
                                 int *width_out,
                                 int *height_out)
{
    int width = 0;
    int height = 0;
    XCOLORA *pixels;
    uint64_t total_pixels;
    uint64_t total_bytes;

    if (data == NULL || data_len <= 0 || pixels_out == NULL || width_out == NULL || height_out == NULL) {
        return 0;
    }

    if (WebPGetInfo((const uint8_t *)data, (size_t)data_len, &width, &height) == 0 ||
        width <= 0 || height <= 0) {
        return 0;
    }

    total_pixels = (uint64_t)width * (uint64_t)height;
    total_bytes = total_pixels * sizeof(XCOLORA);
    if (total_pixels == 0 || total_bytes / sizeof(XCOLORA) != total_pixels) {
        return 0;
    }

    pixels = (XCOLORA *)malloc((size_t)total_bytes);
    if (pixels == NULL) {
        return 0;
    }

    if (WebPDecodeRGBAInto((const uint8_t *)data,
                           (size_t)data_len,
                           (uint8_t *)pixels,
                           (size_t)total_bytes,
                           width * (int)sizeof(XCOLORA)) == NULL) {
        free(pixels);
        return 0;
    }

    *pixels_out = pixels;
    *width_out = width;
    *height_out = height;
    return 1;
}
