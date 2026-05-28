#include <device.h>
#include <drm/drm.h>
#include <drm/drm_mode.h>
#include <errno.h>
#include <fbdev.h>
#include <ioctl.h>
#include <krlibc.h>
#include <lock_queue.h>
#include <mm/heap.h>
#include <mm/page.h>
#include <mm/uaccess.h>
#include <proto.hpp>
#include <task/poll.h>
#include <task/pcb.h>

/* ---- Driver constants ---- */
#define MAX_DUMB_BUFFERS     32
#define MAX_FB_OBJECTS       32
#define MAX_PROP_BLOBS       16
#define EVENT_RING_SIZE      16
#define DEFAULT_MODE_NAME    "1024x768"
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
    uint32_t format;
};

/* ---- Property blob ---- */
struct prop_blob {
    bool     used;
    uint32_t blob_id;
    void    *data;
    uint32_t length;
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
    uint32_t            next_fb_id;

    /* version */
    char drv_name[32], drv_date[32], drv_desc[64];

    /* framebuffer info */
    uint32_t            fb_width, fb_height;

    /* event ring (per-device, single reader for now) */
    struct event_entry  events[EVENT_RING_SIZE];
    uint32_t            event_head, event_count;
    bool                event_awaiting;  /* reader is sleeping */
};

static struct xj380_drm g_drm;
static bool g_inited = false;

/* ---- Helpers ---- */

static inline bool drm_initialized() { return g_inited; }

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

static void ev_queue(struct xj380_drm *drm, uint64_t user_data)
{
    if (drm->event_count >= EVENT_RING_SIZE) return;
    uint32_t idx = (drm->event_head + drm->event_count) % EVENT_RING_SIZE;
    struct event_entry *ev = &drm->events[idx];
    memset(ev, 0, sizeof(*ev));
    ev->vbl.base.type   = DRM_EVENT_FLIP_COMPLETE;
    ev->vbl.base.length = sizeof(ev->vbl);
    ev->vbl.user_data   = user_data;
    ev->vbl.crtc_id     = drm->crtc_id;
    drm->event_count++;
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
static int present(struct xj380_drm *drm, uint32_t fb_id)
{
    struct drm_fb_obj *fb = fb_find_by_id(drm, fb_id);
    if (!fb) return -ENOENT;

    int idx = dumb_find_by_handle(drm, fb->handle);
    if (idx < 0) return -EINVAL;

    struct dumb_buffer *db = &drm->dumb[idx];
    uint8_t *src = (uint8_t *)driver_phys_to_virt(db->phys_addr);
    uint32_t w = MIN(drm->fb_width, fb->width);
    uint32_t h = MIN(drm->fb_height, fb->height);
    uint32_t bpp = 4;

    uint8_t *dst = fbdev_kernel_framebuffer();
    if (!dst) return -ENODEV;

    size_t row_bytes = (size_t)w * bpp;
    for (uint32_t y = 0; y < h; y++) {
        memcpy(dst + (size_t)y * (size_t)drm->fb_width * bpp,
               src + (size_t)y * (size_t)db->pitch,
               row_bytes);
    }

    /* Tell fbdev to actually flush to GOP */
    fbdev_present_kernel_region(0, 0, w, h);
    return 0;
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
    case DRM_CAP_PRIME:             kc.value = 1; break;
    case DRM_CAP_ASYNC_PAGE_FLIP:   kc.value = 1; break;
    case DRM_CAP_CURSOR_WIDTH:      kc.value = 64; break;
    case DRM_CAP_CURSOR_HEIGHT:     kc.value = 64; break;
    default:                        kc.value = 0; break;
    }

    if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, cap, &kc, sizeof(kc)))
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

    if (--drm->dumb[idx].refs == 0) {
        free_frames(drm->dumb[idx].phys_addr,
                    PADDING_UP((size_t)drm->dumb[idx].pitch * drm->dumb[idx].height, PAGE_SIZE) / PAGE_SIZE);
        memset(&drm->dumb[idx], 0, sizeof(drm->dumb[idx]));
    }
    return 0;
}

static int drm_mode_getresources(struct xj380_drm *drm, struct drm_mode_card_res *res)
{
    if (!res) return -EFAULT;
    struct drm_mode_card_res kr;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &kr, res, sizeof(kr)))
        return -EFAULT;

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

    if (kr.crtc_id_ptr && !copy_to_user_pagedir(get_current_task()->parent_group->pagedir,
            (void *)(uintptr_t)kr.crtc_id_ptr, &crtc_id, sizeof(crtc_id)))
        return -EFAULT;
    if (kr.connector_id_ptr && !copy_to_user_pagedir(get_current_task()->parent_group->pagedir,
            (void *)(uintptr_t)kr.connector_id_ptr, &conn_id, sizeof(conn_id)))
        return -EFAULT;
    if (kr.encoder_id_ptr && !copy_to_user_pagedir(get_current_task()->parent_group->pagedir,
            (void *)(uintptr_t)kr.encoder_id_ptr, &enc_id, sizeof(enc_id)))
        return -EFAULT;

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
        if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir,
                (void *)(uintptr_t)kc.modes_ptr, &m, sizeof(m)))
            return -EFAULT;
    }

    /* Write encoder id */
    if (kc.encoders_ptr) {
        uint32_t eid = drm->encoder_id;
        if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir,
                (void *)(uintptr_t)kc.encoders_ptr, &eid, sizeof(eid)))
            return -EFAULT;
    }

    /* Property IDs + values: DPMS, CRTC_ID */
    if (kc.props_ptr) {
        uint32_t pids[3] = { DRM_PROP_CONNECTOR_DPMS, DRM_PROP_CONNECTOR_EDID, DRM_PROP_CONNECTOR_CRTC_ID };
        if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir,
                (void *)(uintptr_t)kc.props_ptr, pids, sizeof(pids)))
            return -EFAULT;
    }
    if (kc.prop_values_ptr) {
        uint64_t pvals[3] = { DRM_MODE_DPMS_ON, drm_connector_edid_blob_id(drm->connector_id), drm->crtc_id };
        if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir,
                (void *)(uintptr_t)kc.prop_values_ptr, pvals, sizeof(pvals)))
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

    if (--drm->dumb[idx].refs == 0) {
        free_frames(drm->dumb[idx].phys_addr,
                    PADDING_UP((size_t)drm->dumb[idx].pitch * drm->dumb[idx].height, PAGE_SIZE) / PAGE_SIZE);
        memset(&drm->dumb[idx], 0, sizeof(drm->dumb[idx]));
    }
    return 0;
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

    uint32_t fb_id = drm->next_fb_id++;
    drm->fbs[fi].used   = true;
    drm->fbs[fi].fb_id  = fb_id;
    drm->fbs[fi].handle = ka.handle;
    drm->fbs[fi].width  = ka.width;
    drm->fbs[fi].height = ka.height;
    drm->fbs[fi].pitch  = ka.pitch;
    drm->fbs[fi].format = DRM_FORMAT_XRGB8888;

    ka.fb_id = fb_id;
    if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, args, &ka, sizeof(ka)))
        return -EFAULT;
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

    uint32_t fb_id = drm->next_fb_id++;
    drm->fbs[fi].used   = true;
    drm->fbs[fi].fb_id  = fb_id;
    drm->fbs[fi].handle = ka.handles[0];
    drm->fbs[fi].width  = ka.width;
    drm->fbs[fi].height = ka.height;
    drm->fbs[fi].pitch  = ka.pitches[0] ? ka.pitches[0] : drm->dumb[idx].pitch;
    drm->fbs[fi].format = ka.pixel_format;

    ka.fb_id = fb_id;
    if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, args, &ka, sizeof(ka)))
        return -EFAULT;
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
    memset(fb, 0, sizeof(*fb));
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
    memset(fb, 0, sizeof(*fb));
    return 0;
}

static int drm_mode_getplane_resources(struct xj380_drm *drm, struct drm_mode_get_plane_res *args)
{
    if (!args) return -EFAULT;
    struct drm_mode_get_plane_res ka;
    if (!copy_from_user_pagedir(get_current_task()->parent_group->pagedir, &ka, args, sizeof(ka)))
        return -EFAULT;

    ka.count_planes = 1;
    uint32_t pid = drm->plane_id;
    if (ka.plane_id_ptr && !copy_to_user_pagedir(get_current_task()->parent_group->pagedir,
            (void *)(uintptr_t)ka.plane_id_ptr, &pid, sizeof(pid)))
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

    ka.crtc_id       = drm->crtc_id;
    ka.fb_id         = drm->current_fb_id;
    ka.possible_crtcs = 1;
    ka.gamma_size    = 0;
    ka.count_format_types = 4;
    uint32_t formats[4] = { DRM_FORMAT_XRGB8888, DRM_FORMAT_ARGB8888,
                            DRM_FORMAT_XBGR8888, DRM_FORMAT_ABGR8888 };
    if (ka.format_type_ptr && !copy_to_user_pagedir(get_current_task()->parent_group->pagedir,
            (void *)(uintptr_t)ka.format_type_ptr, formats, sizeof(formats)))
        return -EFAULT;

    if (!copy_to_user_pagedir(get_current_task()->parent_group->pagedir, args, &ka, sizeof(ka)))
        return -EFAULT;
    return 0;
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
    bool crtc_active = true;

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

    if (new_mode_blob != 0 && new_mode_blob != drm->crtc_id) return -EINVAL;
    if (test_only) return 0;
    if (!crtc_active) {
        drm->current_fb_id = 0;
        return 0;
    }
    if (new_fb_id == 0) return 0;

    struct drm_fb_obj *fb = fb_find_by_id(drm, new_fb_id);
    if (!fb) return -ENOENT;

    int ret = present(drm, new_fb_id);
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
        kbl.length = 0;
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

    /* arg is the ioctl number, the caller has already mapped it */
    /* opaque void* is actually the raw argument from userspace */

    switch (req) {
    case DRM_IOCTL_VERSION:          return drm_version(drm, (struct drm_version *)arg);
    case DRM_IOCTL_GET_CAP:          return drm_get_cap((struct drm_get_cap *)arg);
    case DRM_IOCTL_SET_CLIENT_CAP:   return 0;

    case DRM_IOCTL_MODE_GETRESOURCES: return drm_mode_getresources(drm, (struct drm_mode_card_res *)arg);
    case DRM_IOCTL_MODE_GETCRTC:      return drm_mode_getcrtc(drm, (struct drm_mode_crtc *)arg);
    case DRM_IOCTL_MODE_SETCRTC:      return drm_mode_setcrtc(drm, (struct drm_mode_crtc *)arg);
    case DRM_IOCTL_MODE_PAGE_FLIP:    return drm_mode_page_flip(drm, (struct drm_mode_crtc_page_flip *)arg);
    case DRM_IOCTL_MODE_GETCONNECTOR: return drm_mode_getconnector(drm, (struct drm_mode_get_connector *)arg);
    case DRM_IOCTL_MODE_GETENCODER:   return drm_mode_getencoder(drm, (struct drm_mode_get_encoder *)arg);
    case DRM_IOCTL_MODE_CREATE_DUMB:  return drm_mode_create_dumb(drm, (struct drm_mode_create_dumb *)arg);
    case DRM_IOCTL_MODE_MAP_DUMB:     return drm_mode_map_dumb(drm, (struct drm_mode_map_dumb *)arg);
    case DRM_IOCTL_MODE_DESTROY_DUMB: return drm_mode_destroy_dumb(drm, (struct drm_mode_destroy_dumb *)arg);
    case DRM_IOCTL_MODE_ADDFB:        return drm_mode_addfb(drm, (struct drm_mode_fb_cmd *)arg);
    case DRM_IOCTL_MODE_ADDFB2:       return drm_mode_addfb2(drm, (struct drm_mode_fb_cmd2 *)arg);
    case DRM_IOCTL_MODE_GETFB:        return drm_mode_getfb(drm, (struct drm_mode_fb_cmd *)arg);
    case DRM_IOCTL_MODE_RMFB:         return drm_mode_rmfb(drm, (uint32_t *)arg);
    case DRM_IOCTL_MODE_CLOSEFB:      return drm_mode_closefb(drm, (struct drm_mode_closefb *)arg);
    case DRM_IOCTL_MODE_GETPLANERESOURCES: return drm_mode_getplane_resources(drm, (struct drm_mode_get_plane_res *)arg);
    case DRM_IOCTL_MODE_GETPLANE:      return drm_mode_getplane(drm, (struct drm_mode_get_plane *)arg);
    case DRM_IOCTL_MODE_GETPROPERTY:   return drm_mode_getproperty(drm, (struct drm_mode_get_property *)arg);
    case DRM_IOCTL_MODE_CREATEPROPBLOB:return drm_mode_createpropblob(drm, (struct drm_mode_create_blob *)arg);
    case DRM_IOCTL_MODE_DESTROYPROPBLOB: return drm_mode_destroypropblob(drm, (struct drm_mode_destroy_blob *)arg);
    case DRM_IOCTL_MODE_OBJ_GETPROPERTIES: return drm_mode_obj_getproperties(drm, (struct drm_mode_obj_get_properties *)arg);
    case DRM_IOCTL_MODE_ATOMIC:        return drm_mode_atomic(drm, (struct drm_mode_atomic *)arg);
    case DRM_IOCTL_MODE_GETPROPBLOB:   return drm_mode_getpropblob(drm, (struct drm_mode_get_blob *)arg);

    /* Legacy / no-ops that userspace might call */
    case DRM_IOCTL_SET_MASTER:
    case DRM_IOCTL_DROP_MASTER:
    case DRM_IOCTL_GET_MAGIC:
        return 0;

    /* Stubs for unused ioctls */
    case DRM_IOCTL_GEM_CLOSE:
        return drm_gem_close(drm, (struct drm_gem_close *)arg);

    default:
        return -ENOSYS;
    }
}

/* ---- read (event dequeuer) ---- */
static size_t drm_read(int drive, uint8_t *buf, size_t len, size_t offset)
{
    (void)drive; (void)offset;
    struct xj380_drm *drm = &g_drm;
    if (!drm_initialized()) return (size_t)-ENODEV;
    if (len < sizeof(struct drm_event_vblank)) return (size_t)-EINVAL;

    spin_lock(&drm->lock);
    while (drm->event_count == 0) {
        spin_unlock(&drm->lock);
        /* busy-wait / yield — full sleep would need waitqueue */
        scheduler_yield();
        spin_lock(&drm->lock);
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
            page_map_range(get_current_directory(), (uint64_t)addr, offset, map_len,
                           PTE_USER | PTE_PRESENT | PTE_WRITEABLE | PTE_NO_EXECUTE);
            return addr;
        }
    }
    return NULL;
}

/* ---- Init ---- */

void drm_init()
{
    if (!fbdev_ready()) {
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

    /* Object IDs */
    g_drm.crtc_id      = 1;
    g_drm.connector_id = 2;
    g_drm.encoder_id   = 3;
    g_drm.plane_id     = 4;
    g_drm.next_fb_id   = 10;

    /* Default mode */
    memset(&g_drm.mode, 0, sizeof(g_drm.mode));
    g_drm.mode.hdisplay = g_drm.fb_width;
    g_drm.mode.vdisplay = g_drm.fb_height;
    g_drm.mode.vrefresh = 60;
    g_drm.mode.type     = DRM_MODE_TYPE_PREFERRED;
    strncpy(g_drm.mode.name, DEFAULT_MODE_NAME, DRM_DISPLAY_MODE_LEN - 1);

    /* Register /dev/dri/card0 */
    device_t dev;
    memset(&dev, 0, sizeof(dev));
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
    strcpy(dev.drive_name, "card0");

    regist_device("dri", dev);
    g_inited = true;

    write_serial_fmt("drm: XJ380 DRM initialized, fb=%ux%u\n", g_drm.fb_width, g_drm.fb_height);
}

extern "C" void drm_init_c() { drm_init(); }
