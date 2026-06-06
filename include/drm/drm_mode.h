#pragma once
#include <stdint.h>

#define DRM_DISPLAY_MODE_LEN 32
#define DRM_PROP_NAME_LEN    32

/* Mode type flags */
#define DRM_MODE_TYPE_PREFERRED (1 << 3)
#define DRM_MODE_TYPE_USERDEF   (1 << 5)
#define DRM_MODE_TYPE_DRIVER    (1 << 6)

/* DPMS */
#define DRM_MODE_DPMS_ON      0
#define DRM_MODE_DPMS_STANDBY 1
#define DRM_MODE_DPMS_SUSPEND 2
#define DRM_MODE_DPMS_OFF     3

/* Connector types */
#define DRM_MODE_CONNECTOR_VIRTUAL 15

/* Connection states */
#define DRM_MODE_CONNECTED         1
#define DRM_MODE_DISCONNECTED      2
#define DRM_MODE_UNKNOWNCONNECTION 3

/* Subpixel */
#define DRM_MODE_SUBPIXEL_UNKNOWN       1
#define DRM_MODE_SUBPIXEL_HORIZONTAL_RGB 2

/* Encoder types */
#define DRM_MODE_ENCODER_NONE    0
#define DRM_MODE_ENCODER_VIRTUAL 5

/* Object types */
#define DRM_MODE_OBJECT_ANY       0
#define DRM_MODE_OBJECT_CRTC      0xcccccccc
#define DRM_MODE_OBJECT_CONNECTOR 0xc0c0c0c0
#define DRM_MODE_OBJECT_ENCODER   0xe0e0e0e0
#define DRM_MODE_OBJECT_PROPERTY  0xb0b0b0b0
#define DRM_MODE_OBJECT_FB        0xfbfbfbfb
#define DRM_MODE_OBJECT_PLANE     0xeeeeeeee

/* Property flags */
#define DRM_MODE_PROP_IMMUTABLE (1 << 2)
#define DRM_MODE_PROP_RANGE     (1 << 1)
#define DRM_MODE_PROP_ENUM      (1 << 3)
#define DRM_MODE_PROP_BLOB      (1 << 4)
#define DRM_MODE_PROP_EXTENDED_TYPE 0x0000ffc0
#define DRM_MODE_PROP_TYPE(n)       ((n) << 6)
#define DRM_MODE_PROP_OBJECT        DRM_MODE_PROP_TYPE(1)
#define DRM_MODE_PROP_SIGNED_RANGE  DRM_MODE_PROP_TYPE(2)
#define DRM_MODE_PROP_ATOMIC        0x80000000U

/* Property IDs — we define our own fixed property IDs for simplicity */
#define DRM_PROP_CRTC_ACTIVE   0x1001
#define DRM_PROP_CRTC_MODE_ID  0x1002
#define DRM_PROP_CONNECTOR_DPMS 0x1003
#define DRM_PROP_CONNECTOR_CRTC_ID 0x1004
#define DRM_PROP_PLANE_TYPE     0x1005
#define DRM_PROP_PLANE_FB_ID    0x1006
#define DRM_PROP_PLANE_CRTC_ID  0x1007
#define DRM_PROP_PLANE_SRC_X    0x1008
#define DRM_PROP_PLANE_SRC_Y    0x1009
#define DRM_PROP_PLANE_SRC_W    0x100a
#define DRM_PROP_PLANE_SRC_H    0x100b
#define DRM_PROP_PLANE_CRTC_X   0x100c
#define DRM_PROP_PLANE_CRTC_Y   0x100d
#define DRM_PROP_PLANE_CRTC_W   0x100e
#define DRM_PROP_PLANE_CRTC_H   0x100f
/* Mode flags */
#define DRM_MODE_FLAG_PHSYNC       (1 << 0)
#define DRM_MODE_FLAG_NHSYNC       (1 << 1)
#define DRM_MODE_FLAG_PVSYNC       (1 << 2)
#define DRM_MODE_FLAG_NVSYNC       (1 << 3)
#define DRM_MODE_FLAG_INTERLACE    (1 << 4)
#define DRM_MODE_FLAG_DBLSCAN      (1 << 5)
#define DRM_MODE_FLAG_CSYNC        (1 << 6)
#define DRM_MODE_FLAG_PCSYNC       (1 << 7)
#define DRM_MODE_FLAG_NCSYNC       (1 << 8)
#define DRM_MODE_FLAG_HSKEW        (1 << 9)
#define DRM_MODE_FLAG_BCAST        (1 << 10)
#define DRM_MODE_FLAG_PIXMUX       (1 << 11)
#define DRM_MODE_FLAG_DBLCLK       (1 << 12)
#define DRM_MODE_FLAG_CLKDIV2      (1 << 13)
/* Plane types */
enum { DRM_PLANE_TYPE_OVERLAY = 0, DRM_PLANE_TYPE_PRIMARY = 1, DRM_PLANE_TYPE_CURSOR = 2 };

/* Atomic flags */
#define DRM_MODE_ATOMIC_TEST_ONLY    0x0100
#define DRM_MODE_ATOMIC_NONBLOCK     0x0200
#define DRM_MODE_ATOMIC_ALLOW_MODESET 0x0400
#define DRM_MODE_PAGE_FLIP_EVENT     0x01
#define DRM_MODE_PAGE_FLIP_ASYNC     0x02
#define DRM_MODE_ATOMIC_FLAGS        \
    (DRM_MODE_PAGE_FLIP_EVENT | DRM_MODE_ATOMIC_TEST_ONLY | DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_ATOMIC_ALLOW_MODESET)

#define DRM_MODE_FB_MODIFIERS (1 << 1)

/* ----------------------------------------------------------------------
 * Structures
 * ---------------------------------------------------------------------- */

struct drm_mode_modeinfo {
    uint32_t clock;
    uint16_t hdisplay, hsync_start, hsync_end, htotal, hskew;
    uint16_t vdisplay, vsync_start, vsync_end, vtotal, vscan;
    uint32_t vrefresh;
    uint32_t flags;
    uint32_t type;
    char     name[DRM_DISPLAY_MODE_LEN];
};

struct drm_mode_card_res {
    uint64_t fb_id_ptr;
    uint64_t crtc_id_ptr;
    uint64_t connector_id_ptr;
    uint64_t encoder_id_ptr;
    uint32_t count_fbs;
    uint32_t count_crtcs;
    uint32_t count_connectors;
    uint32_t count_encoders;
    uint32_t min_width, max_width;
    uint32_t min_height, max_height;
};

struct drm_mode_crtc {
    uint64_t set_connectors_ptr;
    uint32_t count_connectors;
    uint32_t crtc_id;
    uint32_t fb_id;
    uint32_t x, y;
    uint32_t gamma_size;
    uint32_t mode_valid;
    struct   drm_mode_modeinfo mode;
};

struct drm_mode_crtc_page_flip {
    uint32_t crtc_id;
    uint32_t fb_id;
    uint32_t flags;
    uint32_t reserved;
    uint64_t user_data;
};

struct drm_mode_get_encoder {
    uint32_t encoder_id;
    uint32_t encoder_type;
    uint32_t crtc_id;
    uint32_t possible_crtcs;
    uint32_t possible_clones;
};

struct drm_mode_get_connector {
    uint64_t encoders_ptr;
    uint64_t modes_ptr;
    uint64_t props_ptr;
    uint64_t prop_values_ptr;
    uint32_t count_modes;
    uint32_t count_props;
    uint32_t count_encoders;
    uint32_t encoder_id;
    uint32_t connector_id;
    uint32_t connector_type;
    uint32_t connector_type_id;
    uint32_t connection;
    uint32_t mm_width, mm_height;
    uint32_t subpixel;
    uint32_t pad;
};

struct drm_mode_get_property {
    uint64_t values_ptr;
    uint64_t enum_blob_ptr;
    uint32_t prop_id;
    uint32_t flags;
    char     name[DRM_PROP_NAME_LEN];
    uint32_t count_values;
    uint32_t count_enum_blobs;
};

struct drm_mode_property_enum {
    uint64_t value;
    char     name[DRM_PROP_NAME_LEN];
};

struct drm_mode_get_blob {
    uint32_t blob_id;
    uint32_t length;
    uint64_t data;
};

struct drm_mode_fb_cmd {
    uint32_t fb_id;
    uint32_t width, height;
    uint32_t pitch;
    uint32_t bpp;
    uint32_t depth;
    uint32_t handle;
};

struct drm_mode_fb_cmd2 {
    uint32_t fb_id;
    uint32_t width, height;
    uint32_t pixel_format;
    uint32_t flags;
    uint32_t handles[4];
    uint32_t pitches[4];
    uint32_t offsets[4];
    uint64_t modifier[4];
};

struct drm_mode_create_dumb {
    uint32_t height;
    uint32_t width;
    uint32_t bpp;
    uint32_t flags;
    uint32_t handle;
    uint32_t pitch;
    uint64_t size;
};

struct drm_mode_map_dumb {
    uint32_t handle;
    uint32_t pad;
    uint64_t offset;
};

struct drm_mode_destroy_dumb {
    uint32_t handle;
};

struct drm_mode_closefb {
    uint32_t fb_id;
    uint32_t pad;
};

struct drm_mode_fb_dirty_cmd {
    uint32_t fb_id;
    uint32_t flags;
    uint32_t color;
    uint32_t num_clips;
    uint64_t clips_ptr;
};

struct drm_mode_get_plane_res {
    uint64_t plane_id_ptr;
    uint32_t count_planes;
};

struct drm_mode_get_plane {
    uint32_t plane_id;
    uint32_t crtc_id;
    uint32_t fb_id;
    uint32_t possible_crtcs;
    uint32_t gamma_size;
    uint32_t count_format_types;
    uint64_t format_type_ptr;
};

struct drm_mode_set_plane {
    uint32_t plane_id;
    uint32_t crtc_id;
    uint32_t fb_id;
    uint32_t flags;
    int32_t  crtc_x, crtc_y;
    uint32_t crtc_w, crtc_h;
    uint32_t src_x, src_y;
    uint32_t src_h, src_w;
};

struct drm_mode_obj_get_properties {
    uint64_t props_ptr;
    uint64_t prop_values_ptr;
    uint32_t count_props;
    uint32_t obj_id;
    uint32_t obj_type;
};

struct drm_mode_atomic {
    uint32_t flags;
    uint32_t count_objs;
    uint64_t objs_ptr;
    uint64_t count_props_ptr;
    uint64_t props_ptr;
    uint64_t prop_values_ptr;
    uint64_t blob_values_ptr;
    uint64_t user_data;
};

struct drm_mode_create_blob {
    uint64_t data;
    uint32_t length;
    uint32_t blob_id;
};

struct drm_mode_destroy_blob {
    uint32_t blob_id;
};

struct drm_mode_create_lease {
    uint64_t object_ids;
    uint32_t object_count;
    uint32_t flags;
    uint32_t lessee_id;
    uint32_t fd;
};

struct drm_mode_list_lessees {
    uint32_t count_lessees;
    uint32_t pad;
    uint64_t lessees_ptr;
};

struct drm_mode_get_lease {
    uint32_t count_objects;
    uint32_t pad;
    uint64_t objects_ptr;
};

struct drm_mode_revoke_lease {
    uint32_t lessee_id;
};
