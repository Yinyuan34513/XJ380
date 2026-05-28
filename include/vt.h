#pragma once
/* Additional VT/KBD ioctl constants beyond those in tty.h */

#define KDGKBTYPE 0x4B33  /* get keyboard type */

/* vt_stat — named differently from tty.h's vt_state to avoid confusion */
struct vt_stat {
    unsigned short v_active;  /* active VT */
    unsigned short v_signal;  /* signal to send */
    unsigned short v_state;   /* VT bitmap */
};
