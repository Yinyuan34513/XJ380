#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EV_SYN 0x00
#define EV_KEY 0x01
#define EV_REL 0x02
#define EV_ABS 0x03
#define EV_MSC 0x04
#define EV_SW  0x05
#define EV_LED 0x11
#define EV_SND 0x12
#define EV_REP 0x14
#define EV_FF  0x15
#define EV_MAX 0x1f
#define EV_CNT (EV_MAX + 1)

#define SYN_REPORT 0
#define SYN_MAX 0x0f
#define SYN_CNT (SYN_MAX + 1)

#define INPUT_PROP_POINTER 0x00
#define INPUT_PROP_MAX     0x1f
#define INPUT_PROP_CNT     (INPUT_PROP_MAX + 1)

#define KEY_RESERVED 0
#define KEY_ESC 1
#define KEY_1 2
#define KEY_0 11
#define KEY_BACKSPACE 14
#define KEY_TAB 15
#define KEY_Q 16
#define KEY_P 25
#define KEY_ENTER 28
#define KEY_A 30
#define KEY_Z 44
#define KEY_SPACE 57
#define KEY_F1 59
#define KEY_F12 88
#define KEY_RIGHTCTRL 97
#define KEY_HOME 102
#define KEY_UP 103
#define KEY_PAGEUP 104
#define KEY_LEFT 105
#define KEY_RIGHT 106
#define KEY_END 107
#define KEY_DOWN 108
#define KEY_PAGEDOWN 109
#define KEY_INSERT 110
#define KEY_DELETE 111
#define KEY_MAX 0x2ff
#define KEY_CNT (KEY_MAX + 1)

#define BTN_LEFT 0x110
#define BTN_RIGHT 0x111
#define BTN_MIDDLE 0x112

#define REL_X 0x00
#define REL_Y 0x01
#define REL_WHEEL 0x08
#define REL_MAX 0x0f
#define REL_CNT (REL_MAX + 1)

#define ABS_MAX 0x3f
#define ABS_CNT (ABS_MAX + 1)

#define MSC_MAX 0x07
#define MSC_CNT (MSC_MAX + 1)

#define LED_MAX 0x0f
#define LED_CNT (LED_MAX + 1)

#define SND_MAX 0x07
#define SND_CNT (SND_MAX + 1)

#define SW_MAX 0x11
#define SW_CNT (SW_MAX + 1)

#define FF_MAX 0x7f
#define FF_CNT (FF_MAX + 1)

#define BUS_I8042 0x11
#define BUS_USB   0x03
#define BUS_VIRTUAL 0x06

#define EVIOCGVERSION 0x80044501
#define EVIOCGID      0x80084502

#define EVDEV_IOC_DIR(nr)  (((nr) >> 30) & 0x3)
#define EVDEV_IOC_SIZE(nr) (((nr) >> 16) & 0x3fff)
#define EVDEV_IOC_TYPE(nr) (((nr) >> 8) & 0xff)
#define EVDEV_IOC_NR(nr)   ((nr) & 0xff)

struct input_event {
    long     tv_sec;
    long     tv_usec;
    uint16_t type;
    uint16_t code;
    int32_t  value;
};

struct input_id {
    uint16_t bustype;
    uint16_t vendor;
    uint16_t product;
    uint16_t version;
};

struct input_absinfo {
    int32_t value;
    int32_t minimum;
    int32_t maximum;
    int32_t fuzz;
    int32_t flat;
    int32_t resolution;
};

void evdev_init(void);
void evdev_push_event(int event_index, uint16_t type, uint16_t code, int32_t value);

#ifdef __cplusplus
}
#endif
