#include <device.h>
#include <drm/drm.h>
#include <drm/drm_mode.h>
#include <errno.h>
#include <fs/vfs/anonfs.h>
#include <fbdev.h>
#include <fs/vfs/vfs.h>
#include <ioctl.h>
#include <krlibc.h>
#include <lock_queue.h>
#include <mm/heap.h>
#include <mm/page.h>
#include <mm/uaccess.h>
#include <proto.hpp>
#include <syscall/syscall.h>
#include <task/poll.h>
#include <task/pcb.h>

/* ---- Driver constants ---- */
#define MAX_DUMB_BUFFERS     32
#define MAX_FB_OBJECTS       32
#define MAX_PROP_BLOBS       16
#define MAX_PRIME_FDS        32
#define EVENT_RING_SIZE      16
#define DEFAULT_MODE_NAME    "1024x768"
#define XJ380_DRM_AUTH_MAGIC 0x12345678U
#define DRM_PROP_CONNECTOR_EDID  0x1010
#define DRM_PROP_PLANE_IN_FORMATS 0x1011
#define DRM_BLOB_ID_CRTC_MODE_BASE 0x10000000U
#define DRM_BLOB_ID_CONNECTOR_EDID_BASE 0x20000000U
#define DRM_BLOB_ID_PLANE_IN_FORMATS_BASE 0x28000000U
#define DRM_U32_MAX 0xffffffffU

/* ---- Dumb buffer ---- */
struct dumb_buffer {
    bool     used;
    uint64_t phys_addr;
    uint32_t width, height, pitch;
    uint32_t handle;
    uint32_t refs;
};

/* ---- DRM framebuffer object ---- */
struct drm_fb_obj {
    bool     used;
    uint32_t fb_id;
    uint32_t handle;
    uint32_t width, height, pitch;
    uint32_t offset;
    uint32_t format;
};

/* ---- Property blob ---- */
struct prop_blob {
    bool     used;
    uint32_t blob_id;
    void    *data;
    uint32_t length;
};

/* ---- Minimal PRIME fd export tracking ---- */
struct prime_fd_entry {
    bool       used;
    pcb_t      process;
    vfs_node_t node;
    uint64_t   inode;
    int32_t    fd;
    uint32_t   handle;
};

/* ---- Per-open file state ---- */
struct drm_file {
    uint64_t client_caps;   /* bitmask of client capabilities */
};

/* ---- Event queue ---- */
struct event_entry {
    struct drm_event_vblank vbl;
};

/* ---- Device state ---- */
struct xj380_drm {
    device_t            device;
    spin_t              lock;

    /* singleton objects */
    uint32_t            crtc_id;
    uint32_t            connector_id;
    uint32_t            encoder_id;
    uint32_t            plane_id;

    /* mutable CRTC state */
    uint32_t            current_fb_id;
    struct drm_mode_modeinfo mode;

    /* resource pools */
    struct dumb_buffer  dumb[MAX_DUMB_BUFFERS];
    struct drm_fb_obj   fbs[MAX_FB_OBJECTS];
    struct prop_blob    blobs[MAX_PROP_BLOBS];
    struct prime_fd_entry prime_fds[MAX_PRIME_FDS];
    uint32_t            next_fb_id;

    /* version */
    char drv_name[32], drv_date[32], drv_desc[64];

    /* framebuffer info */
    uint32_t            fb_width, fb_height;

    /* event ring (per-device, single reader for now) */
    struct event_entry  events[EVENT_RING_SIZE];
    uint32_t            event_head, event_count;
    uint32_t            vblank_sequence;
    bool                event_awaiting;  /* reader is sleeping */
};

static struct xj380_drm g_drm;
static bool g_inited = false;

/* ---- Helpers ---- */

static inline bool drm_initialized() { return g_inited; }

static void drm_monotonic_timeval(uint32_t *sec, uint32_t *usec)
{
    uint64_t now_ns = bootNanoTime();
    *sec  = (uint32_t)(now_ns / 1000000000ULL);
    *usec = (uint32_t)((now_ns % 1000000000ULL) / 1000ULL);
}

static uint32_t drm_connector_edid_blob_id(uint32_t connector_id)
{
    return DRM_BLOB_ID_CONNECTOR_EDID_BASE + connector_id;
}

static uint32_t drm_crtc_mode_blob_id(uint32_t crtc_id)
{
    return DRM_BLOB_ID_CRTC_MODE_BASE + crtc_id;
}

static uint32_t drm_plane_in_formats_blob_id(uint32_t plane_id)
{
    return DRM_BLOB_ID_PLANE_IN_FORMATS_BASE + plane_id;
}

static bool drm_mode_blob_to_crtc_id(uint32_t blob_id, uint32_t *crtc_id)
{
    if (blob_id <= DRM_BLOB_ID_CRTC_MODE_BASE || blob_id >= DRM_BLOB_ID_CONNECTOR_EDID_BASE || crtc_id == NULL)
        return false;
    *crtc_id = blob_id - DRM_BLOB_ID_CRTC_MODE_BASE;
    return *crtc_id != 0;
}

static bool drm_plane_in_formats_blob_to_plane_id(uint32_t blob_id, uint32_t *plane_id)
{
    if (blob_id <= DRM_BLOB_ID_PLANE_IN_FORMATS_BASE || plane_id == NULL) return false;
    *plane_id = blob_id - DRM_BLOB_ID_PLANE_IN_FORMATS_BASE;
    return true;
}

static bool drm_connector_edid_blob_to_connector_id(uint32_t blob_id, uint32_t *connector_id)
{
    if (blob_id <= DRM_BLOB_ID_CONNECTOR_EDID_BASE || blob_id >= DRM_BLOB_ID_PLANE_IN_FORMATS_BASE ||
        connector_id == NULL)
        return false;
    *connector_id = blob_id - DRM_BLOB_ID_CONNECTOR_EDID_BASE;
    return true;
}

static int drm_copy_to_user_ptr(uint64_t user_ptr, const void *src, size_t size)
{
    if (size == 0) return 0;
    if (user_ptr == 0 || src == NULL) return -EFAULT;
    return copy_to_user_pagedir(get_current_task()->parent_group->pagedir, (void *)(uintptr_t)user_ptr,
                                (void *)src, size)
               ? 0
               : -EFAULT;
}

static int drm_copy_from_user_ptr(void *dst, uint64_t user_ptr, size_t size)
{
    if (size == 0) return 0;
    if (user_ptr == 0 || dst == NULL) return -EFAULT;
    return copy_from_user_pagedir(get_current_task()->parent_group->pagedir, dst, (void *)(uintptr_t)user_ptr, size)
               ? 0
               : -EFAULT;
}

static int drm_copy_counted_array_to_user(uint64_t user_ptr, const void *src, uint32_t capacity, uint32_t count,
                                          size_t elem_size)
{
    size_t copy_count = MIN((size_t)capacity, (size_t)count);
    if (copy_count == 0) return 0;
    return drm_copy_to_user_ptr(user_ptr, src, copy_count * elem_size);
}

static void drm_init_mode(struct drm_mode_modeinfo *mode, uint32_t width, uint32_t height, uint32_t refresh)
{
    memset(mode, 0, sizeof(*mode));
    
    /* CVT-RB 标准时序（Reduced Blanking） */
    if (width == 1024 && height == 768) {
        /* 标准 1024x768@60 时序 */
        mode->clock = 65000;           /* 65 MHz */
        mode->hdisplay = 1024;
        mode->hsync_start = 1024 + 24;   /* 1048 */
        mode->hsync_end = 1024 + 24 + 136; /* 1184 */
        mode->htotal = 1024 + 24 + 136 + 160; /* 1344 */
        mode->vdisplay = 768;
        mode->vsync_start = 768 + 3;     /* 771 */
        mode->vsync_end = 768 + 3 + 6;   /* 777 */
        mode->vtotal = 768 + 3 + 6 + 29; /* 806 */
        mode->vrefresh = 60;
        mode->flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC;
        strncpy(mode->name, "1024x768", DRM_DISPLAY_MODE_LEN - 1);
    } 
    else if (width == 1920 && height == 1080) {
        /* 1080p@60 时序 */
        mode->clock = 148500;
        mode->hdisplay = 1920;
        mode->hsync_start = 1920 + 88;
        mode->hsync_end = 1920 + 88 + 44;
        mode->htotal = 1920 + 88 + 44 + 148;
        mode->vdisplay = 1080;
        mode->vsync_start = 1080 + 4;
        mode->vsync_end = 1080 + 4 + 5;
        mode->vtotal = 1080 + 4 + 5 + 36;
        mode->vrefresh = 60;
        mode->flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC;
        strncpy(mode->name, "1920x1080", DRM_DISPLAY_MODE_LEN - 1);
    }
    else {
        /*  CVT-RB 公式 */
        mode->hdisplay = width;
        mode->vdisplay = height;
        mode->vrefresh = refresh;
        
        /* Reduced blanking timings */
        mode->htotal = width + 160;
        mode->hsync_start = width + 32;
        mode->hsync_end = width + 80;
        mode->vtotal = height + 30;
        mode->vsync_start = height + 3;
        mode->vsync_end = height + 9;
        mode->clock = (mode->htotal * mode->vtotal * refresh) / 1000;
        mode->flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC;
        snprintf(mode->name, DRM_DISPLAY_MODE_LEN, "%dx%d", width, height);
    }
    
    mode->type = DRM_MODE_TYPE_PREFERRED | DRM_MODE_TYPE_DRIVER;
    mode->hskew = 0;
    mode->vscan = 0;
}

static void drm_build_edid(const struct drm_mode_modeinfo *mode, uint8_t *edid, size_t edid_size)
{
    if (mode == NULL || edid == NULL || edid_size < 128) return;

    memset(edid, 0, edid_size);
    static const uint8_t header[8] = {0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00};
    memcpy(edid, header, sizeof(header));

    /* Manufacturer "XJX". */
    edid[8] = (uint8_t)((24 << 2) | (10 >> 3));
    edid[9] = (uint8_t)(((10 & 0x7) << 5) | 24);
    edid[10] = 0x80;
    edid[11] = 0x03;
    edid[12] = 0x01;
    edid[16] = 1;
    edid[17] = 36;
    edid[18] = 1;
    edid[19] = 4;
    edid[20] = 0xa5;
    edid[21] = (uint8_t)MIN((uint32_t)255, ((uint32_t)mode->hdisplay * 264u) / 10000u);
    edid[22] = (uint8_t)MIN((uint32_t)255, ((uint32_t)mode->vdisplay * 264u) / 10000u);
    edid[23] = 0x78;
    edid[24] = 0x0a;
    edid[25] = 0xee;
    edid[26] = 0x91;
    edid[27] = 0xa3;
    edid[28] = 0x54;
    edid[29] = 0x4c;
    edid[30] = 0x99;
    edid[31] = 0x26;
    edid[32] = 0x0f;
    edid[33] = 0x50;
    edid[34] = 0x54;
    memset(edid + 38, 0x01, 16);

    uint8_t *dtd = edid + 54;
    uint32_t pixel_clock = (mode->clock + 5) / 10;
    if (pixel_clock > 0xffff) pixel_clock = 0xffff;
    uint32_t hblank = mode->htotal > mode->hdisplay ? mode->htotal - mode->hdisplay : 160;
    uint32_t vblank = mode->vtotal > mode->vdisplay ? mode->vtotal - mode->vdisplay : 30;
    uint32_t hsync_offset = mode->hsync_start > mode->hdisplay ? mode->hsync_start - mode->hdisplay : 32;
    uint32_t hsync_width = mode->hsync_end > mode->hsync_start ? mode->hsync_end - mode->hsync_start : 48;
    uint32_t vsync_offset = mode->vsync_start > mode->vdisplay ? mode->vsync_start - mode->vdisplay : 3;
    uint32_t vsync_width = mode->vsync_end > mode->vsync_start ? mode->vsync_end - mode->vsync_start : 6;
    uint32_t hsize = MIN((uint32_t)4095, ((uint32_t)mode->hdisplay * 264u) / 1000u);
    uint32_t vsize = MIN((uint32_t)4095, ((uint32_t)mode->vdisplay * 264u) / 1000u);

    dtd[0] = (uint8_t)(pixel_clock & 0xff);
    dtd[1] = (uint8_t)(pixel_clock >> 8);
    dtd[2] = (uint8_t)(mode->hdisplay & 0xff);
    dtd[3] = (uint8_t)(hblank & 0xff);
    dtd[4] = (uint8_t)(((mode->hdisplay >> 8) & 0xf) << 4 | ((hblank >> 8) & 0xf));
    dtd[5] = (uint8_t)(mode->vdisplay & 0xff);
    dtd[6] = (uint8_t)(vblank & 0xff);
    dtd[7] = (uint8_t)(((mode->vdisplay >> 8) & 0xf) << 4 | ((vblank >> 8) & 0xf));
    dtd[8] = (uint8_t)(hsync_offset & 0xff);
    dtd[9] = (uint8_t)(hsync_width & 0xff);
    dtd[10] = (uint8_t)(((vsync_offset & 0xf) << 4) | (vsync_width & 0xf));
    dtd[11] = (uint8_t)(((hsync_offset >> 8) & 0x3) << 6 | (((hsync_width >> 8) & 0x3) << 4) |
                        (((vsync_offset >> 4) & 0x3) << 2) | ((vsync_width >> 4) & 0x3));
    dtd[12] = (uint8_t)(hsize & 0xff);
    dtd[13] = (uint8_t)(vsize & 0xff);
    dtd[14] = (uint8_t)(((hsize >> 8) & 0xf) << 4 | ((vsize >> 8) & 0xf));
    dtd[17] = 0x18;
    if (mode->flags & DRM_MODE_FLAG_PHSYNC) dtd[17] |= 0x02;
    if (mode->flags & DRM_MODE_FLAG_PVSYNC) dtd[17] |= 0x04;

    uint8_t *name = edid + 72;
    name[3] = 0xfc;
    name[4] = 0x00;
    memset(name + 5, ' ', 13);
    const char monitor_name[] = "XJ380 DRM\n";
    memcpy(name + 5, monitor_name, sizeof(monitor_name) - 1);

    uint8_t *serial = edid + 90;
    serial[3] = 0xff;
    serial[4] = 0x00;
    memset(serial + 5, ' ', 13);
    const char monitor_serial[] = "XJ380-0001\n";
    memcpy(serial + 5, monitor_serial, sizeof(monitor_serial) - 1);

    uint8_t *dummy = edid + 108;
    dummy[3] = 0x10;

    uint8_t sum = 0;
    for (size_t i = 0; i < 127; i++) sum = (uint8_t)(sum + edid[i]);
    edid[127] = (uint8_t)(0 - sum);
}
struct drm_format_modifier_blob {
    uint32_t version;
    uint32_t flags;
    uint32_t count_formats;
    uint32_t formats_offset;
    uint32_t count_modifiers;
    uint32_t modifiers_offset;
};

static int dumb_find_free(struct xj380_drm *d)
{
    for (int i = 0; i < MAX_DUMB_BUFFERS; i++)
        if (!d->dumb[i].used) return i;
    return -1;
}

static int dumb_find_by_handle(struct xj380_drm *d, uint32_t handle)
{
    for (int i = 0; i < MAX_DUMB_BUFFERS; i++)
        if (d->dumb[i].used && d->dumb[i].handle == handle) return i;
    return -1;
}

static size_t dumb_buffer_pages(const struct dumb_buffer *buffer)
{
    if (buffer == NULL || !buffer->used || buffer->pitch == 0 || buffer->height == 0) return 0;
    return PADDING_UP((size_t)buffer->pitch * (size_t)buffer->height, PAGE_SIZE) / PAGE_SIZE;
}

static bool dumb_handle_has_fb(struct xj380_drm *d, uint32_t handle)
{
    for (int i = 0; i < MAX_FB_OBJECTS; i++)
        if (d->fbs[i].used && d->fbs[i].handle == handle) return true;
    return false;
}

static int dumb_get(struct xj380_drm *d, uint32_t handle)
{
    int idx = dumb_find_by_handle(d, handle);
    if (idx < 0) return -EINVAL;
    if (d->dumb[idx].refs == DRM_U32_MAX) return -EOVERFLOW;
    d->dumb[idx].refs++;
    write_serial_fmt("[drm-trace] dumb-get handle=%u refs=%u\n", handle, d->dumb[idx].refs);
    return 0;
}

static int dumb_put(struct xj380_drm *d, uint32_t handle, const char *reason)
{
    int idx = dumb_find_by_handle(d, handle);
    if (idx < 0) return -EINVAL;

    struct dumb_buffer *buffer = &d->dumb[idx];
    if (buffer->refs == 0)
    {
        write_serial_fmt("[drm-trace] dumb-put-underflow handle=%u reason=%s\n",
                         handle,
                         reason != NULL ? reason : "?");
        return -EINVAL;
    }

    buffer->refs--;
    write_serial_fmt("[drm-trace] dumb-put handle=%u reason=%s refs=%u fb_refs=%d phys=0x%llx\n",
                     handle,
                     reason != NULL ? reason : "?",
                     buffer->refs,
                     dumb_handle_has_fb(d, handle) ? 1 : 0,
                     (unsigned long long)buffer->phys_addr);
    if (buffer->refs != 0) return 0;

    if (dumb_handle_has_fb(d, handle))
    {
        write_serial_fmt("[drm-trace] dumb-put-keep-fb handle=%u\n", handle);
        buffer->refs = 1;
        return 0;
    }

    size_t pages = dumb_buffer_pages(buffer);
    uint64_t phys = buffer->phys_addr;
    if (phys != 0 && pages != 0) free_frames(phys, pages);
    memset(buffer, 0, sizeof(*buffer));
    return 0;
}

static int fb_find_free(struct xj380_drm *d)
{
    for (int i = 0; i < MAX_FB_OBJECTS; i++)
        if (!d->fbs[i].used) return i;
    return -1;
}

static struct drm_fb_obj *fb_find_by_id(struct xj380_drm *d, uint32_t fb_id)
{
    for (int i = 0; i < MAX_FB_OBJECTS; i++)
        if (d->fbs[i].used && d->fbs[i].fb_id == fb_id) return &d->fbs[i];
    return NULL;
}

static int blob_find_free(struct xj380_drm *d)
{
    for (int i = 0; i < MAX_PROP_BLOBS; i++)
        if (!d->blobs[i].used) return i;
    return -1;
}

static struct prop_blob *blob_find_by_id(struct xj380_drm *d, uint32_t blob_id)
{
    for (int i = 0; i < MAX_PROP_BLOBS; i++)
        if (d->blobs[i].used && d->blobs[i].blob_id == blob_id) return &d->blobs[i];
    return NULL;
}

static bool drm_mode_blob_to_mode(struct xj380_drm *drm, uint32_t blob_id, struct drm_mode_modeinfo *mode)
{
    if (drm == NULL || mode == NULL || blob_id == 0) return false;

    uint32_t crtc_id = 0;
    if (drm_mode_blob_to_crtc_id(blob_id, &crtc_id)) {
        if (crtc_id != drm->crtc_id) return false;
        *mode = drm->mode;
        return true;
    }

    struct prop_blob *blob = blob_find_by_id(drm, blob_id);
    if (blob == NULL || blob->data == NULL || blob->length < sizeof(*mode)) return false;

    memcpy(mode, blob->data, sizeof(*mode));
    if (mode->hdisplay == 0 || mode->vdisplay == 0) return false;
    if (mode->htotal < mode->hdisplay || mode->vtotal < mode->vdisplay) return false;
    return true;
}

static void ev_queue(struct xj380_drm *drm, uint64_t user_data)
{
    uint32_t sec;
    uint32_t usec;
    drm_monotonic_timeval(&sec, &usec);

    spin_lock(&drm->lock);
    if (drm->event_count >= EVENT_RING_SIZE)
    {
        spin_unlock(&drm->lock);
        return;
    }
    uint32_t idx = (drm->event_head + drm->event_count) % EVENT_RING_SIZE;
    struct event_entry *ev = &drm->events[idx];
    memset(ev, 0, sizeof(*ev));
    ev->vbl.base.type   = DRM_EVENT_FLIP_COMPLETE;
    ev->vbl.base.length = sizeof(ev->vbl);
    ev->vbl.user_data   = user_data;
    ev->vbl.tv_sec      = sec;
    ev->vbl.tv_usec     = usec;
    ev->vbl.sequence    = ++drm->vblank_sequence;
    ev->vbl.crtc_id     = drm->crtc_id;
    drm->event_count++;
    spin_unlock(&drm->lock);
    poll_notify_event();
}

static int drm_poll(size_t events)
{
    struct xj380_drm *drm = &g_drm;
    if (!drm_initialized()) return EPOLLERR;

    int revents = 0;
    spin_lock(&drm->lock);
    if ((events & (EPOLLIN | EPOLLRDNORM)) && drm->event_count > 0) revents |= EPOLLIN;
    if (events & (EPOLLOUT | EPOLLWRNORM)) revents |= EPOLLOUT;
    spin_unlock(&drm->lock);
    return revents;
}

/* ---- Present: copy dumb buffer to gop framebuffer ---- */
static bool drm_format_is_rgb_order(uint32_t format)
{
    return format == DRM_FORMAT_XRGB8888 || format == DRM_FORMAT_ARGB8888 ||
           format == DRM_FORMAT_RGBX8888 || format == DRM_FORMAT_RGBA8888;
}

static bool drm_format_is_bgr_order(uint32_t format)
{
    return format == DRM_FORMAT_XBGR8888 || format == DRM_FORMAT_ABGR8888 ||
           format == DRM_FORMAT_BGRX8888 || format == DRM_FORMAT_BGRA8888;
}

static uint32_t drm_read_pixel32(const uint8_t *src)
{
    return (uint32_t)src[0] | ((uint32_t)src[1] << 8) | ((uint32_t)src[2] << 16) | ((uint32_t)src[3] << 24);
}

static bool drm_format_matches_fbc_memory(uint32_t format, bool dst_rgb)
{
    switch (format) {
    case DRM_FORMAT_XRGB8888:
    case DRM_FORMAT_ARGB8888:
        return !dst_rgb;
    case DRM_FORMAT_XBGR8888:
    case DRM_FORMAT_ABGR8888:
        return dst_rgb;
    default:
        return false;
    }
}

static void drm_unpack_format_pixel(uint32_t pixel, uint32_t format, uint8_t *r, uint8_t *g, uint8_t *b,
                                    uint8_t *a)
{
    if (r == NULL || g == NULL || b == NULL || a == NULL) {
        return;
    }

    switch (format) {
    case DRM_FORMAT_XRGB8888:
    case DRM_FORMAT_ARGB8888:
        *r = (uint8_t)((pixel >> 16) & 0xffU);
        *g = (uint8_t)((pixel >> 8) & 0xffU);
        *b = (uint8_t)(pixel & 0xffU);
        *a = format == DRM_FORMAT_ARGB8888 ? (uint8_t)((pixel >> 24) & 0xffU) : 0xffU;
        return;
    case DRM_FORMAT_RGBX8888:
    case DRM_FORMAT_RGBA8888:
        *r = (uint8_t)((pixel >> 24) & 0xffU);
        *g = (uint8_t)((pixel >> 16) & 0xffU);
        *b = (uint8_t)((pixel >> 8) & 0xffU);
        *a = format == DRM_FORMAT_RGBA8888 ? (uint8_t)(pixel & 0xffU) : 0xffU;
        return;
    case DRM_FORMAT_XBGR8888:
    case DRM_FORMAT_ABGR8888:
        *r = (uint8_t)(pixel & 0xffU);
        *g = (uint8_t)((pixel >> 8) & 0xffU);
        *b = (uint8_t)((pixel >> 16) & 0xffU);
        *a = format == DRM_FORMAT_ABGR8888 ? (uint8_t)((pixel >> 24) & 0xffU) : 0xffU;
        return;
    case DRM_FORMAT_BGRX8888:
    case DRM_FORMAT_BGRA8888:
        *r = (uint8_t)((pixel >> 8) & 0xffU);
        *g = (uint8_t)((pixel >> 16) & 0xffU);
        *b = (uint8_t)((pixel >> 24) & 0xffU);
        *a = format == DRM_FORMAT_BGRA8888 ? (uint8_t)(pixel & 0xffU) : 0xffU;
        return;
    default:
        *r = 0;
        *g = 0;
        *b = 0;
        *a = 0xffU;
        return;
    }
}

static int present_rect(struct xj380_drm *drm,
                        uint32_t fb_id,
                        uint32_t src_x,
                        uint32_t src_y,
                        uint32_t crtc_x,
                        uint32_t crtc_y,
                        uint32_t width,
                        uint32_t height)
{
    struct drm_fb_obj *fb = fb_find_by_id(drm, fb_id);
    if (!fb) return -ENOENT;

    int idx = dumb_find_by_handle(drm, fb->handle);
    if (idx < 0) return -EINVAL;

    struct dumb_buffer *db = &drm->dumb[idx];
    uint8_t *src = (uint8_t *)driver_phys_to_virt(db->phys_addr);

    uint8_t *dst = fbdev_kernel_framebuffer();
    if (!dst) return -ENODEV;

    const FrameBufferConfig *fbc = fbdev_get_fbc();
    if (!fbc) return -ENODEV;

    size_t dst_pitch = fbdev_framebuffer_pitch();
    if (dst_pitch == 0) return -ENODEV;

    if (src_x >= fb->width || src_y >= fb->height) return 0;
    if (crtc_x >= drm->fb_width || crtc_y >= drm->fb_height) return 0;

    uint32_t w = MIN(width, fb->width - src_x);
    uint32_t h = MIN(height, fb->height - src_y);
    w = MIN(w, drm->fb_width - crtc_x);
    h = MIN(h, drm->fb_height - crtc_y);
    if (w == 0 || h == 0) return 0;

    if (!drm_format_is_rgb_order(fb->format) && !drm_format_is_bgr_order(fb->format)) return -EINVAL;
    bool dst_rgb = fbc->pixel_format == PixelFormat::kRGBR;

    src += fb->offset;
    size_t row_bytes = (size_t)w * 4;
    if (drm_format_matches_fbc_memory(fb->format, dst_rgb))
    {
        for (uint32_t y = 0; y < h; y++)
        {
            uint8_t *dst_row = dst + (size_t)(crtc_y + y) * dst_pitch + (size_t)crtc_x * 4;
            uint8_t *src_row = src + (size_t)(src_y + y) * (size_t)fb->pitch + (size_t)src_x * 4;
            memcpy(dst_row, src_row, row_bytes);
        }
        fbdev_present_kernel_region(crtc_x, crtc_y, crtc_x + w, crtc_y + h);
        return 0;
    }

    for (uint32_t y = 0; y < h; y++)
    {
        uint8_t *dst_row = dst + (size_t)(crtc_y + y) * dst_pitch + (size_t)crtc_x * 4;
        uint8_t *src_row = src + (size_t)(src_y + y) * (size_t)fb->pitch + (size_t)src_x * 4;
        for (uint32_t x = 0; x < w; x++)
        {
            uint32_t src_pixel = drm_read_pixel32(src_row + (size_t)x * 4);
            uint8_t  src_r;
            uint8_t  src_g;
            uint8_t  src_b;
            uint8_t  src_a;
            drm_unpack_format_pixel(src_pixel, fb->format, &src_r, &src_g, &src_b, &src_a);

            uint8_t *dst_pixel = dst_row + (size_t)x * 4;
            if (dst_rgb) {
                dst_pixel[0] = src_r;
                dst_pixel[1] = src_g;
                dst_pixel[2] = src_b;
            } else {
                dst_pixel[0] = src_b;
                dst_pixel[1] = src_g;
                dst_pixel[2] = src_r;
            }
            dst_pixel[3] = src_a;
        }
    }

    /* Tell fbdev to actually flush to GOP */
    fbdev_present_kernel_region(crtc_x, crtc_y, crtc_x + w, crtc_y + h);
    return 0;
}

static int present(struct xj380_drm *drm, uint32_t fb_id)
{
    struct drm_fb_obj *fb = fb_find_by_id(drm, fb_id);
    if (!fb) return -ENOENT;
    return present_rect(drm, fb_id, 0, 0, 0, 0, fb->width, fb->height);
}

/* ---- IOCTL handlers ---- */

static int drm_version(struct xj380_drm *drm, struct drm_version *ver)
{
    if (!ver) return -EFAULT;
    struct drm_version kv;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &kv, ver, sizeof(kv)))
        return -EFAULT;

    kv.version_major      = 1;
    kv.version_minor      = 0;
    kv.version_patchlevel = 0;

    auto cp_str = [](void *user_dst, size_t user_len, const char *src, size_t max_len) -> int {
        if (!user_dst || !user_len) return 0;
        size_t n = MIN(user_len - 1, max_len);
        if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, user_dst, (void *)src, n))
            return -EFAULT;
        char zero = 0;
        if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, (char *)user_dst + n, &zero, 1))
            return -EFAULT;
        return 0;
    };

    int r = cp_str(kv.name, kv.name_len, drm->drv_name, sizeof(drm->drv_name) - 1);
    if (r) return r;
    r = cp_str(kv.date, kv.date_len, drm->drv_date, sizeof(drm->drv_date) - 1);
    if (r) return r;
    r = cp_str(kv.desc, kv.desc_len, drm->drv_desc, sizeof(drm->drv_desc) - 1);
    if (r) return r;

    if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, ver, &kv, sizeof(kv)))
        return -EFAULT;
    return 0;
}

static int drm_get_cap(struct drm_get_cap *cap)
{
    if (!cap) return -EFAULT;
    struct drm_get_cap kc;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &kc, cap, sizeof(kc)))
        return -EFAULT;

    switch (kc.capability) {
    case DRM_CAP_DUMB_BUFFER:       kc.value = 1; break;
    case DRM_CAP_DUMB_PREFERRED_DEPTH: kc.value = 24; break;
    case DRM_CAP_DUMB_PREFER_SHADOW: kc.value = 1; break;
    case DRM_CAP_TIMESTAMP_MONOTONIC: kc.value = 1; break;
    case DRM_CAP_PRIME:             kc.value = DRM_PRIME_CAP_IMPORT | DRM_PRIME_CAP_EXPORT; break;
    case DRM_CAP_ASYNC_PAGE_FLIP:   kc.value = 1; break;
    case DRM_CAP_CURSOR_WIDTH:      kc.value = 64; break;
    case DRM_CAP_CURSOR_HEIGHT:     kc.value = 64; break;
    case DRM_CAP_ADDFB2_MODIFIERS:  kc.value = 0; break;
    case DRM_CAP_PAGE_FLIP_TARGET:  kc.value = 0; break;
    case DRM_CAP_CRTC_IN_VBLANK_EVENT: kc.value = 1; break;
    case DRM_CAP_ATOMIC_ASYNC_PAGE_FLIP: kc.value = 0; break;
    default:                        kc.value = 0; break;
    }

    if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, cap, &kc, sizeof(kc)))
        return -EFAULT;
    return 0;
}

static int drm_get_unique(struct xj380_drm *drm, struct drm_unique *unique)
{
    (void)drm;
    if (!unique) return -EFAULT;
    struct drm_unique ku;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &ku, unique, sizeof(ku)))
        return -EFAULT;

    const char busid[] = "platform:xj380";
    size_t busid_len = sizeof(busid) - 1;
    if (ku.unique && ku.unique_len > 0) {
        size_t n = MIN(ku.unique_len, busid_len);
        if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, ku.unique, (void *)busid, n))
            return -EFAULT;
    }
    ku.unique_len = busid_len;
    if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, unique, &ku, sizeof(ku)))
        return -EFAULT;
    return 0;
}

static int drm_get_magic(struct drm_auth *auth)
{
    if (!auth) return -EFAULT;

    struct drm_auth ka;
    ka.magic = XJ380_DRM_AUTH_MAGIC;
    if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, auth, &ka, sizeof(ka)))
        return -EFAULT;
    return 0;
}

static int drm_auth_magic(struct drm_auth *auth)
{
    if (!auth) return -EFAULT;

    struct drm_auth ka;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &ka, auth, sizeof(ka)))
        return -EFAULT;
    return ka.magic == XJ380_DRM_AUTH_MAGIC ? 0 : -EINVAL;
}

static int drm_set_version(struct drm_set_version *version)
{
    if (!version) return -EFAULT;
    struct drm_set_version kv;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &kv, version, sizeof(kv)))
        return -EFAULT;

    if (kv.drm_di_major < 1) return -EINVAL;
    kv.drm_di_major = 1;
    kv.drm_di_minor = 4;
    kv.drm_dd_major = 1;
    kv.drm_dd_minor = 0;
    if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, version, &kv, sizeof(kv)))
        return -EFAULT;
    return 0;
}

static int drm_get_stats(struct drm_stats *stats)
{
    if (!stats) return -EFAULT;
    struct drm_stats ks;
    memset(&ks, 0, sizeof(ks));
    if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, stats, &ks, sizeof(ks)))
        return -EFAULT;
    return 0;
}

static int drm_wait_vblank(union drm_wait_vblank *vblank)
{
    if (!vblank) return -EFAULT;
    union drm_wait_vblank kv;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &kv, vblank, sizeof(kv)))
        return -EFAULT;

    uint32_t sec;
    uint32_t usec;
    drm_monotonic_timeval(&sec, &usec);

    spin_lock(&g_drm.lock);
    kv.reply.sequence = ++g_drm.vblank_sequence;
    spin_unlock(&g_drm.lock);
    kv.reply.tval_sec = sec;
    kv.reply.tval_usec = usec;
    if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, vblank, &kv, sizeof(kv)))
        return -EFAULT;
    return 0;
}

static int drm_set_client_cap(struct xj380_drm *drm, struct drm_set_client_cap *cap, void *file_priv)
{
    (void)drm;
    if (!cap) return -EFAULT;
    struct drm_set_client_cap kc;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &kc, cap, sizeof(kc)))
        return -EFAULT;

    struct drm_file *f = (struct drm_file *)file_priv;
    if (f == NULL) return 0;
    if (kc.value)
        f->client_caps |= (1ULL << kc.capability);
    else
        f->client_caps &= ~(1ULL << kc.capability);
    return 0;
}

static int drm_gem_close(struct xj380_drm *drm, struct drm_gem_close *gc)
{
    if (!gc) return -EFAULT;
    struct drm_gem_close kc;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &kc, gc, sizeof(kc)))
        return -EFAULT;

    int idx = dumb_find_by_handle(drm, kc.handle);
    if (idx < 0) return -EINVAL;

    write_serial_fmt("[drm-trace] gem-close handle=%u refs=%u no-op\n", kc.handle, drm->dumb[idx].refs);
    return 0;
}

static size_t drm_dumb_buffer_size(const struct dumb_buffer *buffer)
{
    if (buffer == NULL || !buffer->used) return 0;
    return (size_t)buffer->pitch * (size_t)buffer->height;
}

static void drm_close_exported_fd(pcb_t process, int fd)
{
    if (process == NULL || process->file_open == NULL || fd < 0) return;
    fd_file_handle *handle = (fd_file_handle *)queue_remove_at(process->file_open, (size_t)fd);
    if (handle == NULL) return;
    vfs_close(handle->node);
    free(handle);
}

static int drm_prime_remember_fd(struct xj380_drm *drm, pcb_t process, int fd, vfs_node_t node, uint32_t handle)
{
    if (drm == NULL || process == NULL || node == NULL || fd < 0) return -EINVAL;

    spin_lock(&drm->lock);
    int free_slot = -1;
    for (int i = 0; i < MAX_PRIME_FDS; i++)
    {
        if (!drm->prime_fds[i].used)
        {
            if (free_slot < 0) free_slot = i;
            continue;
        }
        if (drm->prime_fds[i].node == node && drm->prime_fds[i].inode == node->inode)
        {
            drm->prime_fds[i].process = process;
            drm->prime_fds[i].fd      = fd;
            drm->prime_fds[i].handle  = handle;
            spin_unlock(&drm->lock);
            return 0;
        }
    }

    if (free_slot < 0)
    {
        spin_unlock(&drm->lock);
        return -EMFILE;
    }

    drm->prime_fds[free_slot].used    = true;
    drm->prime_fds[free_slot].process = process;
    drm->prime_fds[free_slot].node    = node;
    drm->prime_fds[free_slot].inode   = node->inode;
    drm->prime_fds[free_slot].fd      = fd;
    drm->prime_fds[free_slot].handle  = handle;
    spin_unlock(&drm->lock);
    return 0;
}

static int drm_prime_lookup_handle(struct xj380_drm *drm, pcb_t process, int fd, vfs_node_t node, uint32_t *handle)
{
    if (drm == NULL || process == NULL || node == NULL || handle == NULL || fd < 0) return -EINVAL;

    int ret = -EBADF;
    spin_lock(&drm->lock);
    for (int i = 0; i < MAX_PRIME_FDS; i++)
    {
        struct prime_fd_entry *entry = &drm->prime_fds[i];
        if (!entry->used) continue;
        if (entry->node != node || entry->inode != node->inode) continue;
        *handle = entry->handle;
        ret = 0;
        break;
    }
    spin_unlock(&drm->lock);
    return ret;
}

static int drm_prime_handle_to_fd(struct xj380_drm *drm, struct drm_prime_handle *prime)
{
    if (!prime) return -EFAULT;
    struct drm_prime_handle kp;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &kp, prime, sizeof(kp)))
        return -EFAULT;

    if (kp.flags & ~(DRM_CLOEXEC | DRM_RDWR)) return -EINVAL;

    int idx = dumb_find_by_handle(drm, kp.handle);
    if (idx < 0) return -ENOENT;

    size_t size = drm_dumb_buffer_size(&drm->dumb[idx]);
    uint32_t fd_flags = 0;
    if (kp.flags & DRM_CLOEXEC) fd_flags |= O_CLOEXEC;
    if (kp.flags & DRM_RDWR) fd_flags |= O_RDWR;
    int fd = anonfs_create_phys_memfd("xj380-dma-buf", drm->dumb[idx].phys_addr, size, fd_flags);
    if (fd < 0) return fd;

    pcb_t process = get_current_task()->parent_group;
    fd_file_handle *fd_handle = (fd_file_handle *)queue_get(process->file_open, (size_t)fd);
    if (fd_handle == NULL || fd_handle->node == NULL)
    {
        drm_close_exported_fd(process, fd);
        return -EBADF;
    }

    int remember = drm_prime_remember_fd(drm, process, fd, fd_handle->node, kp.handle);
    if (remember < 0)
    {
        drm_close_exported_fd(process, fd);
        return remember;
    }

    kp.fd = fd;
    if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, prime, &kp, sizeof(kp)))
    {
        drm_close_exported_fd(process, fd);
        return -EFAULT;
    }
    return 0;
}

static int drm_prime_fd_to_handle(struct xj380_drm *drm, struct drm_prime_handle *prime)
{
    if (!prime) return -EFAULT;
    struct drm_prime_handle kp;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &kp, prime, sizeof(kp)))
        return -EFAULT;

    pcb_t process = get_current_task()->parent_group;
    if (kp.fd < 0 || process == NULL || process->file_open == NULL) return -EBADF;

    fd_file_handle *fd_handle = (fd_file_handle *)queue_get(process->file_open, (size_t)kp.fd);
    if (fd_handle == NULL || fd_handle->node == NULL) return -EBADF;

    uint32_t handle = 0;
    int ret = drm_prime_lookup_handle(drm, process, kp.fd, fd_handle->node, &handle);
    if (ret < 0) return ret;

    int idx = dumb_find_by_handle(drm, handle);
    if (idx < 0) return -ENOENT;

    kp.handle = handle;
    if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, prime, &kp, sizeof(kp)))
        return -EFAULT;
    return 0;
}

static int drm_mode_create_lease(struct drm_mode_create_lease *lease)
{
    if (!lease) return -EFAULT;
    struct drm_mode_create_lease kl;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &kl, lease, sizeof(kl)))
        return -EFAULT;

    /*
     * XJ380 exposes a single primary DRM device and does not implement object
     * ownership splitting yet. Report the ioctl as present but unsupported so
     * userspace can fall back instead of treating the command as missing.
     */
    return -EOPNOTSUPP;
}

static int drm_mode_list_lessees(struct drm_mode_list_lessees *lessees)
{
    if (!lessees) return -EFAULT;
    struct drm_mode_list_lessees kl;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &kl, lessees, sizeof(kl)))
        return -EFAULT;

    kl.count_lessees = 0;
    if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, lessees, &kl, sizeof(kl)))
        return -EFAULT;
    return 0;
}

static int drm_mode_get_lease(struct drm_mode_get_lease *lease)
{
    if (!lease) return -EFAULT;
    struct drm_mode_get_lease kl;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &kl, lease, sizeof(kl)))
        return -EFAULT;

    kl.count_objects = 0;
    if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, lease, &kl, sizeof(kl)))
        return -EFAULT;
    return 0;
}

static int drm_mode_revoke_lease(struct drm_mode_revoke_lease *lease)
{
    if (!lease) return -EFAULT;
    struct drm_mode_revoke_lease kl;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &kl, lease, sizeof(kl)))
        return -EFAULT;
    return -ENOENT;
}

static int drm_mode_getresources(struct xj380_drm *drm, struct drm_mode_card_res *res)
{
    if (!res) return -EFAULT;
    struct drm_mode_card_res kr;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &kr, res, sizeof(kr)))
        return -EFAULT;

    uint32_t fb_cap        = kr.count_fbs;
    uint32_t crtc_cap      = kr.count_crtcs;
    uint32_t connector_cap = kr.count_connectors;
    uint32_t encoder_cap   = kr.count_encoders;

    kr.count_fbs        = 0;
    kr.count_crtcs      = 1;
    kr.count_connectors = 1;
    kr.count_encoders   = 1;
    kr.min_width = kr.min_height = 0;
    kr.max_width = drm->fb_width;
    kr.max_height = drm->fb_height;

    uint32_t crtc_id = drm->crtc_id;
    uint32_t conn_id = drm->connector_id;
    uint32_t enc_id  = drm->encoder_id;

    if (kr.crtc_id_ptr && drm_copy_counted_array_to_user(kr.crtc_id_ptr, &crtc_id, crtc_cap,
                                                         kr.count_crtcs, sizeof(crtc_id)) != 0)
        return -EFAULT;
    if (kr.connector_id_ptr && drm_copy_counted_array_to_user(kr.connector_id_ptr, &conn_id, connector_cap,
                                                              kr.count_connectors, sizeof(conn_id)) != 0)
        return -EFAULT;
    if (kr.encoder_id_ptr && drm_copy_counted_array_to_user(kr.encoder_id_ptr, &enc_id, encoder_cap,
                                                            kr.count_encoders, sizeof(enc_id)) != 0)
        return -EFAULT;
    (void)fb_cap;

    if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, res, &kr, sizeof(kr)))
        return -EFAULT;
    return 0;
}

static int drm_mode_getcrtc(struct xj380_drm *drm, struct drm_mode_crtc *crtc)
{
    if (!crtc) return -EFAULT;
    struct drm_mode_crtc kc;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &kc, crtc, sizeof(kc)))
        return -EFAULT;

    if (kc.crtc_id != drm->crtc_id) return -ENOENT;

    kc.fb_id      = drm->current_fb_id;
    kc.x = kc.y   = 0;
    kc.gamma_size = 0;
    kc.mode_valid = 1;
    kc.mode       = drm->mode;

    if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, crtc, &kc, sizeof(kc)))
        return -EFAULT;
    return 0;
}

static int drm_mode_setcrtc(struct xj380_drm *drm, struct drm_mode_crtc *crtc)
{
    if (!crtc) return -EFAULT;
    struct drm_mode_crtc kc;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &kc, crtc, sizeof(kc)))
        return -EFAULT;

    if (kc.crtc_id != 0 && kc.crtc_id != drm->crtc_id) return -ENOENT;

    if (kc.fb_id == 0) {
        drm->current_fb_id = 0;
        return 0;
    }

    struct drm_fb_obj *fb = fb_find_by_id(drm, kc.fb_id);
    if (!fb) return -ENOENT;

    drm->current_fb_id = kc.fb_id;
    return present(drm, kc.fb_id);
}

static int drm_mode_page_flip(struct xj380_drm *drm, struct drm_mode_crtc_page_flip *flip)
{
    if (!flip) return -EFAULT;
    struct drm_mode_crtc_page_flip kf;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &kf, flip, sizeof(kf)))
        return -EFAULT;

    if (kf.crtc_id != drm->crtc_id) return -ENOENT;

    struct drm_fb_obj *fb = fb_find_by_id(drm, kf.fb_id);
    if (!fb) return -ENOENT;

    uint32_t old_fb = drm->current_fb_id;
    drm->current_fb_id = kf.fb_id;

    int ret = present(drm, kf.fb_id);
    if (ret) {
        drm->current_fb_id = old_fb;
        return ret;
    }

    if (kf.flags & DRM_MODE_PAGE_FLIP_EVENT)
        ev_queue(drm, kf.user_data);

    return 0;
}

static int drm_mode_getconnector(struct xj380_drm *drm, struct drm_mode_get_connector *conn)
{
    if (!conn) return -EFAULT;
    struct drm_mode_get_connector kc;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &kc, conn, sizeof(kc)))
        return -EFAULT;

    if (kc.connector_id != drm->connector_id) return -ENOENT;

    uint32_t modes_cap    = kc.count_modes;
    uint32_t props_cap    = kc.count_props;
    uint32_t encoders_cap = kc.count_encoders;

    kc.encoder_id       = drm->encoder_id;
    kc.connector_type   = DRM_MODE_CONNECTOR_VIRTUAL;
    kc.connector_type_id = 1;
    kc.connection       = DRM_MODE_CONNECTED;
    kc.mm_width         = (drm->fb_width * 264u) / 1000u;
    kc.mm_height        = (drm->fb_height * 264u) / 1000u;
    kc.subpixel         = DRM_MODE_SUBPIXEL_UNKNOWN;
    kc.count_modes      = 1;
    kc.count_encoders   = 1;
    kc.count_props      = 3;

    /* Write mode */
    if (kc.modes_ptr) {
        struct drm_mode_modeinfo m = drm->mode;
        if (drm_copy_counted_array_to_user(kc.modes_ptr, &m, modes_cap, kc.count_modes, sizeof(m)) != 0)
            return -EFAULT;
    }

    /* Write encoder id */
    if (kc.encoders_ptr) {
        uint32_t eid = drm->encoder_id;
        if (drm_copy_counted_array_to_user(kc.encoders_ptr, &eid, encoders_cap, kc.count_encoders, sizeof(eid)) != 0)
            return -EFAULT;
    }

    /* Property IDs + values: DPMS, CRTC_ID */
    if (kc.props_ptr) {
        uint32_t pids[3] = { DRM_PROP_CONNECTOR_DPMS, DRM_PROP_CONNECTOR_EDID, DRM_PROP_CONNECTOR_CRTC_ID };
        if (drm_copy_counted_array_to_user(kc.props_ptr, pids, props_cap, kc.count_props, sizeof(pids[0])) != 0)
            return -EFAULT;
    }
    if (kc.prop_values_ptr) {
        uint64_t pvals[3] = { DRM_MODE_DPMS_ON, drm_connector_edid_blob_id(drm->connector_id), drm->crtc_id };
        if (drm_copy_counted_array_to_user(kc.prop_values_ptr, pvals, props_cap, kc.count_props, sizeof(pvals[0])) != 0)
            return -EFAULT;
    }

    if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, conn, &kc, sizeof(kc)))
        return -EFAULT;
    return 0;
}

static int drm_mode_getencoder(struct xj380_drm *drm, struct drm_mode_get_encoder *enc)
{
    if (!enc) return -EFAULT;
    struct drm_mode_get_encoder ke;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &ke, enc, sizeof(ke)))
        return -EFAULT;

    if (ke.encoder_id != drm->encoder_id) return -ENOENT;

    ke.encoder_type   = DRM_MODE_ENCODER_VIRTUAL;
    ke.crtc_id        = drm->crtc_id;
    ke.possible_crtcs = 1;
    ke.possible_clones = 0;

    if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, enc, &ke, sizeof(ke)))
        return -EFAULT;
    return 0;
}

static int drm_mode_create_dumb(struct xj380_drm *drm, struct drm_mode_create_dumb *args)
{
    if (!args) return -EFAULT;
    struct drm_mode_create_dumb ka;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &ka, args, sizeof(ka)))
        return -EFAULT;

    if (ka.width == 0 || ka.height == 0) return -EINVAL;
    if (ka.bpp == 0) ka.bpp = 32;
    uint32_t bpp = ka.bpp / 8;
    if (bpp == 0) return -EINVAL;

    ka.pitch = PADDING_UP(ka.width * bpp, 64);
    ka.size  = (uint64_t)ka.pitch * ka.height;

    int idx = dumb_find_free(drm);
    if (idx < 0) return -ENOSPC;

    size_t pages = PADDING_UP((size_t)ka.size, PAGE_SIZE) / PAGE_SIZE;
    uint64_t phys = alloc_frames(pages);
    if (phys == 0) return -ENOMEM;

    uint8_t *virt = (uint8_t *)driver_phys_to_virt(phys);
    page_map_range(get_current_directory(), (uint64_t)virt, phys,
                   pages * PAGE_SIZE, KERNEL_PTE_FLAGS);
    memset(virt, 0, pages * PAGE_SIZE);

    uint32_t handle = (uint32_t)(idx + 1);
    drm->dumb[idx].used      = true;
    drm->dumb[idx].phys_addr = phys;
    drm->dumb[idx].width     = ka.width;
    drm->dumb[idx].height    = ka.height;
    drm->dumb[idx].pitch     = ka.pitch;
    drm->dumb[idx].handle    = handle;
    drm->dumb[idx].refs      = 1;

    ka.handle = handle;
    if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, args, &ka, sizeof(ka)))
        return -EFAULT;
    return 0;
}

static int drm_mode_map_dumb(struct xj380_drm *drm, struct drm_mode_map_dumb *args)
{
    if (!args) return -EFAULT;
    struct drm_mode_map_dumb ka;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &ka, args, sizeof(ka)))
        return -EFAULT;

    int idx = dumb_find_by_handle(drm, ka.handle);
    if (idx < 0) return -EINVAL;

    ka.offset = drm->dumb[idx].phys_addr;
    if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, args, &ka, sizeof(ka)))
        return -EFAULT;
    return 0;
}

static int drm_mode_destroy_dumb(struct xj380_drm *drm, struct drm_mode_destroy_dumb *args)
{
    if (!args) return -EFAULT;
    struct drm_mode_destroy_dumb ka;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &ka, args, sizeof(ka)))
        return -EFAULT;

    int idx = dumb_find_by_handle(drm, ka.handle);
    if (idx < 0) return -EINVAL;

    (void)idx;
    return dumb_put(drm, ka.handle, "destroy-dumb");
}

static int drm_mode_addfb(struct xj380_drm *drm, struct drm_mode_fb_cmd *args)
{
    if (!args) return -EFAULT;
    struct drm_mode_fb_cmd ka;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &ka, args, sizeof(ka)))
        return -EFAULT;

    int idx = dumb_find_by_handle(drm, ka.handle);
    if (idx < 0) return -EINVAL;

    int fi = fb_find_free(drm);
    if (fi < 0) return -ENOSPC;

    int get_ret = dumb_get(drm, ka.handle);
    if (get_ret < 0) return get_ret;

    uint32_t fb_id = drm->next_fb_id++;
    drm->fbs[fi].used   = true;
    drm->fbs[fi].fb_id  = fb_id;
    drm->fbs[fi].handle = ka.handle;
    drm->fbs[fi].width  = ka.width;
    drm->fbs[fi].height = ka.height;
    drm->fbs[fi].pitch  = ka.pitch;
    drm->fbs[fi].offset = 0;
    drm->fbs[fi].format = DRM_FORMAT_XRGB8888;

    ka.fb_id = fb_id;
    if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, args, &ka, sizeof(ka)))
    {
        memset(&drm->fbs[fi], 0, sizeof(drm->fbs[fi]));
        dumb_put(drm, ka.handle, "addfb-fault");
        return -EFAULT;
    }
    return 0;
}

static int drm_mode_addfb2(struct xj380_drm *drm, struct drm_mode_fb_cmd2 *args)
{
    if (!args) return -EFAULT;
    struct drm_mode_fb_cmd2 ka;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &ka, args, sizeof(ka)))
        return -EFAULT;

    if (ka.handles[0] == 0) return -EINVAL;

    int idx = dumb_find_by_handle(drm, ka.handles[0]);
    if (idx < 0) return -EINVAL;

    int fi = fb_find_free(drm);
    if (fi < 0) return -ENOSPC;

    int get_ret = dumb_get(drm, ka.handles[0]);
    if (get_ret < 0) return get_ret;

    uint32_t fb_id = drm->next_fb_id++;
    drm->fbs[fi].used   = true;
    drm->fbs[fi].fb_id  = fb_id;
    drm->fbs[fi].handle = ka.handles[0];
    drm->fbs[fi].width  = ka.width;
    drm->fbs[fi].height = ka.height;
    drm->fbs[fi].pitch  = ka.pitches[0] ? ka.pitches[0] : drm->dumb[idx].pitch;
    drm->fbs[fi].offset = ka.offsets[0];
    drm->fbs[fi].format = ka.pixel_format;

    ka.fb_id = fb_id;
    if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, args, &ka, sizeof(ka)))
    {
        memset(&drm->fbs[fi], 0, sizeof(drm->fbs[fi]));
        dumb_put(drm, ka.handles[0], "addfb2-fault");
        return -EFAULT;
    }
    return 0;
}

static int drm_mode_getfb(struct xj380_drm *drm, struct drm_mode_fb_cmd *args)
{
    if (!args) return -EFAULT;
    struct drm_mode_fb_cmd ka;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &ka, args, sizeof(ka)))
        return -EFAULT;

    struct drm_fb_obj *fb = fb_find_by_id(drm, ka.fb_id);
    if (!fb) return -ENOENT;

    ka.width  = fb->width;
    ka.height = fb->height;
    ka.pitch  = fb->pitch;
    ka.bpp    = 32;
    ka.depth  = 24;
    ka.handle = fb->handle;

    if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, args, &ka, sizeof(ka)))
        return -EFAULT;
    return 0;
}

static int drm_mode_rmfb(struct xj380_drm *drm, uint32_t *fb_id)
{
    if (!fb_id) return -EFAULT;
    uint32_t kfb;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &kfb, fb_id, sizeof(kfb)))
        return -EFAULT;

    struct drm_fb_obj *fb = fb_find_by_id(drm, kfb);
    if (!fb) return -ENOENT;

    if (drm->current_fb_id == kfb) drm->current_fb_id = 0;
    uint32_t handle = fb->handle;
    memset(fb, 0, sizeof(*fb));
    dumb_put(drm, handle, "rmfb");
    return 0;
}

static int drm_mode_closefb(struct xj380_drm *drm, struct drm_mode_closefb *args)
{
    if (!args) return -EFAULT;
    struct drm_mode_closefb ka;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &ka, args, sizeof(ka)))
        return -EFAULT;

    struct drm_fb_obj *fb = fb_find_by_id(drm, ka.fb_id);
    if (!fb) return -ENOENT;

    if (drm->current_fb_id == ka.fb_id) drm->current_fb_id = 0;
    uint32_t handle = fb->handle;
    memset(fb, 0, sizeof(*fb));
    dumb_put(drm, handle, "closefb");
    return 0;
}

static int drm_mode_getplane_resources(struct xj380_drm *drm, struct drm_mode_get_plane_res *args)
{
    if (!args) return -EFAULT;
    struct drm_mode_get_plane_res ka;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &ka, args, sizeof(ka)))
        return -EFAULT;

    uint32_t plane_cap = ka.count_planes;

    ka.count_planes = 1;
    uint32_t pid = drm->plane_id;
    if (ka.plane_id_ptr && drm_copy_counted_array_to_user(ka.plane_id_ptr, &pid, plane_cap,
                                                          ka.count_planes, sizeof(pid)) != 0)
        return -EFAULT;

    if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, args, &ka, sizeof(ka)))
        return -EFAULT;
    return 0;
}

static int drm_mode_getplane(struct xj380_drm *drm, struct drm_mode_get_plane *args)
{
    if (!args) return -EFAULT;
    struct drm_mode_get_plane ka;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &ka, args, sizeof(ka)))
        return -EFAULT;

    if (ka.plane_id != drm->plane_id) return -ENOENT;

    uint32_t formats_cap = ka.count_format_types;

    ka.crtc_id       = drm->crtc_id;
    ka.fb_id         = drm->current_fb_id;
    ka.possible_crtcs = 1;
    ka.gamma_size    = 0;
    ka.count_format_types = 4;
    uint32_t formats[4] = { DRM_FORMAT_XRGB8888, DRM_FORMAT_ARGB8888,
                            DRM_FORMAT_XBGR8888, DRM_FORMAT_ABGR8888 };
    if (ka.format_type_ptr && drm_copy_counted_array_to_user(ka.format_type_ptr, formats, formats_cap,
                                                             ka.count_format_types, sizeof(formats[0])) != 0)
        return -EFAULT;

    if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, args, &ka, sizeof(ka)))
        return -EFAULT;
    return 0;
}

static int drm_mode_setplane(struct xj380_drm *drm, struct drm_mode_set_plane *args)
{
    if (!args) return -EFAULT;
    struct drm_mode_set_plane ka;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &ka, args, sizeof(ka)))
        return -EFAULT;

    if (ka.plane_id != drm->plane_id) return -ENOENT;
    if (ka.crtc_id != 0 && ka.crtc_id != drm->crtc_id) return -ENOENT;
    if (ka.fb_id == 0) {
        drm->current_fb_id = 0;
        return 0;
    }

    struct drm_fb_obj *fb = fb_find_by_id(drm, ka.fb_id);
    if (!fb) return -ENOENT;

    int ret = present(drm, ka.fb_id);
    if (ret) return ret;
    drm->current_fb_id = ka.fb_id;
    return 0;
}

static int drm_mode_dirtyfb(struct xj380_drm *drm, struct drm_mode_fb_dirty_cmd *args)
{
    if (!args) return -EFAULT;
    struct drm_mode_fb_dirty_cmd ka;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &ka, args, sizeof(ka)))
        return -EFAULT;

    if (fb_find_by_id(drm, ka.fb_id) == NULL) return -ENOENT;
    if (drm->current_fb_id != ka.fb_id) return 0;
    return present(drm, ka.fb_id);
}

static int drm_mode_getproperty(struct xj380_drm *drm, struct drm_mode_get_property *args)
{
    if (!args) return -EFAULT;
    struct drm_mode_get_property kp;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &kp, args, sizeof(kp)))
        return -EFAULT;

    uint32_t values_cap = kp.count_values;
    uint32_t enum_blobs_cap = kp.count_enum_blobs;

    memset(&kp.flags, 0, sizeof(kp) - offsetof(struct drm_mode_get_property, flags));
    kp.count_values = 0;
    kp.count_enum_blobs = 0;

    switch (kp.prop_id) {
    case DRM_PROP_CRTC_ACTIVE:
        strncpy(kp.name, "ACTIVE", DRM_PROP_NAME_LEN - 1);
        kp.flags = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC;
        kp.count_values = 2;
        if (kp.values_ptr) {
            uint64_t vals[2] = {0, 1};
            size_t copy_values = MIN((size_t)values_cap, (size_t)kp.count_values);
            if (drm_copy_to_user_ptr(kp.values_ptr, vals, copy_values * sizeof(uint64_t)) != 0)
                return -EFAULT;
        }
        break;
    case DRM_PROP_CRTC_MODE_ID:
        strncpy(kp.name, "MODE_ID", DRM_PROP_NAME_LEN - 1);
        kp.flags = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_ATOMIC;
        break;
    case DRM_PROP_CONNECTOR_DPMS:
        strncpy(kp.name, "DPMS", DRM_PROP_NAME_LEN - 1);
        kp.flags = DRM_MODE_PROP_ENUM;
        kp.count_enum_blobs = 4;
        if (kp.enum_blob_ptr) {
            struct drm_mode_property_enum enums[4];
            memset(enums, 0, sizeof(enums));
            strncpy(enums[0].name, "On", DRM_PROP_NAME_LEN - 1);
            enums[0].value = DRM_MODE_DPMS_ON;
            strncpy(enums[1].name, "Standby", DRM_PROP_NAME_LEN - 1);
            enums[1].value = DRM_MODE_DPMS_STANDBY;
            strncpy(enums[2].name, "Suspend", DRM_PROP_NAME_LEN - 1);
            enums[2].value = DRM_MODE_DPMS_SUSPEND;
            strncpy(enums[3].name, "Off", DRM_PROP_NAME_LEN - 1);
            enums[3].value = DRM_MODE_DPMS_OFF;
            size_t copy_enums = MIN((size_t)enum_blobs_cap, (size_t)kp.count_enum_blobs);
            if (drm_copy_to_user_ptr(kp.enum_blob_ptr, enums, copy_enums * sizeof(struct drm_mode_property_enum)) !=
                0)
                return -EFAULT;
        }
        break;
    case DRM_PROP_CONNECTOR_EDID:
        strncpy(kp.name, "EDID", DRM_PROP_NAME_LEN - 1);
        kp.flags = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_IMMUTABLE;
        break;
    case DRM_PROP_CONNECTOR_CRTC_ID:
        strncpy(kp.name, "CRTC_ID", DRM_PROP_NAME_LEN - 1);
        kp.flags = DRM_MODE_PROP_OBJECT | DRM_MODE_PROP_ATOMIC;
        kp.count_values = 1;
        if (kp.values_ptr) {
            uint64_t vals[1] = {DRM_MODE_OBJECT_CRTC};
            size_t copy_values = MIN((size_t)values_cap, (size_t)kp.count_values);
            if (drm_copy_to_user_ptr(kp.values_ptr, vals, copy_values * sizeof(uint64_t)) != 0)
                return -EFAULT;
        }
        break;
    case DRM_PROP_PLANE_TYPE:
        strncpy(kp.name, "type", DRM_PROP_NAME_LEN - 1);
        kp.flags = DRM_MODE_PROP_ENUM | DRM_MODE_PROP_IMMUTABLE;
        kp.count_enum_blobs = 3;
        if (kp.enum_blob_ptr) {
            struct drm_mode_property_enum enums[3];
            memset(enums, 0, sizeof(enums));
            strncpy(enums[0].name, "Primary", DRM_PROP_NAME_LEN - 1);
            enums[0].value = DRM_PLANE_TYPE_PRIMARY;
            strncpy(enums[1].name, "Overlay", DRM_PROP_NAME_LEN - 1);
            enums[1].value = DRM_PLANE_TYPE_OVERLAY;
            strncpy(enums[2].name, "Cursor", DRM_PROP_NAME_LEN - 1);
            enums[2].value = DRM_PLANE_TYPE_CURSOR;
            size_t copy_enums = MIN((size_t)enum_blobs_cap, (size_t)kp.count_enum_blobs);
            if (drm_copy_to_user_ptr(kp.enum_blob_ptr, enums, copy_enums * sizeof(struct drm_mode_property_enum)) !=
                0)
                return -EFAULT;
        }
        break;
    case DRM_PROP_PLANE_IN_FORMATS:
        strncpy(kp.name, "IN_FORMATS", DRM_PROP_NAME_LEN - 1);
        kp.flags = DRM_MODE_PROP_BLOB | DRM_MODE_PROP_IMMUTABLE;
        break;
    case DRM_PROP_PLANE_FB_ID:
        strncpy(kp.name, "FB_ID", DRM_PROP_NAME_LEN - 1);
        kp.flags = DRM_MODE_PROP_OBJECT | DRM_MODE_PROP_ATOMIC;
        kp.count_values = 1;
        if (kp.values_ptr) {
            uint64_t vals[1] = {DRM_MODE_OBJECT_FB};
            size_t copy_values = MIN((size_t)values_cap, (size_t)kp.count_values);
            if (drm_copy_to_user_ptr(kp.values_ptr, vals, copy_values * sizeof(uint64_t)) != 0)
                return -EFAULT;
        }
        break;
    case DRM_PROP_PLANE_CRTC_ID:
        strncpy(kp.name, "CRTC_ID", DRM_PROP_NAME_LEN - 1);
        kp.flags = DRM_MODE_PROP_OBJECT | DRM_MODE_PROP_ATOMIC;
        kp.count_values = 1;
        if (kp.values_ptr) {
            uint64_t vals[1] = {DRM_MODE_OBJECT_CRTC};
            size_t copy_values = MIN((size_t)values_cap, (size_t)kp.count_values);
            if (drm_copy_to_user_ptr(kp.values_ptr, vals, copy_values * sizeof(uint64_t)) != 0)
                return -EFAULT;
        }
        break;
    case DRM_PROP_PLANE_SRC_X:
        strncpy(kp.name, "SRC_X", DRM_PROP_NAME_LEN - 1);
        kp.flags = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC;
        kp.count_values = 2;
        if (kp.values_ptr) {
            uint64_t vals[2] = {0, DRM_U32_MAX};
            size_t copy_values = MIN((size_t)values_cap, (size_t)kp.count_values);
            if (drm_copy_to_user_ptr(kp.values_ptr, vals, copy_values * sizeof(uint64_t)) != 0)
                return -EFAULT;
        }
        break;
    case DRM_PROP_PLANE_SRC_Y:
        strncpy(kp.name, "SRC_Y", DRM_PROP_NAME_LEN - 1);
        kp.flags = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC;
        kp.count_values = 2;
        if (kp.values_ptr) {
            uint64_t vals[2] = {0, DRM_U32_MAX};
            size_t copy_values = MIN((size_t)values_cap, (size_t)kp.count_values);
            if (drm_copy_to_user_ptr(kp.values_ptr, vals, copy_values * sizeof(uint64_t)) != 0)
                return -EFAULT;
        }
        break;
    case DRM_PROP_PLANE_SRC_W:
        strncpy(kp.name, "SRC_W", DRM_PROP_NAME_LEN - 1);
        kp.flags = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC;
        kp.count_values = 2;
        if (kp.values_ptr) {
            uint64_t vals[2] = {0, DRM_U32_MAX};
            size_t copy_values = MIN((size_t)values_cap, (size_t)kp.count_values);
            if (drm_copy_to_user_ptr(kp.values_ptr, vals, copy_values * sizeof(uint64_t)) != 0)
                return -EFAULT;
        }
        break;
    case DRM_PROP_PLANE_SRC_H:
        strncpy(kp.name, "SRC_H", DRM_PROP_NAME_LEN - 1);
        kp.flags = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC;
        kp.count_values = 2;
        if (kp.values_ptr) {
            uint64_t vals[2] = {0, DRM_U32_MAX};
            size_t copy_values = MIN((size_t)values_cap, (size_t)kp.count_values);
            if (drm_copy_to_user_ptr(kp.values_ptr, vals, copy_values * sizeof(uint64_t)) != 0)
                return -EFAULT;
        }
        break;
    case DRM_PROP_PLANE_CRTC_X:
        strncpy(kp.name, "CRTC_X", DRM_PROP_NAME_LEN - 1);
        kp.flags = DRM_MODE_PROP_SIGNED_RANGE | DRM_MODE_PROP_ATOMIC;
        kp.count_values = 2;
        if (kp.values_ptr) {
            uint64_t vals[2] = {(uint64_t)(-(1LL << 31)), (uint64_t)((1LL << 31) - 1)};
            size_t copy_values = MIN((size_t)values_cap, (size_t)kp.count_values);
            if (drm_copy_to_user_ptr(kp.values_ptr, vals, copy_values * sizeof(uint64_t)) != 0)
                return -EFAULT;
        }
        break;
    case DRM_PROP_PLANE_CRTC_Y:
        strncpy(kp.name, "CRTC_Y", DRM_PROP_NAME_LEN - 1);
        kp.flags = DRM_MODE_PROP_SIGNED_RANGE | DRM_MODE_PROP_ATOMIC;
        kp.count_values = 2;
        if (kp.values_ptr) {
            uint64_t vals[2] = {(uint64_t)(-(1LL << 31)), (uint64_t)((1LL << 31) - 1)};
            size_t copy_values = MIN((size_t)values_cap, (size_t)kp.count_values);
            if (drm_copy_to_user_ptr(kp.values_ptr, vals, copy_values * sizeof(uint64_t)) != 0)
                return -EFAULT;
        }
        break;
    case DRM_PROP_PLANE_CRTC_W:
        strncpy(kp.name, "CRTC_W", DRM_PROP_NAME_LEN - 1);
        kp.flags = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC;
        kp.count_values = 2;
        if (kp.values_ptr) {
            uint64_t vals[2] = {0, 8192};
            size_t copy_values = MIN((size_t)values_cap, (size_t)kp.count_values);
            if (drm_copy_to_user_ptr(kp.values_ptr, vals, copy_values * sizeof(uint64_t)) != 0)
                return -EFAULT;
        }
        break;
    case DRM_PROP_PLANE_CRTC_H:
        strncpy(kp.name, "CRTC_H", DRM_PROP_NAME_LEN - 1);
        kp.flags = DRM_MODE_PROP_RANGE | DRM_MODE_PROP_ATOMIC;
        kp.count_values = 2;
        if (kp.values_ptr) {
            uint64_t vals[2] = {0, 8192};
            size_t copy_values = MIN((size_t)values_cap, (size_t)kp.count_values);
            if (drm_copy_to_user_ptr(kp.values_ptr, vals, copy_values * sizeof(uint64_t)) != 0)
                return -EFAULT;
        }
        break;
    default:
        return -EINVAL;
    }

    if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, args, &kp, sizeof(kp)))
        return -EFAULT;
    return 0;
}

static int drm_mode_createpropblob(struct xj380_drm *drm, struct drm_mode_create_blob *args)
{
    if (!args) return -EFAULT;
    struct drm_mode_create_blob kbl;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &kbl, args, sizeof(kbl)))
        return -EFAULT;

    if (kbl.length == 0 || kbl.length > 65536) return -EINVAL;

    int bi = blob_find_free(drm);
    if (bi < 0) return -ENOSPC;

    void *data = malloc(kbl.length);
    if (!data) return -ENOMEM;

    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, data,
            (void *)(uintptr_t)kbl.data, kbl.length)) {
        free(data);
        return -EFAULT;
    }

    uint32_t blob_id = (uint32_t)(bi + 1);
    drm->blobs[bi].used    = true;
    drm->blobs[bi].blob_id = blob_id;
    drm->blobs[bi].data    = data;
    drm->blobs[bi].length  = kbl.length;

    kbl.blob_id = blob_id;
    if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, args, &kbl, sizeof(kbl)))
        return -EFAULT;
    return 0;
}

static int drm_mode_destroypropblob(struct xj380_drm *drm, struct drm_mode_destroy_blob *args)
{
    if (!args) return -EFAULT;
    struct drm_mode_destroy_blob kbl;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &kbl, args, sizeof(kbl)))
        return -EFAULT;

    struct prop_blob *blob = blob_find_by_id(drm, kbl.blob_id);
    if (!blob) return -ENOENT;

    if (blob->data) free(blob->data);
    memset(blob, 0, sizeof(*blob));
    return 0;
}

static int drm_mode_obj_getproperties(struct xj380_drm *drm, struct drm_mode_obj_get_properties *args)
{
    if (!args) return -EFAULT;
    struct drm_mode_obj_get_properties kp;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &kp, args, sizeof(kp)))
        return -EFAULT;

    uint32_t count = 0;
    uint32_t prop_ids[16];
    uint64_t prop_vals[16];

    if (kp.obj_type == DRM_MODE_OBJECT_ANY) {
        if (kp.obj_id == drm->crtc_id)
            kp.obj_type = DRM_MODE_OBJECT_CRTC;
        else if (kp.obj_id == drm->connector_id)
            kp.obj_type = DRM_MODE_OBJECT_CONNECTOR;
        else if (kp.obj_id == drm->encoder_id)
            kp.obj_type = DRM_MODE_OBJECT_ENCODER;
        else if (kp.obj_id == drm->plane_id)
            kp.obj_type = DRM_MODE_OBJECT_PLANE;
        else
            return -ENOENT;
    }

    if (kp.obj_type == DRM_MODE_OBJECT_CRTC && kp.obj_id == drm->crtc_id) {
        count = 2;
        prop_ids[0] = DRM_PROP_CRTC_ACTIVE;  prop_vals[0] = 1;
        prop_ids[1] = DRM_PROP_CRTC_MODE_ID; prop_vals[1] = drm_crtc_mode_blob_id(drm->crtc_id);
    } else if (kp.obj_type == DRM_MODE_OBJECT_CONNECTOR && kp.obj_id == drm->connector_id) {
        count = 3;
        prop_ids[0] = DRM_PROP_CONNECTOR_DPMS;    prop_vals[0] = DRM_MODE_DPMS_ON;
        prop_ids[1] = DRM_PROP_CONNECTOR_EDID;    prop_vals[1] = drm_connector_edid_blob_id(drm->connector_id);
        prop_ids[2] = DRM_PROP_CONNECTOR_CRTC_ID; prop_vals[2] = drm->crtc_id;
    } else if (kp.obj_type == DRM_MODE_OBJECT_PLANE && kp.obj_id == drm->plane_id) {
        count = 12;
        prop_ids[0] = DRM_PROP_PLANE_TYPE;    prop_vals[0] = DRM_PLANE_TYPE_PRIMARY;
        prop_ids[1] = DRM_PROP_PLANE_IN_FORMATS;
        prop_vals[1] = drm_plane_in_formats_blob_id(drm->plane_id);
        prop_ids[2]  = DRM_PROP_PLANE_FB_ID;   prop_vals[2]  = drm->current_fb_id;
        prop_ids[3]  = DRM_PROP_PLANE_CRTC_ID; prop_vals[3]  = drm->crtc_id;
        prop_ids[4]  = DRM_PROP_PLANE_SRC_X;   prop_vals[4]  = 0;
        prop_ids[5]  = DRM_PROP_PLANE_SRC_Y;   prop_vals[5]  = 0;
        prop_ids[6]  = DRM_PROP_PLANE_SRC_W;   prop_vals[6]  = (uint64_t)drm->fb_width << 16;
        prop_ids[7]  = DRM_PROP_PLANE_SRC_H;   prop_vals[7]  = (uint64_t)drm->fb_height << 16;
        prop_ids[8]  = DRM_PROP_PLANE_CRTC_X;  prop_vals[8]  = 0;
        prop_ids[9]  = DRM_PROP_PLANE_CRTC_Y;  prop_vals[9]  = 0;
        prop_ids[10] = DRM_PROP_PLANE_CRTC_W;  prop_vals[10] = drm->fb_width;
        prop_ids[11] = DRM_PROP_PLANE_CRTC_H;  prop_vals[11] = drm->fb_height;
    } else if (kp.obj_type == DRM_MODE_OBJECT_ENCODER && kp.obj_id == drm->encoder_id) {
        count = 0;
    } else {
        return -ENOENT;
    }

    kp.count_props = count;
    if (kp.props_ptr && count > 0) {
        if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir,
                (void *)(uintptr_t)kp.props_ptr, prop_ids, count * sizeof(uint32_t)))
            return -EFAULT;
    }
    if (kp.prop_values_ptr && count > 0) {
        if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir,
                (void *)(uintptr_t)kp.prop_values_ptr, prop_vals, count * sizeof(uint64_t)))
            return -EFAULT;
    }

    if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, args, &kp, sizeof(kp)))
        return -EFAULT;
    return 0;
}

static int drm_mode_atomic(struct xj380_drm *drm, struct drm_mode_atomic *args)
{
    if (!args) return -EFAULT;
    struct drm_mode_atomic ka;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &ka, args, sizeof(ka)))
        return -EFAULT;

    if (ka.flags & ~DRM_MODE_ATOMIC_FLAGS) return -EINVAL;
    if (ka.count_objs == 0) return 0;
    if (!ka.objs_ptr || !ka.count_props_ptr || !ka.props_ptr || !ka.prop_values_ptr) return -EINVAL;

    bool test_only = (ka.flags & DRM_MODE_ATOMIC_TEST_ONLY) != 0;
    bool page_flip_event = (ka.flags & DRM_MODE_PAGE_FLIP_EVENT) != 0;
    uint32_t new_fb_id = 0;
    uint64_t new_mode_blob = 0;
    struct drm_mode_modeinfo new_mode;
    bool has_new_mode = false;
    bool crtc_active = true;
    uint32_t src_x = 0;
    uint32_t src_y = 0;
    uint32_t src_w = drm->fb_width;
    uint32_t src_h = drm->fb_height;
    int32_t crtc_x = 0;
    int32_t crtc_y = 0;
    uint32_t crtc_w = drm->fb_width;
    uint32_t crtc_h = drm->fb_height;

    if (ka.count_objs > 64) return -E2BIG;

    uint32_t *obj_ids = (uint32_t *)calloc(ka.count_objs, sizeof(uint32_t));
    uint32_t *prop_counts = (uint32_t *)calloc(ka.count_objs, sizeof(uint32_t));
    uint32_t *prop_ids = NULL;
    uint64_t *prop_vals = NULL;
    if (!obj_ids || !prop_counts) {
        free(obj_ids);
        free(prop_counts);
        return -ENOMEM;
    }

    if (drm_copy_from_user_ptr(obj_ids, ka.objs_ptr, ka.count_objs * sizeof(uint32_t)) != 0 ||
        drm_copy_from_user_ptr(prop_counts, ka.count_props_ptr, ka.count_objs * sizeof(uint32_t)) != 0) {
        free(obj_ids);
        free(prop_counts);
        return -EFAULT;
    }

    uint64_t total_props = 0;
    for (uint32_t i = 0; i < ka.count_objs; i++) {
        if (prop_counts[i] > 32 || total_props > DRM_U32_MAX - prop_counts[i]) {
            free(obj_ids);
            free(prop_counts);
            return -E2BIG;
        }
        total_props += prop_counts[i];
    }

    if (total_props > 0) {
        prop_ids = (uint32_t *)calloc(total_props, sizeof(uint32_t));
        prop_vals = (uint64_t *)calloc(total_props, sizeof(uint64_t));
        if (!prop_ids || !prop_vals) {
            free(obj_ids);
            free(prop_counts);
            free(prop_ids);
            free(prop_vals);
            return -ENOMEM;
        }

        if (drm_copy_from_user_ptr(prop_ids, ka.props_ptr, total_props * sizeof(uint32_t)) != 0 ||
            drm_copy_from_user_ptr(prop_vals, ka.prop_values_ptr, total_props * sizeof(uint64_t)) != 0) {
            free(obj_ids);
            free(prop_counts);
            free(prop_ids);
            free(prop_vals);
            return -EFAULT;
        }
    }

    uint64_t pi = 0;
    for (uint32_t i = 0; i < ka.count_objs; i++) {
        uint32_t obj_id = obj_ids[i];
        uint32_t count = prop_counts[i];
        if (pi + count > total_props) {
            free(obj_ids);
            free(prop_counts);
            free(prop_ids);
            free(prop_vals);
            return -EINVAL;
        }

        for (uint32_t j = 0; j < count; j++, pi++) {
            uint32_t pid = prop_ids[pi];
            uint64_t val = prop_vals[pi];

            if (obj_id == drm->plane_id && pid == DRM_PROP_PLANE_FB_ID) {
                new_fb_id = (uint32_t)val;
            } else if (obj_id == drm->plane_id && pid == DRM_PROP_PLANE_SRC_X) {
                src_x = (uint32_t)(val >> 16);
            } else if (obj_id == drm->plane_id && pid == DRM_PROP_PLANE_SRC_Y) {
                src_y = (uint32_t)(val >> 16);
            } else if (obj_id == drm->plane_id && pid == DRM_PROP_PLANE_SRC_W) {
                src_w = (uint32_t)(val >> 16);
            } else if (obj_id == drm->plane_id && pid == DRM_PROP_PLANE_SRC_H) {
                src_h = (uint32_t)(val >> 16);
            } else if (obj_id == drm->plane_id && pid == DRM_PROP_PLANE_CRTC_X) {
                crtc_x = (int32_t)val;
            } else if (obj_id == drm->plane_id && pid == DRM_PROP_PLANE_CRTC_Y) {
                crtc_y = (int32_t)val;
            } else if (obj_id == drm->plane_id && pid == DRM_PROP_PLANE_CRTC_W) {
                crtc_w = (uint32_t)val;
            } else if (obj_id == drm->plane_id && pid == DRM_PROP_PLANE_CRTC_H) {
                crtc_h = (uint32_t)val;
            } else if (obj_id == drm->crtc_id && pid == DRM_PROP_CRTC_ACTIVE) {
                crtc_active = val != 0;
            } else if (obj_id == drm->crtc_id && pid == DRM_PROP_CRTC_MODE_ID) {
                new_mode_blob = val;
            }
        }
    }

    free(obj_ids);
    free(prop_counts);
    free(prop_ids);
    free(prop_vals);

    if (new_mode_blob != 0) {
        if (!drm_mode_blob_to_mode(drm, (uint32_t)new_mode_blob, &new_mode)) return -EINVAL;
        has_new_mode = true;
    }
    if (test_only) return 0;
    if (has_new_mode) drm->mode = new_mode;
    if (!crtc_active) {
        drm->current_fb_id = 0;
        return 0;
    }
    if (new_fb_id == 0) return 0;

    struct drm_fb_obj *fb = fb_find_by_id(drm, new_fb_id);
    if (!fb) return -ENOENT;

    if (crtc_x < 0 || crtc_y < 0) return -EINVAL;
    if (src_w != crtc_w || src_h != crtc_h) return -EINVAL;

    int ret = present_rect(drm, new_fb_id, src_x, src_y, (uint32_t)crtc_x, (uint32_t)crtc_y, crtc_w, crtc_h);
    if (ret) return ret;

    drm->current_fb_id = new_fb_id;

    if (page_flip_event)
        ev_queue(drm, ka.user_data);

    return 0;
}

static int drm_mode_getpropblob(struct xj380_drm *drm, struct drm_mode_get_blob *args)
{
    if (!args) return -EFAULT;
    struct drm_mode_get_blob kbl;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &kbl, args, sizeof(kbl)))
        return -EFAULT;

    uint32_t connector_id = 0;
    uint32_t crtc_id = 0;
    if (drm_mode_blob_to_crtc_id(kbl.blob_id, &crtc_id)) {
        if (crtc_id != drm->crtc_id) return -ENOENT;
        kbl.length = sizeof(drm->mode);
        if (kbl.data && drm_copy_to_user_ptr(kbl.data, &drm->mode, sizeof(drm->mode)) != 0) return -EFAULT;
        if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, args, &kbl, sizeof(kbl)))
            return -EFAULT;
        return 0;
    }

    if (drm_connector_edid_blob_to_connector_id(kbl.blob_id, &connector_id)) {
        if (connector_id != drm->connector_id) return -ENOENT;
        uint8_t edid[128];
        drm_build_edid(&drm->mode, edid, sizeof(edid));
        kbl.length = sizeof(edid);
        if (kbl.data && drm_copy_to_user_ptr(kbl.data, edid, sizeof(edid)) != 0) return -EFAULT;
        if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, args, &kbl, sizeof(kbl)))
            return -EFAULT;
        return 0;
    }

    uint32_t plane_id = 0;
    if (drm_plane_in_formats_blob_to_plane_id(kbl.blob_id, &plane_id)) {
        if (plane_id != drm->plane_id) return -ENOENT;

        struct {
            struct drm_format_modifier_blob blob;
            uint32_t formats[4];
        } in_formats;
        memset(&in_formats, 0, sizeof(in_formats));
        in_formats.blob.version = 1;
        in_formats.blob.count_formats = 4;
        in_formats.blob.formats_offset = sizeof(struct drm_format_modifier_blob);
        in_formats.blob.count_modifiers = 0;
        in_formats.blob.modifiers_offset = sizeof(in_formats);
        in_formats.formats[0] = DRM_FORMAT_XRGB8888;
        in_formats.formats[1] = DRM_FORMAT_ARGB8888;
        in_formats.formats[2] = DRM_FORMAT_XBGR8888;
        in_formats.formats[3] = DRM_FORMAT_ABGR8888;

        kbl.length = sizeof(in_formats);
        if (kbl.data && drm_copy_to_user_ptr(kbl.data, &in_formats, sizeof(in_formats)) != 0) return -EFAULT;
        if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, args, &kbl, sizeof(kbl)))
            return -EFAULT;
        return 0;
    }

    struct prop_blob *blob = blob_find_by_id(drm, kbl.blob_id);
    if (!blob) return -ENOENT;

    kbl.length = blob->length;
    if (kbl.data && blob->data && blob->length > 0) {
        if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir,
                (void *)(uintptr_t)kbl.data, blob->data, blob->length))
            return -EFAULT;
    }

    if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, args, &kbl, sizeof(kbl)))
        return -EFAULT;
    return 0;
}

/* ---- IOCTL dispatcher ---- */

static int drm_ioctl(device_t *dev, size_t req, void *arg)
{
    (void)dev;
    struct xj380_drm *drm = &g_drm;
    if (!drm_initialized()) return -ENODEV;

    /*
     * Linux ioctl commands are 32-bit values. The syscall path may sign-extend
     * commands with bit 31 set before they reach devfs, so normalize before the
     * switch just like naos does in its DRM ioctl dispatcher.
     */
    uint32_t cmd = (uint32_t)req;

    int ret = 0;
    switch (cmd) {
    case DRM_IOCTL_VERSION:          ret = drm_version(drm, (struct drm_version *)arg); break;
    case DRM_IOCTL_GET_UNIQUE:       ret = drm_get_unique(drm, (struct drm_unique *)arg); break;
    case DRM_IOCTL_GET_MAGIC:        ret = drm_get_magic((struct drm_auth *)arg); break;
    case DRM_IOCTL_AUTH_MAGIC:       ret = drm_auth_magic((struct drm_auth *)arg); break;
    case DRM_IOCTL_GET_CAP:          ret = drm_get_cap((struct drm_get_cap *)arg); break;
    case DRM_IOCTL_SET_VERSION:      ret = drm_set_version((struct drm_set_version *)arg); break;
    case DRM_IOCTL_GET_STATS:        ret = drm_get_stats((struct drm_stats *)arg); break;
    case DRM_IOCTL_WAIT_VBLANK:      ret = drm_wait_vblank((union drm_wait_vblank *)arg); break;
    case DRM_IOCTL_SET_CLIENT_CAP:   ret = drm_set_client_cap(drm, (struct drm_set_client_cap *)arg, NULL); break;
    case DRM_IOCTL_PRIME_HANDLE_TO_FD: ret = drm_prime_handle_to_fd(drm, (struct drm_prime_handle *)arg); break;
    case DRM_IOCTL_PRIME_FD_TO_HANDLE: ret = drm_prime_fd_to_handle(drm, (struct drm_prime_handle *)arg); break;

    case DRM_IOCTL_MODE_GETRESOURCES: ret = drm_mode_getresources(drm, (struct drm_mode_card_res *)arg); break;
    case DRM_IOCTL_MODE_GETCRTC:      ret = drm_mode_getcrtc(drm, (struct drm_mode_crtc *)arg); break;
    case DRM_IOCTL_MODE_SETCRTC:      ret = drm_mode_setcrtc(drm, (struct drm_mode_crtc *)arg); break;
    case DRM_IOCTL_MODE_PAGE_FLIP:    ret = drm_mode_page_flip(drm, (struct drm_mode_crtc_page_flip *)arg); break;
    case DRM_IOCTL_MODE_GETCONNECTOR: ret = drm_mode_getconnector(drm, (struct drm_mode_get_connector *)arg); break;
    case DRM_IOCTL_MODE_GETENCODER:   ret = drm_mode_getencoder(drm, (struct drm_mode_get_encoder *)arg); break;
    case DRM_IOCTL_MODE_CREATE_DUMB:  ret = drm_mode_create_dumb(drm, (struct drm_mode_create_dumb *)arg); break;
    case DRM_IOCTL_MODE_MAP_DUMB:     ret = drm_mode_map_dumb(drm, (struct drm_mode_map_dumb *)arg); break;
    case DRM_IOCTL_MODE_DESTROY_DUMB: ret = drm_mode_destroy_dumb(drm, (struct drm_mode_destroy_dumb *)arg); break;
    case DRM_IOCTL_MODE_ADDFB:        ret = drm_mode_addfb(drm, (struct drm_mode_fb_cmd *)arg); break;
    case DRM_IOCTL_MODE_ADDFB2:       ret = drm_mode_addfb2(drm, (struct drm_mode_fb_cmd2 *)arg); break;
    case DRM_IOCTL_MODE_GETFB:        ret = drm_mode_getfb(drm, (struct drm_mode_fb_cmd *)arg); break;
    case DRM_IOCTL_MODE_RMFB:         ret = drm_mode_rmfb(drm, (uint32_t *)arg); break;
    case DRM_IOCTL_MODE_CLOSEFB:      ret = drm_mode_closefb(drm, (struct drm_mode_closefb *)arg); break;
    case DRM_IOCTL_MODE_GETPLANERESOURCES: ret = drm_mode_getplane_resources(drm, (struct drm_mode_get_plane_res *)arg); break;
    case DRM_IOCTL_MODE_GETPLANE:      ret = drm_mode_getplane(drm, (struct drm_mode_get_plane *)arg); break;
    case DRM_IOCTL_MODE_SETPLANE:      ret = drm_mode_setplane(drm, (struct drm_mode_set_plane *)arg); break;
    case DRM_IOCTL_MODE_DIRTYFB:       ret = drm_mode_dirtyfb(drm, (struct drm_mode_fb_dirty_cmd *)arg); break;
    case DRM_IOCTL_MODE_GETPROPERTY:   ret = drm_mode_getproperty(drm, (struct drm_mode_get_property *)arg); break;
    case DRM_IOCTL_MODE_CREATEPROPBLOB: ret = drm_mode_createpropblob(drm, (struct drm_mode_create_blob *)arg); break;
    case DRM_IOCTL_MODE_DESTROYPROPBLOB: ret = drm_mode_destroypropblob(drm, (struct drm_mode_destroy_blob *)arg); break;
    case DRM_IOCTL_MODE_OBJ_GETPROPERTIES: ret = drm_mode_obj_getproperties(drm, (struct drm_mode_obj_get_properties *)arg); break;
    case DRM_IOCTL_MODE_ATOMIC:        ret = drm_mode_atomic(drm, (struct drm_mode_atomic *)arg); break;
    case DRM_IOCTL_MODE_GETPROPBLOB:   ret = drm_mode_getpropblob(drm, (struct drm_mode_get_blob *)arg); break;
    case DRM_IOCTL_MODE_CREATE_LEASE:  ret = drm_mode_create_lease((struct drm_mode_create_lease *)arg); break;
    case DRM_IOCTL_MODE_LIST_LESSEES:  ret = drm_mode_list_lessees((struct drm_mode_list_lessees *)arg); break;
    case DRM_IOCTL_MODE_GET_LEASE:     ret = drm_mode_get_lease((struct drm_mode_get_lease *)arg); break;
    case DRM_IOCTL_MODE_REVOKE_LEASE:  ret = drm_mode_revoke_lease((struct drm_mode_revoke_lease *)arg); break;

    /* Legacy / no-ops that userspace might call */
    case DRM_IOCTL_SET_MASTER:
    case DRM_IOCTL_DROP_MASTER:
        ret = 0;
        break;

    /* Stubs for unused ioctls */
    case DRM_IOCTL_GEM_CLOSE:
        ret = drm_gem_close(drm, (struct drm_gem_close *)arg);
        break;

    default:
        write_serial_fmt("[udev-trace] drm:unsupported ioctl req=0x%llx cmd=0x%x\n",
                         (unsigned long long)req, cmd);
        return -ENOSYS;
    }

    if (ret < 0)
        write_serial_fmt("[drm-trace] ioctl cmd=0x%x ret=%d\n", cmd, ret);
    return ret;
}

/* ---- read (event dequeuer) ---- */
static size_t drm_read(int drive, uint8_t *buf, size_t len, size_t offset)
{
    (void)drive; (void)offset;
    struct xj380_drm *drm = &g_drm;
    if (!drm_initialized()) return (size_t)-ENODEV;
    if (len < sizeof(struct drm_event_vblank)) return (size_t)-EINVAL;

    spin_lock(&drm->lock);
    if (drm->event_count == 0) {
        spin_unlock(&drm->lock);
        return (size_t)-EAGAIN;
    }

    struct event_entry *ev = &drm->events[drm->event_head];
    drm->event_head = (drm->event_head + 1) % EVENT_RING_SIZE;
    drm->event_count--;

    memcpy(buf, &ev->vbl, sizeof(ev->vbl));
    spin_unlock(&drm->lock);
    return sizeof(ev->vbl);
}

/* ---- mmap (dumb buffer map to user) ---- */
static void *drm_map(int drive, void *addr, size_t len, size_t offset)
{
    (void)drive;
    struct xj380_drm *drm = &g_drm;
    if (!drm_initialized()) return NULL;
    if (offset == 0 || len == 0 || addr == NULL) return NULL;

    /* Look up the dumb buffer by physical address (offset) */
    for (int i = 0; i < MAX_DUMB_BUFFERS; i++) {
        if (drm->dumb[i].used && drm->dumb[i].phys_addr == offset) {
            size_t buf_bytes = (size_t)drm->dumb[i].pitch * drm->dumb[i].height;
            size_t map_len = PADDING_UP(MIN(len, buf_bytes), PAGE_SIZE);
            for (size_t pos = 0; pos < map_len; pos += PAGE_SIZE)
            {
                page_map_ref_to(get_current_directory(), (uint64_t)addr + pos, offset + pos,
                                PTE_USER | PTE_PRESENT | PTE_WRITEABLE | PTE_NO_EXECUTE);
            }
            return addr;
        }
    }
    return NULL;
}

/* ---- Init ---- */

void drm_init()
{
    write_serial_string("[udev-trace] drm:init begin\n");
    if (!fbdev_ready()) {
        write_serial_string("[udev-trace] drm:init fbdev-not-ready skip\n");
        write_serial_string("drm: fbdev not ready, skipping\n");
        return;
    }

    memset(&g_drm, 0, sizeof(g_drm));
    spin_init(&g_drm.lock);

    strncpy(g_drm.drv_name, "xj380", sizeof(g_drm.drv_name) - 1);
    strncpy(g_drm.drv_date, "20260525", sizeof(g_drm.drv_date) - 1);
    strncpy(g_drm.drv_desc, "XJ380 DRM driver", sizeof(g_drm.drv_desc) - 1);

    /* Get framebuffer dimensions from fbdev */
    g_drm.fb_width  = 1024;//先这样吧
    g_drm.fb_height = 768;

    const FrameBufferConfig *fbc = fbdev_get_fbc();
    if (fbc) {
        g_drm.fb_width  = fbc->horizontal_resolution;
        g_drm.fb_height = fbc->vertical_resolution;
    }
    write_serial_fmt("[udev-trace] drm:init fb=%ux%u fbc=%p\n", g_drm.fb_width, g_drm.fb_height, fbc);
    drm_init_mode(&g_drm.mode, g_drm.fb_width, g_drm.fb_height, 60);
    
    /* 调试输出 */
    write_serial_fmt("[drm] mode: %dx%d, clock=%d kHz, htotal=%d, vtotal=%d, vrefresh=%d\n",
                     g_drm.mode.hdisplay, g_drm.mode.vdisplay,
                     g_drm.mode.clock, g_drm.mode.htotal, 
                     g_drm.mode.vtotal, g_drm.mode.vrefresh);
    
    /* Object IDs */
    g_drm.crtc_id      = 1;
    g_drm.connector_id = 2;
    g_drm.encoder_id   = 3;
    g_drm.plane_id     = 4;
    g_drm.next_fb_id   = 10;

    /* Register /dev/dri/card0 */
    device_t dev = {};
    dev.read   = drm_read;
    dev.write  = NULL;
    dev.ioctl  = drm_ioctl;
    dev.map    = drm_map;
    dev.poll   = drm_poll;
    dev.flag   = 1;
    dev.type   = DEVICE_STREAM;
    dev.size   = 0;
    dev.sector_size = 1;
    dev.max_size = 0;
    dev.major = 226;
    dev.minor = 0;
    strcpy(dev.drive_name, "card0");

    g_inited = true;

    errno_t mkdir_ret = vfs_mkdir("/dev/dri");
    write_serial_fmt("[udev-trace] drm:init mkdir /dev/dri ret=%d\n", mkdir_ret);
    int dev_id = regist_device("dri/card0", dev);
    write_serial_fmt("[udev-trace] drm:init regist_device dri/card0 id=%d major=%u minor=%u\n",
                     dev_id,
                     dev.major,
                     dev.minor);

    write_serial_fmt("drm: XJ380 DRM initialized, fb=%ux%u\n", g_drm.fb_width, g_drm.fb_height);
}

extern "C" void drm_init_c() { drm_init(); }
