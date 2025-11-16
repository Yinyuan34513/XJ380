#include "hda/vsound.h"
#include "proto.hpp"
#include "krlibc.h"
#include "fs/vfs/vfs.h"

#define ALL_IMPLEMENTATION
#include "rbtree-strptr.h"

#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO
#include "dr_mp3.h"

static rbtree_sp_t vsound_list;

static const int index_adjust[8] = {-1, -1, -1, -1, 2, 4, 6, 8};

static const int step_table[89] = {
    7,     8,     9,     10,    11,    12,    13,    14,    16,    17,    19,   21,    23,
    25,    28,    31,    34,    37,    41,    45,    50,    55,    60,    66,   73,    80,
    88,    97,    107,   118,   130,   143,   157,   173,   190,   209,   230,  253,   279,
    307,   337,   371,   408,   449,   494,   544,   598,   658,   724,   796,  876,   963,
    1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,  2272,  2499,  2749, 3024,  3327,
    3660,  4026,  4428,  4871,  5358,  5894,  6484,  7132,  7845,  8630,  9493, 10442, 11487,
    12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};

void sound_ima_adpcm_encode(ImaAdpcmCtx *ctx, void *dst, const int16_t *src, size_t len) {
    for (size_t i = 0; i < len; i++) {
        int delta = src[i] - ctx->prev_sample;
        int sb    = delta < 0 ? 8 : 0;
        delta     = delta < 0 ? -delta : delta;
        int code  = 4 * delta / step_table[ctx->index];
        if (code > 7) code = 7;
        ctx->index += index_adjust[code];
        if (ctx->index < 0) ctx->index = 0;
        if (ctx->index > 88) ctx->index = 88;
        ctx->prev_sample = src[i];
        if (i % 2 == 0) {
            ((uint8_t *)dst)[i / 2] = code | sb;
        } else {
            ((uint8_t *)dst)[i / 2] |= (code | sb) << 4;
        }
    }
}

void sound_ima_adpcm_decode(ImaAdpcmCtx *ctx, int16_t *dst, const void *src, size_t len) {
    for (size_t i = 0; i < len; i++) {
        int  code  = i % 2 == 0 ? ((uint8_t *)src)[i / 2] & 0x0f : ((uint8_t *)src)[i / 2] >> 4;
        bool sb    = code & 8;
        code      &= 7;
        int delta  = (step_table[ctx->index] * code) / 4 + step_table[ctx->index] / 8;
        delta      = sb ? -delta : delta;
        ctx->prev_sample += delta;
        if (ctx->prev_sample > 32767) ctx->prev_sample = 32767;
        if (ctx->prev_sample < -32768) ctx->prev_sample = -32768;
        dst[i]      = ctx->prev_sample;
        ctx->index += index_adjust[code];
        if (ctx->index < 0) ctx->index = 0;
        if (ctx->index > 88) ctx->index = 88;
    }
}
// 是否为有符号
bool _sound_fmt_issigned[] = {
    // - 8bit / 1byte
    true,
    false,
    // - 16bit / 2byte
    true,
    true,
    false,
    false,
    // - 24bit / 3byte
    true,
    true,
    false,
    false,
    // - 24bit / 4byte(low 3byte)
    true,
    true,
    false,
    false,
    // - 32bit / 4byte
    true,
    true,
    false,
    false,
    // - 64bit / 8byte
    true,
    true,
    false,
    false,
    // - 16bit / 2byte  <- float
    true,
    true,
    // - 32bit / 4byte  <- float
    true,
    true,
    // - 64bit / 8byte  <- float
    true,
    true,
};
// 是否为浮点
bool _sound_fmt_isfloat[] = {
    // - 8bit / 1byte
    false,
    false,
    // - 16bit / 2byte
    false,
    false,
    false,
    false,
    // - 24bit / 3byte
    false,
    false,
    false,
    false,
    // - 24bit / 4byte(low 3byte)
    false,
    false,
    false,
    false,
    // - 32bit / 4byte
    false,
    false,
    false,
    false,
    // - 64bit / 8byte
    false,
    false,
    false,
    false,
    // - 16bit / 2byte  <- float
    true,
    true,
    // - 32bit / 4byte  <- float
    true,
    true,
    // - 64bit / 8byte  <- float
    true,
    true,
};
// 是否为大端序
bool _sound_fmt_isbe[] = {
    // - 8bit / 1byte
    false,
    false,
    // - 16bit / 2byte
    false,
    true,
    false,
    true,
    // - 24bit / 3byte
    false,
    true,
    false,
    true,
    // - 24bit / 4byte(low 3byte)
    false,
    true,
    false,
    true,
    // - 32bit / 4byte
    false,
    true,
    false,
    true,
    // - 64bit / 8byte
    false,
    true,
    false,
    true,
    // - 16bit / 2byte  <- float
    false,
    true,
    // - 32bit / 4byte  <- float
    false,
    true,
    // - 64bit / 8byte  <- float
    false,
    true,
};
// 大小
int _sound_fmt_bytes[] = {
    // - 8bit / 1byte
    1,
    1,
    // - 16bit / 2byte
    2,
    2,
    2,
    2,
    // - 24bit / 3byte
    3,
    3,
    3,
    3,
    // - 24bit / 4byte(low 3byte)
    4,
    4,
    4,
    4,
    // - 32bit / 4byte
    4,
    4,
    4,
    4,
    // - 64bit / 8byte
    8,
    8,
    8,
    8,
    // - 16bit / 2byte  <- float
    2,
    2,
    // - 32bit / 4byte  <- float
    4,
    4,
    // - 64bit / 8byte  <- float
    8,
    8,
};

bool sound_fmt_issigned(sound_pcmfmt_t fmt) {
    if (fmt < 0 || fmt >= SOUND_FMT_CNT) return false;
    return _sound_fmt_issigned[fmt];
}

bool sound_fmt_isfloat(sound_pcmfmt_t fmt) {
    if (fmt < 0 || fmt >= SOUND_FMT_CNT) return false;
    return _sound_fmt_isfloat[fmt];
}

bool sound_fmt_isbe(sound_pcmfmt_t fmt) {
    if (fmt < 0 || fmt >= SOUND_FMT_CNT) return false;
    return _sound_fmt_isbe[fmt];
}

int sound_fmt_bytes(sound_pcmfmt_t fmt) {
    if (fmt < 0 || fmt >= SOUND_FMT_CNT) return -1;
    return _sound_fmt_bytes[fmt];
}

static int samplerate_id(int rate) {
    switch (rate) {
    case 8000: return 0;
    case 11025: return 1;
    case 16000: return 2;
    case 22050: return 3;
    case 24000: return 4;
    case 32000: return 5;
    case 44100: return 6;
    case 47250: return 7;
    case 48000: return 8;
    case 50000: return 9;
    case 88200: return 10;
    case 96000: return 11;
    case 176400: return 12;
    case 192000: return 13;
    case 352800: return 14;
    case 384000: return 15;
    case 768000: return 16;
    default: return -1;
    }
}

static void *getbuffer(vsound_t snd) {
    if (snd->bufpos == snd->bufsize) {
        queue_enqueue(snd->bufs1, snd->buf);
        if (snd->start_dma) snd->start_dma(snd, snd->buf);
        snd->is_dma_ready = true;
        snd->buf          = NULL;
    }
    if (snd->buf == NULL) {
        snd->buf    = queue_dequeue(snd->bufs0);
        snd->bufpos = 0;
    }
    return snd->buf;
}

bool vsound_regist(vsound_t device) {
    if (device == NULL) return false;
    if (device->is_registed || device->is_using) return false;
    if (rbtree_sp_get(vsound_list, device->name)) return false;
    rbtree_sp_insert(vsound_list, device->name, device);
    device->is_registed = true;
    return true;
}

#define MASK8(n)  ((uint8_t)1 << (n))
#define MASK16(n) ((uint16_t)1 << (n))
#define MASK32(n) ((uint32_t)1 << (n))
#define MASK64(n) ((uint64_t)1 << (n))
#define MASK(n)   ((size_t)1 << (n))

bool vsound_set_supported_fmt(vsound_t device, int16_t fmt) {
    if (device == NULL) return false;
    if (fmt >= SOUND_FMT_CNT) {
        write_serial_fmt("不支持的采样格式 %d", fmt);
        return false;
    }
    device->supported_fmts |= MASK32(fmt);
    return true;
}

bool vsound_set_supported_rate(vsound_t device, int32_t rate) {
    if (device == NULL) return false;
    int id = samplerate_id(rate);
    if (id < 0) {
        write_serial_fmt("不支持的采样率 %d", rate);
        return false;
    }
    device->supported_rates |= MASK32(id);
    return true;
}

bool vsound_set_supported_ch(vsound_t device, int16_t ch) {
    if (device == NULL) return false;
    if (ch < 1 || ch > 16) {
        write_serial_fmt("不支持的声道数 %d", ch);
        return false;
    }
    device->supported_chs |= MASK32(ch - 1);
    return true;
}

bool vsound_set_supported_fmts(vsound_t device, const int16_t *fmts, ssize_t len) {
    if (device == NULL) return false;
    size_t nseted = 0;
    if (len < 0) {
        for (size_t i = 0; fmts[i] >= 0; i++) {
            if (fmts[i] >= SOUND_FMT_CNT) {
                write_serial_fmt("不支持的采样格式 %d", fmts[i]);
                continue;
            }
            device->supported_fmts |= MASK32(fmts[i]);
            nseted++;
        }
    } else {
        for (size_t i = 0; i < len; i++) {
            if (fmts[i] >= SOUND_FMT_CNT) {
                write_serial_fmt("不支持的采样格式 %d", fmts[i]);
                continue;
            }
            device->supported_fmts |= MASK32(fmts[i]);
            nseted++;
        }
    }
    return nseted > 0;
}

bool vsound_set_supported_rates(vsound_t device, const int32_t *rates, ssize_t len) {
    if (device == NULL) return false;
    size_t nseted = 0;
    if (len < 0) {
        for (size_t i = 0; rates[i] > 0; i++) {
            int id = samplerate_id(rates[i]);
            if (id < 0) {
                write_serial_fmt("不支持的采样率 %d", rates[i]);
                continue;
            }
            device->supported_rates |= MASK32(id);
            nseted++;
        }
    } else {
        for (size_t i = 0; i < len; i++) {
            int id = samplerate_id(rates[i]);
            if (id < 0) {
                write_serial_fmt("不支持的采样率 %d", rates[i]);
                continue;
            }
            device->supported_rates |= MASK32(id);
            nseted++;
        }
    }
    return nseted > 0;
}

bool vsound_set_supported_chs(vsound_t device, const int16_t *chs, ssize_t len) {
    if (device == NULL) return false;
    size_t nseted = 0;
    if (len < 0) {
        for (size_t i = 0; chs[i] > 0; i++) {
            if (chs[i] < 1 || chs[i] > 16) {
                write_serial_fmt("不支持的声道数 %d", chs[i]);
                continue;
            }
            device->supported_chs |= MASK32(chs[i] - 1);
            nseted++;
        }
    } else {
        for (size_t i = 0; i < len; i++) {
            if (chs[i] < 1 || chs[i] > 16) {
                write_serial_fmt("不支持的声道数 %d", chs[i]);
                continue;
            }
            device->supported_chs |= MASK32(chs[i] - 1);
            nseted++;
        }
    }
    return nseted > 0;
}

void vsound_addbuf(vsound_t device, void *buf) {
    if (device == NULL) return;
    memset(buf, 0, device->bufsize);
    queue_enqueue(device->bufs0, buf);
}

void vsound_addbufs(vsound_t device, void *const *bufs, ssize_t len) {
    if (device == NULL) return;
    if (len < 0) {
        for (size_t i = 0; bufs[i] != NULL; i++) {
            memset(bufs[i], 0, device->bufsize);
            queue_enqueue(device->bufs0, bufs[i]);
        }
    } else {
        for (size_t i = 0; i < len; i++) {
            memset(bufs[i], 0, device->bufsize);
            queue_enqueue(device->bufs0, bufs[i]);
        }
    }
}

vsound_t vsound_find(const char *name) {
    return (vsound_t)rbtree_sp_get(vsound_list, name);
}

int vsound_played(vsound_t snd) {
    if (snd == NULL) return -1;
    void *buf = queue_dequeue(snd->bufs1);
    if (buf == NULL) return -1;
    memset(buf, 0, snd->bufsize);
    queue_enqueue(snd->bufs0, buf);
    return 0;
}

int vsound_clearbuffer(vsound_t snd) {
    if (snd == NULL) return -1;
    void *buf;
    while ((buf = queue_dequeue(snd->bufs1)) != NULL) {
        memset(buf, 0, snd->bufsize);
        queue_enqueue(snd->bufs0, buf);
    }
    if (snd->buf != NULL) {
        memset(snd->buf, 0, snd->bufsize);
        queue_enqueue(snd->bufs0, snd->buf);
    }
    return 0;
}

int vsound_open(vsound_t snd) { // 打开设备
    if (snd->is_using || snd->is_dma_ready || snd->is_running) return -1;
    if ((snd->supported_fmts & MASK32(snd->fmt)) == 0) return -1;
    int id = samplerate_id(snd->rate);
    if (id < 0) return -1;
    if ((snd->supported_rates & MASK32(id)) == 0) return -1;
    if ((snd->supported_chs & MASK32(snd->channels - 1)) == 0) return -1;
    snd->bytes_per_sample = sound_fmt_bytes((sound_pcmfmt_t)snd->fmt) * snd->channels;
    snd->open(snd);
    snd->is_using = true;
    return 0;
}

int vsound_close(vsound_t snd) { // 关闭设备
    if (snd == NULL) return -1;
    snd->close(snd);
    snd->is_using     = false;
    snd->is_dma_ready = false;
    snd->is_running   = false;
    vsound_clearbuffer(snd);
    return 0;
}

int vsound_play(vsound_t snd) { // 开始播放
    if (snd == NULL) return -1;
    if (snd->is_running) return 1;
    int rets = snd->play ? snd->play(snd) : -1;
    if (rets == 0) snd->is_running = true;
    return rets;
}

int vsound_pause(vsound_t snd) { // 暂停播放
    if (snd == NULL) return -1;
    if (!snd->is_running) return 1;
    int rets = snd->pause ? snd->pause(snd) : -1;
    if (rets == 0) snd->is_running = false;
    return rets;
}

int vsound_drop(vsound_t snd) { // 停止播放并丢弃缓冲
    if (snd == NULL) return -1;
    if (!snd->is_running) return 1;
    int rets = snd->drop ? snd->drop(snd) : -1;
    if (rets == 0) snd->is_running = false;
    return rets;
}

int vsound_drain(vsound_t snd) { // 等待播放完毕后停止播放
    if (snd == NULL) return -1;
    if (!snd->is_running) return 1;
    int rets = snd->drain ? snd->drain(snd) : -1;
    if (rets == 0) snd->is_running = false;
    return rets;
}

int vsound_read(vsound_t snd, uint64_t *data, size_t len) { // 读取 (录音)
    if (snd == NULL) return -1;
    if (snd->is_output) return -1;
    if (snd->is_rwmode) {
        if (snd->read) return snd->read(snd, data, len);
        return -1;
    }

    void  *buf  = getbuffer(snd);
    size_t size = len * snd->bytes_per_sample;
    while (size > 0) {
        waitif((buf = getbuffer(snd)) == NULL);
        size_t nread = MIN(size, snd->bufsize - snd->bufpos);
        memcpy(data, (void*)(((uint64_t*)(buf)) + snd->bufpos), nread);
        snd->bufpos += nread;
        data        += nread;
        size        -= nread;
    }
    getbuffer(snd); // 刷新一下，如果读空就触发 DMA
    return len;
}

int vsound_write(vsound_t snd, uint64_t *data, size_t len) { // 写入 (播放)
    if (snd == NULL) return -1;
    if (!snd->is_output) return -1;
    if (snd->is_rwmode) {
        if (snd && snd->write) return snd->write(snd, data, len);
        return -1;
    }

    void  *buf  = getbuffer(snd);
    size_t size = len * snd->bytes_per_sample;
    while (size > 0) {
        waitif((buf = getbuffer(snd)) == NULL);
        size_t nwrite = MIN(size, snd->bufsize - snd->bufpos);
        memcpy((void*)(((uint64_t*)(buf)) + snd->bufpos), data, nwrite);
        snd->bufpos += nwrite;
        data        += nwrite;
        size        -= nwrite;
    }
    getbuffer(snd); // 刷新一下，如果写满就触发 DMA
    return len;
}

float vsound_getvol(vsound_t snd) {
    return snd ? snd->volume : -1;
}

int vsound_setvol(vsound_t snd, float vol) {
    if (snd && snd->setvol) {
        snd->volume = vol;
        return snd->setvol(snd, vol);
    }
    return -1;
}

vsound_t snd;

void wav_player(const char *path) {
    vfs_node_t n    = vfs_open(path);
    void      *buf1 = malloc(n->size);
    vfs_read(n, buf1, 0, n->size);

    snd           = vsound_find("hda");
    snd->fmt      = SOUND_FMT_U8;
    snd->channels = 1;
    snd->rate     = 8000;
    snd->volume   = 1;
    vsound_open(snd);
    uint8_t data[1024];

    for (int nx = 0;; nx++) {
        for (int i = 0; i < 1024; i++) {
            const int t = nx * 1024 + i;
            data[i]     = t * (42 & t >> 10);
        }
        vsound_write(snd, (uint64_t*)data, 1024);
    }
    vsound_close(snd);
    free(buf1);
}

void mp3_player(const char *path) {
    vfs_node_t n    = vfs_open(path);
    void      *buf1 = malloc(n->size);
    vfs_read(n, buf1, 0, n->size);
    drmp3_uint64 samples;

    drmp3_config mp3;
    short *data   = drmp3_open_memory_and_read_pcm_frames_s16(buf1, n->size, &mp3, &samples, NULL);
    snd           = vsound_find("hda");
    snd->fmt      = SOUND_FMT_S16;
    snd->channels = mp3.channels;
    snd->rate     = mp3.sampleRate;
    snd->volume   = 1;

    write_serial_fmt("MP3 SAMPLE: %d %08x\n", samples, data);

    vsound_open(snd);
    for (int i = 0; i < samples; i += snd->bufsize / mp3.channels) {
        write_serial_fmt("writing %d samples", i);
        vsound_write(snd, (uint64_t*)data + i * mp3.channels, snd->bufsize / mp3.channels);
        write_serial_fmt("\r%d/%d sec", (int)((float)i / (float)mp3.sampleRate),
              (int)((float)samples / (float)mp3.sampleRate));
    }
    vsound_close(snd);
    free(data);
    free(buf1);
}