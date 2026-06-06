#pragma once
#include <drm/drm_fourcc.h>
#include <ioctl.h>
#include <stdint.h>

/* DRM ioctl base: 'd' = 0x64 */
#define DRM_IOCTL_BASE 'd'
#define DRM_IO(nr)     _IO(DRM_IOCTL_BASE, nr)
#define DRM_IOR(nr, t) _IOR(DRM_IOCTL_BASE, nr, t)
#define DRM_IOW(nr, t) _IOW(DRM_IOCTL_BASE, nr, t)
#define DRM_IOWR(nr, t) _IOWR(DRM_IOCTL_BASE, nr, t)

/* Core ioctls */
#define DRM_IOCTL_VERSION         DRM_IOWR(0x00, struct drm_version)
#define DRM_IOCTL_GET_UNIQUE      DRM_IOWR(0x01, struct drm_unique)
#define DRM_IOCTL_GET_MAGIC       DRM_IOR(0x02, struct drm_auth)
#define DRM_IOCTL_IRQ_BUSID       DRM_IOWR(0x03, struct drm_irq_busid)
#define DRM_IOCTL_GET_CAP         DRM_IOWR(0x0c, struct drm_get_cap)
#define DRM_IOCTL_SET_CLIENT_CAP  DRM_IOW(0x0d, struct drm_set_client_cap)
#define DRM_IOCTL_AUTH_MAGIC      DRM_IOW(0x11, struct drm_auth)
#define DRM_IOCTL_SET_MASTER      DRM_IO(0x1e)
#define DRM_IOCTL_DROP_MASTER     DRM_IO(0x1f)
#define DRM_IOCTL_PRIME_HANDLE_TO_FD DRM_IOWR(0x2d, struct drm_prime_handle)
#define DRM_IOCTL_PRIME_FD_TO_HANDLE DRM_IOWR(0x2e, struct drm_prime_handle)
#define DRM_IOCTL_GEM_CLOSE       DRM_IOW(0x09, struct drm_gem_close)
#define DRM_IOCTL_GEM_FLINK       DRM_IOWR(0x0a, struct drm_gem_flink)
#define DRM_IOCTL_GEM_OPEN        DRM_IOWR(0x0b, struct drm_gem_open)
#define DRM_IOCTL_SET_VERSION     DRM_IOWR(0x07, struct drm_set_version)
#define DRM_IOCTL_GET_STATS       DRM_IOR(0x06, struct drm_stats)
#define DRM_IOCTL_WAIT_VBLANK     DRM_IOWR(0x3a, union drm_wait_vblank)

/* Modesetting ioctls */
#define DRM_IOCTL_MODE_GETRESOURCES    DRM_IOWR(0xA0, struct drm_mode_card_res)
#define DRM_IOCTL_MODE_GETCRTC         DRM_IOWR(0xA1, struct drm_mode_crtc)
#define DRM_IOCTL_MODE_SETCRTC         DRM_IOWR(0xA2, struct drm_mode_crtc)
#define DRM_IOCTL_MODE_GETENCODER      DRM_IOWR(0xA6, struct drm_mode_get_encoder)
#define DRM_IOCTL_MODE_GETCONNECTOR    DRM_IOWR(0xA7, struct drm_mode_get_connector)
#define DRM_IOCTL_MODE_GETPROPERTY     DRM_IOWR(0xAA, struct drm_mode_get_property)
#define DRM_IOCTL_MODE_GETPROPBLOB     DRM_IOWR(0xAC, struct drm_mode_get_blob)
#define DRM_IOCTL_MODE_GETFB           DRM_IOWR(0xAD, struct drm_mode_fb_cmd)
#define DRM_IOCTL_MODE_ADDFB           DRM_IOWR(0xAE, struct drm_mode_fb_cmd)
#define DRM_IOCTL_MODE_RMFB            DRM_IOWR(0xAF, uint32_t)
#define DRM_IOCTL_MODE_PAGE_FLIP       DRM_IOWR(0xB0, struct drm_mode_crtc_page_flip)
#define DRM_IOCTL_MODE_DIRTYFB         DRM_IOWR(0xB1, struct drm_mode_fb_dirty_cmd)
#define DRM_IOCTL_MODE_CREATE_DUMB     DRM_IOWR(0xB2, struct drm_mode_create_dumb)
#define DRM_IOCTL_MODE_MAP_DUMB        DRM_IOWR(0xB3, struct drm_mode_map_dumb)
#define DRM_IOCTL_MODE_DESTROY_DUMB    DRM_IOWR(0xB4, struct drm_mode_destroy_dumb)
#define DRM_IOCTL_MODE_ADDFB2          DRM_IOWR(0xB8, struct drm_mode_fb_cmd2)
#define DRM_IOCTL_MODE_CLOSEFB         DRM_IOWR(0xD0, struct drm_mode_closefb)

/* Atomic / planes */
#define DRM_IOCTL_MODE_GETPLANERESOURCES DRM_IOWR(0xB5, struct drm_mode_get_plane_res)
#define DRM_IOCTL_MODE_GETPLANE          DRM_IOWR(0xB6, struct drm_mode_get_plane)
#define DRM_IOCTL_MODE_SETPLANE          DRM_IOWR(0xB7, struct drm_mode_set_plane)
#define DRM_IOCTL_MODE_OBJ_GETPROPERTIES DRM_IOWR(0xB9, struct drm_mode_obj_get_properties)
#define DRM_IOCTL_MODE_ATOMIC            DRM_IOWR(0xBC, struct drm_mode_atomic)
#define DRM_IOCTL_MODE_CREATEPROPBLOB    DRM_IOWR(0xBD, struct drm_mode_create_blob)
#define DRM_IOCTL_MODE_DESTROYPROPBLOB   DRM_IOWR(0xBE, struct drm_mode_destroy_blob)

#define DRM_IOCTL_MODE_CREATE_LEASE      DRM_IOWR(0xC6, struct drm_mode_create_lease)
#define DRM_IOCTL_MODE_LIST_LESSEES      DRM_IOWR(0xC7, struct drm_mode_list_lessees)
#define DRM_IOCTL_MODE_GET_LEASE         DRM_IOWR(0xC8, struct drm_mode_get_lease)
#define DRM_IOCTL_MODE_REVOKE_LEASE      DRM_IOWR(0xC9, struct drm_mode_revoke_lease)

#define DRM_IOCTL_MODE_GETFB2            DRM_IOWR(0xCE, struct drm_mode_fb_cmd2)

/* Capabilities */
#define DRM_CAP_DUMB_BUFFER        0x1
#define DRM_CAP_VBLANK_HIGH_CRTC   0x2
#define DRM_CAP_DUMB_PREFERRED_DEPTH 0x3
#define DRM_CAP_DUMB_PREFER_SHADOW 0x4
#define DRM_CAP_PRIME              0x5
#define DRM_PRIME_CAP_IMPORT       0x1
#define DRM_PRIME_CAP_EXPORT       0x2
#define DRM_CAP_TIMESTAMP_MONOTONIC 0x6
#define DRM_CAP_ASYNC_PAGE_FLIP    0x7
#define DRM_CAP_CURSOR_WIDTH       0x8
#define DRM_CAP_CURSOR_HEIGHT      0x9
#define DRM_CAP_ADDFB2_MODIFIERS   0x10
#define DRM_CAP_PAGE_FLIP_TARGET   0x11
#define DRM_CAP_CRTC_IN_VBLANK_EVENT 0x12
#define DRM_CAP_ATOMIC_ASYNC_PAGE_FLIP 0x15

/* Client capabilities */
#define DRM_CLIENT_CAP_STEREO_3D    1
#define DRM_CLIENT_CAP_UNIVERSAL_PLANES 2
#define DRM_CLIENT_CAP_ATOMIC       3
#define DRM_CLIENT_CAP_ASPECT_RATIO 4
#define DRM_CLIENT_CAP_WRITEBACK_CONNECTORS 5

#define DRM_RDWR    02
#define DRM_CLOEXEC 02000000

/* ----------------------------------------------------------------------
 * DRM structures
 * ---------------------------------------------------------------------- */

struct drm_version {
    int    version_major;
    int    version_minor;
    int    version_patchlevel;
    size_t name_len;
    void  *name;
    size_t date_len;
    void  *date;
    size_t desc_len;
    void  *desc;
};

struct drm_unique {
    size_t unique_len;
    void  *unique;
};

struct drm_auth {
    uint32_t magic;
};

struct drm_irq_busid {
    int      irq;
    int      busnum;
    int      devnum;
    int      funcnum;
};

struct drm_get_cap {
    uint64_t capability;
    uint64_t value;
};

struct drm_set_client_cap {
    uint64_t capability;
    uint64_t value;
};

struct drm_gem_close {
    uint32_t handle;
    uint32_t pad;
};

struct drm_gem_flink {
    uint32_t handle;
    uint32_t name;
};

struct drm_gem_open {
    uint32_t name;
    uint32_t handle;
    uint64_t size;
};

struct drm_prime_handle {
    uint32_t handle;
    uint32_t flags;
    int32_t  fd;
};

struct drm_set_version {
    int drm_di_major;
    int drm_di_minor;
    int drm_dd_major;
    int drm_dd_minor;
};

struct drm_stats {
    uint32_t count;
    struct {
        uint64_t value;
        uint32_t type;
    } data[15];
};

struct drm_wait_vblank_request {
    uint32_t type;
    uint32_t sequence;
    uint64_t signal;
};

struct drm_wait_vblank_reply {
    uint32_t type;
    uint32_t sequence;
    int64_t tval_sec;
    int64_t tval_usec;
};

union drm_wait_vblank {
    struct drm_wait_vblank_request request;
    struct drm_wait_vblank_reply reply;
};

struct drm_clip_rect {
    uint16_t x1, y1, x2, y2;
};

/* ----------------------------------------------------------------------
 * Event structures
 * ---------------------------------------------------------------------- */

#define DRM_EVENT_VBLANK         0x01
#define DRM_EVENT_FLIP_COMPLETE  0x02

struct drm_event {
    uint32_t type;
    uint32_t length;
};

struct drm_event_vblank {
    struct drm_event base;
    uint64_t user_data;
    uint32_t tv_sec;
    uint32_t tv_usec;
    uint32_t sequence;
    uint32_t crtc_id;
};
