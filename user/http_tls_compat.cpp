#include "krlibc.h"
#include "libsys.h"
#include "xposix/stdio.h"
#include "liballoc/alloc.h"
#include "mbed_compat/time.h"
#include <psa/crypto.h>
#include <psa/crypto_platform.h>

extern int xj380_snprintf(char *str, size_t size, const char *format, ...)
    asm("_Z8snprintfPcyPKcz");
extern int xj380_vsnprintf(char *str, size_t size, const char *format, va_list ap)
    asm("_Z9vsnprintfPcyPKcP13__va_list_tag");
int c_snprintf(char *str, size_t maxlen, const char *fmt, ...) asm("snprintf");
int c_vsnprintf(char *str, size_t maxlen, const char *fmt, va_list ap) asm("vsnprintf");

struct tls_timespec
{
    int64_t tv_sec;
    int64_t tv_nsec;
};

struct tls_stat
{
    uint64_t st_dev;
    uint64_t st_ino;
    uint64_t st_nlink;
    uint32_t st_mode;
};

extern "C" void *memmove(void *dest, const void *src, size_t n)
{
    uint8_t       *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;

    if (d == s || n == 0) return dest;

    if (d < s) {
        for (size_t i = 0; i < n; ++i) d[i] = s[i];
    } else {
        for (size_t i = n; i > 0; --i) d[i - 1] = s[i - 1];
    }

    return dest;
}

extern "C" void abort(void)
{
    for (;;) {}
}

extern "C" void *__memcpy_chk(void *dest, const void *src, size_t len, size_t destlen)
{
    if (len > destlen) len = destlen;
    return memcpy(dest, src, len);
}

extern "C" void *__memset_chk(void *dest, int ch, size_t len, size_t destlen)
{
    if (len > destlen) len = destlen;
    return memset(dest, ch, len);
}

extern "C" int __snprintf_chk(char *str, size_t maxlen, int, size_t, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = xj380_vsnprintf(str, maxlen, fmt, ap);
    va_end(ap);
    return ret;
}

extern "C" int __vsnprintf_chk(char *str, size_t maxlen, int, size_t, const char *fmt, va_list ap)
{
    return xj380_vsnprintf(str, maxlen, fmt, ap);
}

int c_snprintf(char *str, size_t maxlen, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = xj380_vsnprintf(str, maxlen, fmt, ap);
    va_end(ap);
    return ret;
}

int c_vsnprintf(char *str, size_t maxlen, const char *fmt, va_list ap)
{
    return xj380_vsnprintf(str, maxlen, fmt, ap);
}

extern "C" int __printf_chk(int, const char *fmt, ...)
{
    char    buffer[512];
    va_list ap;
    va_start(ap, fmt);
    int ret = xj380_vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);

    if (ret > 0) {
        int write_len = ret;
        if (write_len >= (int)sizeof(buffer)) write_len = (int)sizeof(buffer) - 1;
        write(1, buffer, (uint64_t)write_len);
    }

    return ret;
}

extern "C" int printf(const char *fmt, ...)
{
    char    buffer[512];
    va_list ap;
    va_start(ap, fmt);
    int ret = xj380_vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);

    if (ret > 0) {
        int write_len = ret;
        if (write_len >= (int)sizeof(buffer)) write_len = (int)sizeof(buffer) - 1;
        write(1, buffer, (uint64_t)write_len);
    }

    return ret;
}

extern "C" int *__errno_location(void)
{
    static int g_errno = 0;
    return &g_errno;
}

static inline uint64_t tls_rdtsc()
{
    uint32_t lo = 0;
    uint32_t hi = 0;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static uint64_t tls_mix64(uint64_t x)
{
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    return x * 2685821657736338717ULL;
}

extern "C" time_t time(time_t *timer)
{
    time_t now = (time_t)(tls_rdtsc() / 1000000ULL);
    if (timer != NULL) *timer = now;
    return now;
}

extern "C" int clock_gettime(int, struct tls_timespec *ts)
{
    if (ts == NULL) return -1;

    uint64_t ret = enter_syscall(SYS_CLOCK_GETTIME, 0, (uint64_t)ts, 0, 0, 0, 0);
    return (int)ret;
}

extern "C" struct tm *gmtime_r(const time_t *timer, struct tm *result)
{
    if (result == NULL) return NULL;
    memset(result, 0, sizeof(*result));
    if (timer != NULL) result->tm_sec = (int)(*timer % 60);
    return result;
}

extern "C" void setbuf(FILE *, char *)
{
}

extern "C" size_t fwrite(const void *, size_t size, size_t count, FILE *)
{
    return size * count;
}

extern "C" char *fgets(char *, int, FILE *)
{
    return NULL;
}

extern "C" int remove(const char *)
{
    return -1;
}

extern "C" int rename(const char *, const char *)
{
    return -1;
}

extern "C" void *opendir(const char *)
{
    return NULL;
}

extern "C" void *readdir(void *)
{
    return NULL;
}

extern "C" int closedir(void *)
{
    return -1;
}

extern "C" int stat(const char *, struct tls_stat *)
{
    return -1;
}

extern "C" int inet_pton(int af, const char *src, void *dst)
{
    if (af != 2 || src == NULL || dst == NULL) return 0;

    uint32_t parts[4] = {0, 0, 0, 0};
    int      part = 0;
    uint32_t value = 0;

    for (const char *p = src;; ++p) {
        char ch = *p;
        if (ch >= '0' && ch <= '9') {
            value = value * 10 + (uint32_t)(ch - '0');
            if (value > 255) return 0;
        } else if (ch == '.' || ch == '\0') {
            if (part >= 4) return 0;
            parts[part++] = value;
            value = 0;
            if (ch == '\0') break;
        } else {
            return 0;
        }
    }

    if (part != 4) return 0;

    uint8_t *out = (uint8_t *)dst;
    out[0] = (uint8_t)parts[0];
    out[1] = (uint8_t)parts[1];
    out[2] = (uint8_t)parts[2];
    out[3] = (uint8_t)parts[3];
    return 1;
}

extern "C" int rand(void)
{
    return (int)(tls_rdtsc() & 0x7fffffff);
}

extern "C" psa_status_t mbedtls_psa_external_get_random(
    mbedtls_psa_external_random_context_t *context,
    uint8_t *output, size_t output_size, size_t *output_length)
{
    if (context == NULL || output == NULL || output_length == NULL) {
        return PSA_ERROR_INVALID_ARGUMENT;
    }

    uint64_t *state = (uint64_t *)context;
    if (state[0] == 0 && state[1] == 0) {
        state[0] = tls_rdtsc() ^ (uint64_t)(uintptr_t)context ^ 0x5858333830475541ULL;
        state[1] = tls_mix64((uint64_t)(uintptr_t)output ^ 0x9e3779b97f4a7c15ULL);
    }

    for (size_t i = 0; i < output_size; ++i) {
        state[0] ^= tls_rdtsc() + ((uint64_t)(uintptr_t)&output[i] << 7);
        state[0] = tls_mix64(state[0] + state[1]);
        state[1] = tls_mix64(state[1] + 0x9e3779b97f4a7c15ULL + i);
        output[i] = (uint8_t)(state[(i & 1U)] >> ((i & 7U) * 8U));
    }

    *output_length = output_size;
    return PSA_SUCCESS;
}

extern "C" void psa_reset_key_attributes(psa_key_attributes_t *attributes)
{
    if (attributes != NULL) {
        memset(attributes, 0, sizeof(*attributes));
    }
}

extern "C" long syscall(long, ...)
{
    return -1;
}

extern "C" void __explicit_bzero_chk(void *dest, size_t len, size_t destlen)
{
    if (len > destlen) len = destlen;
    memset(dest, 0, len);
}

extern "C" int pthread_mutex_init(int *mutex, const int *)
{
    if (mutex != NULL) *mutex = 0;
    return 0;
}

extern "C" int pthread_mutex_destroy(int *)
{
    return 0;
}

extern "C" int pthread_mutex_lock(int *)
{
    return 0;
}

extern "C" int pthread_mutex_unlock(int *)
{
    return 0;
}

extern "C" uint64_t __stack_chk_guard;

extern "C" int mbedtls_aesni_has_support(unsigned int)
{
    return 0;
}

extern "C" int mbedtls_aesni_crypt_ecb(...)
{
    return -1;
}

extern "C" int mbedtls_aesni_gcm_mult(...)
{
    return -1;
}

extern "C" int mbedtls_aesni_inverse_key(...)
{
    return -1;
}

extern "C" int mbedtls_aesni_setkey_enc(...)
{
    return -1;
}

extern "C" int xtls_prepare_runtime(uint64_t *saved_fs, void **tls_block)
{
    if (saved_fs == NULL || tls_block == NULL) return -1;

    uint8_t *block = (uint8_t *)malloc(0x40);
    if (block == NULL) return -1;

    memset(block, 0, 0x40);

    uint64_t original_fs = enter_syscall(SYS_ARCH_PRCTL, ARCH_GET_FS, 0, 0, 0, 0, 0);

    *(uint64_t *)(block + 0x28) = __stack_chk_guard;

    uint64_t set_ret = enter_syscall(SYS_ARCH_PRCTL, ARCH_SET_FS, (uint64_t)block, 0, 0, 0, 0);
    if ((int64_t)set_ret < 0) {
        free(block);
        return -1;
    }

    *saved_fs = original_fs;
    *tls_block = block;
    return 0;
}

extern "C" void xtls_restore_runtime(uint64_t saved_fs, void *tls_block)
{
    enter_syscall(SYS_ARCH_PRCTL, ARCH_SET_FS, saved_fs, 0, 0, 0, 0);
    if (tls_block != NULL) free(tls_block);
}
