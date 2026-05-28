#include <device.h>
#include <errno.h>
#include <krlibc.h>
#include <tty.h>     /* VT_OPENQRY, VT_SETMODE, KDGETMODE, KDSETMODE, vt_mode, etc. */
#include <vt.h>      /* KDGKBTYPE, vt_stat */

/* ---- Minimal VT state ---- */
static struct {
    bool     inited;
    int      kd_mode;      /* KD_TEXT (0) or KD_GRAPHICS (1) */
    int      vt_mode;      /* VT_AUTO (0) or VT_PROCESS (1) */
    short    vt_relsig;
    short    vt_acqsig;
    int      active_vt;    /* currently active VT number */
} g_vt;

/* ---- Read / Write (no data, this is a control-only device) ---- */
static size_t vt_read(int drive, uint8_t *buf, size_t len, size_t offset)
{
    (void)drive; (void)buf; (void)len; (void)offset;
    return (size_t)-ENODEV;
}

static size_t vt_write(int drive, uint8_t *buf, size_t len, size_t offset)
{
    (void)drive; (void)buf; (void)len; (void)offset;
    return (size_t)-ENODEV;
}

/* ---- IOCTL handler — shared by /dev/tty0 and /dev/tty1 ---- */
static int vt_ioctl(device_t *dev, size_t req, void *arg)
{
    (void)dev;
    if (!g_vt.inited) return -ENODEV;

    write_serial_fmt("[vt-ioctl] req=0x%04x arg=%p\r\n", (unsigned)req, arg);

    switch (req) {

    /* VT_OPENQRY — query next available VT */
    case VT_OPENQRY:
        if (arg) *(int *)arg = 1;  /* VT 1 is "free" */
        write_serial_fmt("[vt-ioctl] VT_OPENQRY -> 1\r\n");
        return 0;

    /* VT_ACTIVATE — switch to VT n (arg is the VT number itself) */
    case VT_ACTIVATE: {
        int vt_num = (int)(uintptr_t)arg;
        write_serial_fmt("[vt-ioctl] VT_ACTIVATE(%d)\r\n", vt_num);
        if (vt_num >= 1 && vt_num <= 12) {
            g_vt.active_vt = vt_num;
            return 0;
        }
        return -EINVAL;
    }

    /* VT_WAITACTIVE — wait until VT n is active (immediate no-op) */
    case VT_WAITACTIVE:
        write_serial_fmt("[vt-ioctl] VT_WAITACTIVE(%d)\r\n", (int)(uintptr_t)arg);
        return 0;

    /* VT_GETMODE — read VT mode (struct vt_mode) */
    case VT_GETMODE:
        if (arg) {
            struct vt_mode *vm = (struct vt_mode *)arg;
            vm->mode   = (char)g_vt.vt_mode;
            vm->waitv  = 0;
            vm->relsig = g_vt.vt_relsig;
            vm->acqsig = g_vt.vt_acqsig;
            vm->frsig  = 0;
        }
        write_serial_fmt("[vt-ioctl] VT_GETMODE\r\n");
        return 0;

    /* VT_SETMODE — set VT mode (struct vt_mode) */
    case VT_SETMODE:
        if (arg) {
            struct vt_mode *vm = (struct vt_mode *)arg;
            g_vt.vt_mode   = vm->mode;
            g_vt.vt_relsig = vm->relsig;
            g_vt.vt_acqsig = vm->acqsig;
        }
        write_serial_fmt("[vt-ioctl] VT_SETMODE\r\n");
        return 0;

    /* VT_GETSTATE — read global VT state (struct vt_stat) */
    case VT_GETSTATE:
        if (arg) {
            struct vt_stat *vs = (struct vt_stat *)arg;
            vs->v_active = (unsigned short)g_vt.active_vt;
            vs->v_signal = 0;
            vs->v_state  = (unsigned short)(1U << (g_vt.active_vt - 1));
        }
        write_serial_fmt("[vt-ioctl] VT_GETSTATE\r\n");
        return 0;

    /* KDGETMODE — get keyboard display mode */
    case KDGETMODE:
        if (arg) *(int *)arg = g_vt.kd_mode;
        write_serial_fmt("[vt-ioctl] KDGETMODE -> %d\r\n", g_vt.kd_mode);
        return 0;

    /* KDSETMODE — set keyboard display mode (KD_TEXT / KD_GRAPHICS) */
    case KDSETMODE: {
        int mode = (int)(uintptr_t)arg;
        write_serial_fmt("[vt-ioctl] KDSETMODE(%d)\r\n", mode);
        if (mode == KD_TEXT || mode == KD_GRAPHICS) {
            g_vt.kd_mode = mode;
            return 0;
        }
        return -EINVAL;
    }

    /* KDSKBMODE — set keyboard mode (K_RAW, K_XLATE, etc.), accept and ignore */
    case KDSKBMODE:
        write_serial_fmt("[vt-ioctl] KDSKBMODE\r\n");
        return 0;

    /* KDGKBMODE — get keyboard mode */
    case KDGKBMODE:
        if (arg) *(int *)arg = K_XLATE;
        write_serial_fmt("[vt-ioctl] KDGKBMODE\r\n");
        return 0;

    /* KDGKBTYPE — query keyboard type */
    case KDGKBTYPE:
        if (arg) *(int *)arg = 0x02;  /* KB_101 */
        write_serial_fmt("[vt-ioctl] KDGKBTYPE\r\n");
        return 0;

    default:
        return -ENOSYS;
    }
}

/* ---- Init ---- */

void vt_init()
{
    if (g_vt.inited) return;

    memset(&g_vt, 0, sizeof(g_vt));
    g_vt.kd_mode   = KD_TEXT;
    g_vt.vt_mode   = VT_AUTO;
    g_vt.active_vt = 1;

    /* Register /dev/tty0 — VT master / "current" VT */
    device_t tty0 = {};
    tty0.type        = DEVICE_STREAM;
    tty0.flag        = 1;
    tty0.sector_size = 1;
    tty0.size        = 0;
    tty0.max_size    = 0;
    tty0.read        = vt_read;
    tty0.write       = vt_write;
    tty0.ioctl       = vt_ioctl;
    tty0.poll        = NULL;
    tty0.map         = NULL;
    strcpy(tty0.drive_name, "tty0");
    regist_device(NULL, tty0);

    /* Register /dev/tty1 — first specific VT */
    device_t tty1 = tty0;
    strcpy(tty1.drive_name, "tty1");
    regist_device(NULL, tty1);

    g_vt.inited = true;
    write_serial_string("vt: XJ380 VT subsystem initialized (tty0, tty1)\n");
}

extern "C" void vt_init_c() { vt_init(); }
