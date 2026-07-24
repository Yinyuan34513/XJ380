#define ALL_IMPLEMENTATION

#include <device.h>
#include <dlinker.h>
#include <errno.h>
#include <fs/vfs/sys.h>
#include <input/evdev.h>
#include <ioctl.h>
#include <krlibc.h>
#include <proto.hpp>
#include <rtc.h>
#include <stdint.h>
#include <task/poll.h>

#define EVDEV_CLOCK_REALTIME 0
#define EVDEV_CLOCK_MONOTONIC 1
#define EVDEV_CLOCK_MONOTONIC_RAW 4
#define EVDEV_CLOCK_MONOTONIC_COARSE 6
#define EVDEV_CLOCK_BOOTTIME 7

typedef struct evdev_device {
    int                index;
    char               name[64];
    char               phys[128];
    char               uniq[64];
    struct input_id    id;
    size_t             properties;
    int                clock_id;
    spin_t             lock;
    size_t             head;
    size_t             tail;
    size_t             count;
    struct input_event events[128];
    uint8_t            evbit[(EV_CNT + 7) / 8];
    uint8_t            keybit[(KEY_CNT + 7) / 8];
    uint8_t            relbit[(REL_CNT + 7) / 8];
    uint8_t            absbit[(ABS_CNT + 7) / 8];
    uint8_t            propbit[(INPUT_PROP_CNT + 7) / 8];
    struct input_absinfo absinfo[ABS_CNT];
} evdev_device_t;

static evdev_device_t g_evdev[2];
static int            g_evdev_device_ids[2] = {-1, -1};
static uint8_t        g_evdev_mouse_buttons;

static void evdev_set_bit(uint8_t *bits, size_t bit)
{
    bits[bit / 8] |= (uint8_t)(1U << (bit % 8));
}

static void evdev_write_sysfs_file(const char *path, const char *content)
{
    if (path == NULL || content == NULL) return;
    vfs_node_t node = sysfs_ensure_file(path);
    write_serial_fmt("[udev-trace] evdev:sysfs-write path=%s node=%p len=%llu first=%.*s\n",
                     path,
                     node,
                     (unsigned long long)strlen(content),
                     (int)MIN(strlen(content), (size_t)80),
                     content);
    if (node == NULL) return;
    sysfs_write_node(node, content, strlen(content), 0);
    vfs_close(node);
}

static const char *evdev_uevent_extra(int index)
{
    return index == 0 ? "ID_INPUT=1\nID_INPUT_KEYBOARD=1\nSUBSYSTEM=input\n"
                      : "ID_INPUT=1\nID_INPUT_MOUSE=1\nSUBSYSTEM=input\n";
}

static bool evdev_append_char(char *out, size_t out_size, size_t *off, char ch)
{
    if (out == NULL || off == NULL || *off + 1 >= out_size) return false;
    out[(*off)++] = ch;
    out[*off] = '\0';
    return true;
}

static bool evdev_append_hex_word(char *out, size_t out_size, size_t *off, unsigned long value, size_t digits)
{
    static const char hex[] = "0123456789abcdef";

    if (digits == 0)
    {
        bool started = false;
        for (size_t pos = sizeof(unsigned long) * 2; pos > 0; pos--)
        {
            size_t       nibble = pos - 1;
            unsigned int digit = (unsigned int)((value >> (nibble * 4)) & 0xf);
            if (digit == 0 && !started) continue;
            started = true;
            if (!evdev_append_char(out, out_size, off, hex[digit])) return false;
        }
        if (!started) return evdev_append_char(out, out_size, off, '0');
        return true;
    }

    for (size_t pos = digits; pos > 0; pos--)
    {
        size_t       nibble = pos - 1;
        unsigned int digit = (unsigned int)((value >> (nibble * 4)) & 0xf);
        if (!evdev_append_char(out, out_size, off, hex[digit])) return false;
    }
    return true;
}

static void evdev_bitmap_to_string(char *out, size_t out_size, const uint8_t *bitmap, size_t bitmap_bytes)
{
    if (out == NULL || out_size == 0) return;
    out[0] = '\0';

    size_t word_bytes = sizeof(unsigned long);
    ssize_t last = (ssize_t)((bitmap_bytes + word_bytes - 1) / word_bytes) - 1;
    while (last > 0)
    {
        bool empty_word = true;
        size_t start = (size_t)last * word_bytes;
        for (size_t i = start; i < bitmap_bytes && i < start + word_bytes; i++)
        {
            if (bitmap[i] != 0)
            {
                empty_word = false;
                break;
            }
        }
        if (!empty_word) break;
        last--;
    }

    size_t off = 0;
    for (ssize_t word = last; word >= 0; word--)
    {
        unsigned long value = 0;
        size_t start = (size_t)word * word_bytes;
        for (size_t i = 0; i < word_bytes && start + i < bitmap_bytes; i++)
        {
            value |= (unsigned long)bitmap[start + i] << (i * 8);
        }

        if (word != last && !evdev_append_char(out, out_size, &off, ' ')) break;
        if (!evdev_append_hex_word(out, out_size, &off, value, word == last ? 0 : word_bytes * 2)) break;
    }

    if (off + 1 < out_size)
    {
        out[off++] = '\n';
        out[off] = '\0';
    }
}

static void evdev_input_path(int index, char *out, size_t out_size)
{
    snprintf(out, out_size, "/sys/devices/platform/i8042/serio%d/input%d", index, index);
}

static void evdev_event_path(int index, char *out, size_t out_size)
{
    snprintf(out, out_size, "/sys/devices/platform/i8042/serio%d/input%d/event%d", index, index, index);
}

static evdev_device_t *evdev_by_drive(int drive)
{
    for (size_t i = 0; i < sizeof(g_evdev_device_ids) / sizeof(g_evdev_device_ids[0]); i++)
    {
        if (g_evdev_device_ids[i] == drive) return &g_evdev[i];
    }
    return NULL;
}

static evdev_device_t *evdev_by_index(int index)
{
    if (index < 0 || index >= (int)(sizeof(g_evdev) / sizeof(g_evdev[0]))) return NULL;
    return &g_evdev[index];
}

static uint64_t evdev_clock_now_ns(const evdev_device_t *dev)
{
    if (dev != NULL && dev->clock_id == EVDEV_CLOCK_REALTIME) return realtime_ns();
    return bootNanoTime();
}

static void evdev_stamp_event(struct input_event *event, uint64_t ns)
{
    if (event == NULL) return;
    event->tv_sec = (long)(ns / 1000000000ULL);
    event->tv_usec = (long)((ns % 1000000000ULL) / 1000ULL);
}

static void evdev_drop_oldest_frame(evdev_device_t *dev)
{
    if (dev == NULL || dev->count == 0) return;

    size_t capacity = sizeof(dev->events) / sizeof(dev->events[0]);
    bool dropped_syn = false;
    while (dev->count != 0)
    {
        struct input_event *old = &dev->events[dev->head];
        dropped_syn = old->type == EV_SYN && old->code == SYN_REPORT;
        dev->head = (dev->head + 1) % capacity;
        dev->count--;
        if (dropped_syn) break;
    }

    if (!dropped_syn)
    {
        dev->head = dev->tail;
        dev->count = 0;
    }
}

static void evdev_queue_event_locked(evdev_device_t *dev, const struct input_event *src)
{
    size_t capacity = sizeof(dev->events) / sizeof(dev->events[0]);
    if (dev->count == sizeof(dev->events) / sizeof(dev->events[0]))
    {
        evdev_drop_oldest_frame(dev);
    }
    struct input_event *event = &dev->events[dev->tail];
    memcpy(event, src, sizeof(*event));
    dev->tail = (dev->tail + 1) % capacity;
    dev->count++;
}

static bool evdev_frame_has_syn_report(const struct input_event *events, size_t count)
{
    if (events == NULL) return false;
    for (size_t i = 0; i < count; i++)
    {
        if (events[i].type == EV_SYN && events[i].code == SYN_REPORT) return true;
    }
    return false;
}

static bool evdev_coalesce_mouse_frame_locked(evdev_device_t *dev, struct input_event *frame, size_t count)
{
    if (dev == NULL || frame == NULL || dev->index != 1 || count == 0 || dev->count == 0) return false;
    if (frame[count - 1].type != EV_SYN || frame[count - 1].code != SYN_REPORT) return false;

    size_t capacity = sizeof(dev->events) / sizeof(dev->events[0]);
    struct input_event *last = &dev->events[(dev->tail + capacity - 1) % capacity];
    if (last->type != EV_SYN || last->code != SYN_REPORT) return false;

    size_t frame_start = dev->tail;
    size_t frame_count = 0;
    size_t scan = dev->tail;

    for (size_t inspected = 0; inspected < dev->count; inspected++)
    {
        scan = (scan + capacity - 1) % capacity;
        frame_start = scan;
        frame_count++;

        struct input_event *event = &dev->events[scan];
        if (event->type == EV_SYN && event->code == SYN_REPORT && frame_count > 1)
        {
            frame_start = (scan + 1) % capacity;
            frame_count--;
            break;
        }
    }

    if (frame_count == 0) return false;

    bool has_relative = false;
    int merged_x = 0;
    int merged_y = 0;
    int merged_wheel = 0;
    for (size_t i = 0; i < frame_count; i++)
    {
        struct input_event *event = &dev->events[(frame_start + i) % capacity];
        if (event->type == EV_SYN && event->code == SYN_REPORT) continue;
        if (event->type != EV_REL)
        {
            return false;
        }
        if (event->code == REL_X) merged_x += event->value;
        else if (event->code == REL_Y) merged_y += event->value;
        else if (event->code == REL_WHEEL) merged_wheel += event->value;
        else return false;
        has_relative = true;
    }
    if (!has_relative) return false;

    for (size_t i = 0; i + 1 < count; i++)
    {
        if (frame[i].type != EV_REL) return false;
        if (frame[i].code == REL_X) merged_x += frame[i].value;
        else if (frame[i].code == REL_Y) merged_y += frame[i].value;
        else if (frame[i].code == REL_WHEEL) merged_wheel += frame[i].value;
        else return false;
    }

    struct input_event merged[4] = {};
    size_t merged_count = 0;
    uint64_t ns = (uint64_t)frame[count - 1].tv_sec * 1000000000ULL + (uint64_t)frame[count - 1].tv_usec * 1000ULL;

    if (merged_x != 0)
    {
        evdev_stamp_event(&merged[merged_count], ns);
        merged[merged_count].type = EV_REL;
        merged[merged_count].code = REL_X;
        merged[merged_count].value = merged_x;
        merged_count++;
    }
    if (merged_y != 0)
    {
        evdev_stamp_event(&merged[merged_count], ns);
        merged[merged_count].type = EV_REL;
        merged[merged_count].code = REL_Y;
        merged[merged_count].value = merged_y;
        merged_count++;
    }
    if (merged_wheel != 0)
    {
        evdev_stamp_event(&merged[merged_count], ns);
        merged[merged_count].type = EV_REL;
        merged[merged_count].code = REL_WHEEL;
        merged[merged_count].value = merged_wheel;
        merged_count++;
    }

    if (merged_count == 0) return false;

    evdev_stamp_event(&merged[merged_count], ns);
    merged[merged_count].type = EV_SYN;
    merged[merged_count].code = SYN_REPORT;
    merged_count++;

    dev->tail = frame_start;
    dev->count -= frame_count;
    for (size_t i = 0; i < merged_count; i++)
        evdev_queue_event_locked(dev, &merged[i]);
    return true;
}

void evdev_push_event(int event_index, uint16_t type, uint16_t code, int32_t value)
{
    evdev_device_t *dev = evdev_by_index(event_index);
    if (dev == NULL) return;

    uint64_t ns = evdev_clock_now_ns(dev);
    struct input_event event = {};
    evdev_stamp_event(&event, ns);
    event.type = type;
    event.code = code;
    event.value = value;

    spin_lock(&dev->lock);
    bool was_empty = dev->count == 0;
    evdev_queue_event_locked(dev, &event);
    spin_unlock(&dev->lock);

    if (was_empty || (type == EV_SYN && code == SYN_REPORT)) poll_notify_event();
}

static void evdev_push_frame(int event_index, const struct input_event *events, size_t count)
{
    evdev_device_t *dev = evdev_by_index(event_index);
    if (dev == NULL || events == NULL || count == 0) return;

    uint64_t ns = evdev_clock_now_ns(dev);
    struct input_event frame[8];
    if (count > sizeof(frame) / sizeof(frame[0])) count = sizeof(frame) / sizeof(frame[0]);

    for (size_t i = 0; i < count; i++)
    {
        frame[i] = events[i];
        evdev_stamp_event(&frame[i], ns);
    }

    bool notify = false;
    spin_lock(&dev->lock);
    bool was_empty = dev->count == 0;
    if (evdev_coalesce_mouse_frame_locked(dev, frame, count))
    {
        spin_unlock(&dev->lock);
        poll_notify_event();
        return;
    }

    while (dev->count + count > sizeof(dev->events) / sizeof(dev->events[0]))
        evdev_drop_oldest_frame(dev);

    for (size_t i = 0; i < count; i++)
        evdev_queue_event_locked(dev, &frame[i]);
    notify = was_empty || evdev_frame_has_syn_report(frame, count);
    spin_unlock(&dev->lock);

    if (notify) poll_notify_event();
}

static uint16_t evdev_usb_keyboard_usage_to_key(uint8_t usage)
{
    if (usage >= 0x1e && usage <= 0x26) return (uint16_t)(KEY_1 + usage - 0x1e);

    switch (usage)
    {
    case 0x04: return KEY_A;
    case 0x05: return KEY_B;
    case 0x06: return KEY_C;
    case 0x07: return KEY_D;
    case 0x08: return KEY_E;
    case 0x09: return KEY_F;
    case 0x0a: return KEY_G;
    case 0x0b: return KEY_H;
    case 0x0c: return KEY_I;
    case 0x0d: return KEY_J;
    case 0x0e: return KEY_K;
    case 0x0f: return KEY_L;
    case 0x10: return KEY_M;
    case 0x11: return KEY_N;
    case 0x12: return KEY_O;
    case 0x13: return KEY_P;
    case 0x14: return KEY_Q;
    case 0x15: return KEY_R;
    case 0x16: return KEY_S;
    case 0x17: return KEY_T;
    case 0x18: return KEY_U;
    case 0x19: return KEY_V;
    case 0x1a: return KEY_W;
    case 0x1b: return KEY_X;
    case 0x1c: return KEY_Y;
    case 0x1d: return KEY_Z;
    case 0x27: return KEY_0;
    case 0x28: return KEY_ENTER;
    case 0x29: return KEY_ESC;
    case 0x2a: return KEY_BACKSPACE;
    case 0x2b: return KEY_TAB;
    case 0x2c: return KEY_SPACE;
    case 0x2d: return KEY_MINUS;
    case 0x2e: return KEY_EQUAL;
    case 0x2f: return KEY_LEFTBRACE;
    case 0x30: return KEY_RIGHTBRACE;
    case 0x31: return KEY_BACKSLASH;
    case 0x33: return KEY_SEMICOLON;
    case 0x34: return KEY_APOSTROPHE;
    case 0x35: return KEY_GRAVE;
    case 0x36: return KEY_COMMA;
    case 0x37: return KEY_DOT;
    case 0x38: return KEY_SLASH;
    case 0x39: return KEY_CAPSLOCK;
    case 0x49: return KEY_INSERT;
    case 0x4a: return KEY_HOME;
    case 0x4b: return KEY_PAGEUP;
    case 0x4c: return KEY_DELETE;
    case 0x4d: return KEY_END;
    case 0x4e: return KEY_PAGEDOWN;
    case 0x4f: return KEY_RIGHT;
    case 0x50: return KEY_LEFT;
    case 0x51: return KEY_DOWN;
    case 0x52: return KEY_UP;
    case 0xe0: return KEY_LEFTCTRL;
    case 0xe1: return KEY_LEFTSHIFT;
    case 0xe2: return KEY_LEFTALT;
    case 0xe3: return KEY_LEFTMETA;
    case 0xe4: return KEY_RIGHTCTRL;
    case 0xe5: return KEY_RIGHTSHIFT;
    case 0xe6: return KEY_RIGHTALT;
    case 0xe7: return KEY_RIGHTMETA;
    default:
        if (usage >= 0x3a && usage <= 0x45) return (uint16_t)(KEY_F1 + usage - 0x3a);
        return KEY_RESERVED;
    }
}

static uint16_t evdev_ps2_scancode_to_key(uint8_t scancode, bool extended)
{
    uint8_t code = scancode & 0x7f;
    if (!extended)
    {
        return code > 0 && code <= KEY_F12 ? code : KEY_RESERVED;
    }

    switch (code)
    {
    case 0x1c: return 96;
    case 0x1d: return KEY_RIGHTCTRL;
    case 0x35: return 98;
    case 0x38: return KEY_RIGHTALT;
    case 0x47: return KEY_HOME;
    case 0x48: return KEY_UP;
    case 0x49: return KEY_PAGEUP;
    case 0x4b: return KEY_LEFT;
    case 0x4d: return KEY_RIGHT;
    case 0x4f: return KEY_END;
    case 0x50: return KEY_DOWN;
    case 0x51: return KEY_PAGEDOWN;
    case 0x52: return KEY_INSERT;
    case 0x53: return KEY_DELETE;
    case 0x5b: return KEY_LEFTMETA;
    case 0x5c: return KEY_RIGHTMETA;
    case 0x5d: return 139;
    default: return KEY_RESERVED;
    }
}

void evdev_push_ps2_keyboard_event(uint8_t scancode, uint8_t extended)
{
    uint16_t key = evdev_ps2_scancode_to_key(scancode, extended != 0);
    if (key == KEY_RESERVED) return;

    struct input_event frame[2] = {};
    frame[0].type = EV_KEY;
    frame[0].code = key;
    frame[0].value = (scancode & 0x80) == 0 ? 1 : 0;
    frame[1].type = EV_SYN;
    frame[1].code = SYN_REPORT;
    evdev_push_frame(0, frame, 2);
}

void evdev_push_usb_keyboard_event(uint8_t usage, uint8_t pressed)
{
    uint16_t key = evdev_usb_keyboard_usage_to_key(usage);
    if (key == KEY_RESERVED) return;

    struct input_event frame[2] = {};
    frame[0].type = EV_KEY;
    frame[0].code = key;
    frame[0].value = pressed ? 1 : 0;
    frame[1].type = EV_SYN;
    frame[1].code = SYN_REPORT;
    evdev_push_frame(0, frame, 2);
}

void evdev_push_mouse_report(int dx, int dy, uint8_t buttons, int wheel)
{
    struct input_event frame[8] = {};
    size_t count = 0;

    if (dx != 0)
    {
        frame[count].type = EV_REL;
        frame[count].code = REL_X;
        frame[count].value = dx;
        count++;
    }
    if (dy != 0)
    {
        frame[count].type = EV_REL;
        frame[count].code = REL_Y;
        frame[count].value = dy;
        count++;
    }
    if (wheel != 0)
    {
        frame[count].type = EV_REL;
        frame[count].code = REL_WHEEL;
        frame[count].value = wheel;
        count++;
    }

    buttons &= 0x07;
    if ((buttons ^ g_evdev_mouse_buttons) & 0x1)
    {
        frame[count].type = EV_KEY;
        frame[count].code = BTN_LEFT;
        frame[count].value = buttons & 0x1 ? 1 : 0;
        count++;
    }
    if ((buttons ^ g_evdev_mouse_buttons) & 0x2)
    {
        frame[count].type = EV_KEY;
        frame[count].code = BTN_RIGHT;
        frame[count].value = buttons & 0x2 ? 1 : 0;
        count++;
    }
    if ((buttons ^ g_evdev_mouse_buttons) & 0x4)
    {
        frame[count].type = EV_KEY;
        frame[count].code = BTN_MIDDLE;
        frame[count].value = buttons & 0x4 ? 1 : 0;
        count++;
    }

    g_evdev_mouse_buttons = buttons;
    if (count == 0) return;

    frame[count].type = EV_SYN;
    frame[count].code = SYN_REPORT;
    count++;
    evdev_push_frame(1, frame, count);
}

static size_t evdev_read(int drive, uint8_t *buffer, size_t number, size_t offset)
{
    (void)offset;
    evdev_device_t *dev = evdev_by_drive(drive);
    if (dev == NULL || buffer == NULL) return VFS_STATUS_FAILED;
    if (number < sizeof(struct input_event)) return (size_t)-EINVAL;

    spin_lock(&dev->lock);
    if (dev->count == 0)
    {
        spin_unlock(&dev->lock);
        return (size_t)-EWOULDBLOCK;
    }

    size_t written = 0;
    while (dev->count != 0 && written + sizeof(struct input_event) <= number)
    {
        memcpy(buffer + written, &dev->events[dev->head], sizeof(struct input_event));
        dev->head = (dev->head + 1) % (sizeof(dev->events) / sizeof(dev->events[0]));
        dev->count--;
        written += sizeof(struct input_event);
    }
    spin_unlock(&dev->lock);

    return written;
}

static size_t evdev_write(int drive, uint8_t *buffer, size_t number, size_t offset)
{
    (void)drive;
    (void)buffer;
    (void)offset;
    return number;
}

static int evdev_ioctl(struct _device *device, size_t req, void *arg)
{
    if (device == NULL) return -EINVAL;
    evdev_device_t *dev = evdev_by_drive((int)device->vdiskid);
    if (dev == NULL) return -ENODEV;
    size_t req_size = EVDEV_IOC_SIZE(req);
    size_t req_nr = EVDEV_IOC_NR(req);

    if (req_nr == 0x90 || req_nr == 0x91)
    {
        return 0;
    }

    if (arg == NULL && req_size != 0) return -EFAULT;

    if (req == EVIOCGVERSION)
    {
        *(int *)arg = 0x010001;
        return 0;
    }
    if (req == EVIOCGID)
    {
        memcpy(arg, &dev->id, sizeof(dev->id));
        return 0;
    }

    if (EVDEV_IOC_TYPE(req) != 'E') return -ENOTTY;

    if (req_nr == 0x03)
    {
        unsigned int repeat[2] = {500, 33};
        size_t copy = MIN(req_size, sizeof(repeat));
        memcpy(arg, repeat, copy);
        return (int)copy;
    }

    if (req_nr == 0x06)
    {
        size_t copy = MIN(req_size, strlen(dev->name) + 1);
        memcpy(arg, dev->name, copy);
        return (int)copy;
    }

    if (req_nr == 0x07)
    {
        size_t copy = MIN(req_size, strlen(dev->phys) + 1);
        memcpy(arg, dev->phys, copy);
        return (int)copy;
    }

    if (req_nr == 0x08)
    {
        size_t copy = MIN(req_size, strlen(dev->uniq) + 1);
        memcpy(arg, dev->uniq, copy);
        return (int)copy;
    }

    if (req_nr == 0x09)
    {
        memset(arg, 0, req_size);
        size_t copy = MIN(req_size, sizeof(dev->propbit));
        memcpy(arg, dev->propbit, copy);
        return (int)copy;
    }

    if (req_nr == 0x18)
    {
        memset(arg, 0, req_size);
        return (int)req_size;
    }

    if (req_nr == 0x19 || req_nr == 0x1a || req_nr == 0x1b)
    {
        memset(arg, 0, req_size);
        return (int)req_size;
    }

    if (req_nr >= 0x20 && req_nr < 0x20 + EV_CNT)
    {
        size_t copy = req_size;
        uint8_t *src = NULL;
        size_t src_size = 0;
        switch (req_nr - 0x20)
        {
        case EV_SYN:
            src = dev->evbit;
            src_size = sizeof(dev->evbit);
            break;
        case EV_KEY:
            src = dev->keybit;
            src_size = sizeof(dev->keybit);
            break;
        case EV_REL:
            src = dev->relbit;
            src_size = sizeof(dev->relbit);
            break;
        case EV_ABS:
            src = dev->absbit;
            src_size = sizeof(dev->absbit);
            break;
        default:
            memset(arg, 0, copy);
            return (int)copy;
        }
        memset(arg, 0, copy);
        memcpy(arg, src, MIN(copy, src_size));
        return (int)copy;
    }

    if (req_nr >= 0x40 && req_nr < 0x40 + ABS_CNT)
    {
        uint16_t abs = (uint16_t)(req_nr - 0x40);
        size_t copy = MIN(req_size, sizeof(dev->absinfo[abs]));
        memcpy(arg, &dev->absinfo[abs], copy);
        return (int)copy;
    }

    if (req_nr == 0x84)
    {
        if (req_size != 0) *(int *)arg = 0;
        return req_size != 0 ? (int)sizeof(int) : 0;
    }

    if (req_nr == 0xa0)
    {
        if (arg == NULL) return -EFAULT;
        int clock_id = *(int *)arg;
        switch (clock_id)
        {
        case EVDEV_CLOCK_REALTIME:
        case EVDEV_CLOCK_MONOTONIC:
        case EVDEV_CLOCK_MONOTONIC_RAW:
        case EVDEV_CLOCK_MONOTONIC_COARSE:
        case EVDEV_CLOCK_BOOTTIME:
            dev->clock_id = clock_id;
            break;
        default:
            return -EINVAL;
        }
        return 0;
    }

    return -ENOTTY;
}

static int evdev_poll_one(int index, size_t events)
{
    evdev_device_t *dev = evdev_by_index(index);
    if (dev == NULL) return EPOLLERR;

    int revents = 0;
    spin_lock(&dev->lock);
    if ((events & EPOLLIN) && dev->count != 0) revents |= EPOLLIN;
    spin_unlock(&dev->lock);

    if (events & EPOLLOUT) revents |= EPOLLOUT;
    return revents;
}

static int evdev_keyboard_poll(size_t events)
{
    return evdev_poll_one(0, events);
}

static int evdev_mouse_poll(size_t events)
{
    return evdev_poll_one(1, events);
}

static void evdev_publish_metadata(int index)
{
    evdev_device_t *dev = &g_evdev[index];
    char input_path[128];
    char input_name[32];
    char event_path[128];
    char event_name[32];
    char path[192];
    char content[2048];
    snprintf(input_name, sizeof(input_name), "input%d", index);
    snprintf(event_name, sizeof(event_name), "event%d", index);
    evdev_input_path(index, input_path, sizeof(input_path));
    evdev_event_path(index, event_path, sizeof(event_path));
    write_serial_fmt("[udev-trace] evdev:publish begin index=%d input_path=%s event_path=%s name=%s\n",
                     index,
                     input_path,
                     event_path,
                     dev->name);

    sysfs_ensure_dir(input_path);
    sysfs_ensure_dir(event_path);

    snprintf(path, sizeof(path), "%s/uevent", input_path);
    evdev_write_sysfs_file(path, evdev_uevent_extra(index));

    snprintf(path, sizeof(path), "/sys/class/input/%s", input_name);
    sysfs_ensure_symlink(path, input_path);
    snprintf(path, sizeof(path), "/sys/class/input/%s", event_name);
    sysfs_ensure_symlink(path, event_path);

    snprintf(path, sizeof(path), "%s/subsystem", input_path);
    sysfs_ensure_symlink(path, "/sys/class/input");
    snprintf(path, sizeof(path), "%s/device", event_path);
    sysfs_ensure_symlink(path, input_path);

    snprintf(path, sizeof(path), "%s/name", input_path);
    snprintf(content, sizeof(content), "%s\n", dev->name);
    evdev_write_sysfs_file(path, content);

    snprintf(path, sizeof(path), "%s/phys", input_path);
    snprintf(content, sizeof(content), "%s\n", dev->phys);
    evdev_write_sysfs_file(path, content);

    snprintf(path, sizeof(path), "%s/uniq", input_path);
    snprintf(content, sizeof(content), "%s\n", dev->uniq);
    evdev_write_sysfs_file(path, content);

    snprintf(path, sizeof(path), "%s/properties", input_path);
    snprintf(content, sizeof(content), "%lx\n", (unsigned long)dev->properties);
    evdev_write_sysfs_file(path, content);

    snprintf(path, sizeof(path), "%s/modalias", input_path);
    snprintf(content,
             sizeof(content),
             "input:b%04Xv%04Xp%04Xe%04X\n",
             dev->id.bustype,
             dev->id.vendor,
             dev->id.product,
             dev->id.version);
    evdev_write_sysfs_file(path, content);

    snprintf(path, sizeof(path), "%s/id", input_path);
    sysfs_ensure_dir(path);
    snprintf(path, sizeof(path), "%s/id/bustype", input_path);
    snprintf(content, sizeof(content), "%04x\n", dev->id.bustype);
    evdev_write_sysfs_file(path, content);
    snprintf(path, sizeof(path), "%s/id/vendor", input_path);
    snprintf(content, sizeof(content), "%04x\n", dev->id.vendor);
    evdev_write_sysfs_file(path, content);
    snprintf(path, sizeof(path), "%s/id/product", input_path);
    snprintf(content, sizeof(content), "%04x\n", dev->id.product);
    evdev_write_sysfs_file(path, content);
    snprintf(path, sizeof(path), "%s/id/version", input_path);
    snprintf(content, sizeof(content), "%04x\n", dev->id.version);
    evdev_write_sysfs_file(path, content);

    snprintf(path, sizeof(path), "%s/capabilities", input_path);
    sysfs_ensure_dir(path);
    snprintf(path, sizeof(path), "%s/capabilities/ev", input_path);
    evdev_bitmap_to_string(content, sizeof(content), dev->evbit, sizeof(dev->evbit));
    evdev_write_sysfs_file(path, content);
    snprintf(path, sizeof(path), "%s/capabilities/key", input_path);
    evdev_bitmap_to_string(content, sizeof(content), dev->keybit, sizeof(dev->keybit));
    evdev_write_sysfs_file(path, content);
    snprintf(path, sizeof(path), "%s/capabilities/rel", input_path);
    evdev_bitmap_to_string(content, sizeof(content), dev->relbit, sizeof(dev->relbit));
    evdev_write_sysfs_file(path, content);
    snprintf(path, sizeof(path), "%s/capabilities/abs", input_path);
    evdev_bitmap_to_string(content, sizeof(content), dev->absbit, sizeof(dev->absbit));
    evdev_write_sysfs_file(path, content);
    snprintf(path, sizeof(path), "%s/capabilities/prop", input_path);
    evdev_bitmap_to_string(content, sizeof(content), dev->propbit, sizeof(dev->propbit));
    evdev_write_sysfs_file(path, content);
    snprintf(path, sizeof(path), "%s/capabilities/msc", input_path);
    evdev_write_sysfs_file(path, "0\n");
    snprintf(path, sizeof(path), "%s/capabilities/led", input_path);
    evdev_write_sysfs_file(path, "0\n");
    snprintf(path, sizeof(path), "%s/capabilities/snd", input_path);
    evdev_write_sysfs_file(path, "0\n");
    snprintf(path, sizeof(path), "%s/capabilities/sw", input_path);
    evdev_write_sysfs_file(path, "0\n");
    snprintf(path, sizeof(path), "%s/capabilities/ff", input_path);
    evdev_write_sysfs_file(path, "0\n");

    write_serial_fmt("evdev: sysfs metadata input%d event%d name=%s\n", index, index, dev->name);
    write_serial_fmt("[udev-trace] evdev:publish done index=%d input=%s event=%s\n",
                     index,
                     input_path,
                     event_path);
}

static void evdev_setup_one(int index, const char *name, bool mouse)
{
    evdev_device_t *dev = &g_evdev[index];
    memset(dev, 0, sizeof(*dev));
    dev->index = index;
    dev->lock = SPIN_INIT;
    strncpy(dev->name, name, sizeof(dev->name) - 1);
    snprintf(dev->phys, sizeof(dev->phys), "/sys/devices/platform/i8042/serio%d/input%d", index, index);
    snprintf(dev->uniq, sizeof(dev->uniq), "xj380-event%d", index);
    dev->id.bustype = BUS_I8042;
    dev->id.vendor = 0x1;
    dev->id.product = (uint16_t)(index + 1);
    dev->id.version = 1;
    dev->clock_id = EVDEV_CLOCK_MONOTONIC;
    evdev_set_bit(dev->evbit, EV_SYN);
    evdev_set_bit(dev->evbit, mouse ? EV_REL : EV_KEY);
    if (mouse)
    {
        dev->properties = 1UL << INPUT_PROP_POINTER;
        evdev_set_bit(dev->propbit, INPUT_PROP_POINTER);
        evdev_set_bit(dev->evbit, EV_KEY);
        evdev_set_bit(dev->keybit, BTN_LEFT);
        evdev_set_bit(dev->keybit, BTN_RIGHT);
        evdev_set_bit(dev->keybit, BTN_MIDDLE);
        evdev_set_bit(dev->relbit, REL_X);
        evdev_set_bit(dev->relbit, REL_Y);
        evdev_set_bit(dev->relbit, REL_WHEEL);
    }
    else
    {
        static const uint16_t keyboard_keys[] = {
            KEY_ESC,        KEY_1,          KEY_2,        KEY_3,          KEY_4,         KEY_5,
            KEY_6,          KEY_7,          KEY_8,        KEY_9,          KEY_0,         KEY_MINUS,
            KEY_EQUAL,      KEY_BACKSPACE,  KEY_TAB,      KEY_Q,          KEY_W,         KEY_E,
            KEY_R,          KEY_T,          KEY_Y,        KEY_U,          KEY_I,         KEY_O,
            KEY_P,          KEY_LEFTBRACE,  KEY_RIGHTBRACE, KEY_ENTER,    KEY_LEFTCTRL,  KEY_A,
            KEY_S,          KEY_D,          KEY_F,        KEY_G,          KEY_H,         KEY_J,
            KEY_K,          KEY_L,          KEY_SEMICOLON, KEY_APOSTROPHE, KEY_GRAVE,    KEY_LEFTSHIFT,
            KEY_BACKSLASH,  KEY_Z,          KEY_X,        KEY_C,          KEY_V,         KEY_B,
            KEY_N,          KEY_M,          KEY_COMMA,    KEY_DOT,        KEY_SLASH,     KEY_RIGHTSHIFT,
            KEY_LEFTALT,    KEY_SPACE,      KEY_CAPSLOCK, KEY_RIGHTCTRL,  KEY_RIGHTALT,  KEY_LEFTMETA,
            KEY_RIGHTMETA,  KEY_HOME,       KEY_UP,       KEY_PAGEUP,     KEY_LEFT,      KEY_RIGHT,
            KEY_END,        KEY_DOWN,       KEY_PAGEDOWN, KEY_INSERT,     KEY_DELETE,
        };
        for (size_t i = 0; i < sizeof(keyboard_keys) / sizeof(keyboard_keys[0]); i++)
        {
            evdev_set_bit(dev->keybit, keyboard_keys[i]);
        }
        for (uint16_t key = KEY_F1; key <= KEY_F12; key++) evdev_set_bit(dev->keybit, key);
    }

    device_t device = {};
    device.type = DEVICE_STREAM;
    device.flag = 1;
    device.sector_size = 1;
    device.size = (size_t)-1;
    device.read = evdev_read;
    device.write = evdev_write;
    device.ioctl = evdev_ioctl;
    device.poll = mouse ? evdev_mouse_poll : evdev_keyboard_poll;
    device.map = (mapf)empty;
    snprintf(device.drive_name, sizeof(device.drive_name), "event%d", index);
    device.major = 13;
    device.minor = (uint32_t)(64 + index);
    char devnode[64];
    snprintf(devnode, sizeof(devnode), "input/event%d", index);
    evdev_publish_metadata(index);
    write_serial_fmt("[udev-trace] evdev:setup index=%d devnode=%s name=%s major=%u minor=%u\n",
                     index,
                     devnode,
                     dev->name,
                     device.major,
                     device.minor);
    int device_id = regist_device(devnode, device);
    write_serial_fmt("[udev-trace] evdev:regist-device index=%d id=%d devnode=%s\n",
                     index,
                     device_id,
                     devnode);
    if (device_id >= 0)
    {
        g_evdev_device_ids[index] = device_id;
        char event_path[128];
        char real_path[128];
        evdev_event_path(index, event_path, sizeof(event_path));
        snprintf(real_path, sizeof(real_path), "input/event%d", index);
        write_serial_fmt("[udev-trace] evdev:sysfs-regist-dev index=%d event_path=%s real_path=%s\n",
                         index,
                         event_path,
                         real_path);
        sysfs_regist_dev('c',
                         (int)device.major,
                         (int)device.minor,
                         event_path,
                         real_path,
                         evdev_uevent_extra(index),
                         "/sys/class/input",
                         "/sys/class/input",
                         device.drive_name,
                         "/sys/devices/platform/i8042");
    }
}

void evdev_init(void)
{
    write_serial_string("[udev-trace] evdev:init begin\n");
    errno_t mkdir_ret = vfs_mkdir("/dev/input");
    write_serial_fmt("[udev-trace] evdev:init mkdir /dev/input ret=%d\n", mkdir_ret);
    evdev_setup_one(0, "XJ380 virtual keyboard", false);
    evdev_setup_one(1, "XJ380 virtual mouse", true);
    write_serial_string("[udev-trace] evdev:init done\n");
}

EXPORT_SYMBOL(evdev_push_usb_keyboard_event);
EXPORT_SYMBOL(evdev_push_mouse_report);
