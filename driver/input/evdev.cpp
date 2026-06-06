#define ALL_IMPLEMENTATION

#include <device.h>
#include <errno.h>
#include <fs/vfs/sys.h>
#include <input/evdev.h>
#include <ioctl.h>
#include <krlibc.h>
#include <proto.hpp>
#include <rtc.h>
#include <stdint.h>
#include <task/poll.h>

#define EVDEV_CLOCK_MONOTONIC 1

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
                      : "ID_INPUT=1\nID_INPUT_MOUSE=1\nID_INPUT_POINTINGSTICK=1\nSUBSYSTEM=input\n";
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

        int written = snprintf(out + off,
                               out_size - off,
                               word == last ? "%lx" : " %0*lx",
                               value,
                               (int)(word_bytes * 2),
                               value);
        if (written < 0 || (size_t)written >= out_size - off) break;
        off += (size_t)written;
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

void evdev_push_event(int event_index, uint16_t type, uint16_t code, int32_t value)
{
    evdev_device_t *dev = evdev_by_index(event_index);
    if (dev == NULL) return;
    tm now = {};
    time_read(&now);
    spin_lock(&dev->lock);
    if (dev->count == sizeof(dev->events) / sizeof(dev->events[0]))
    {
        dev->head = (dev->head + 1) % (sizeof(dev->events) / sizeof(dev->events[0]));
        dev->count--;
    }
    struct input_event *event = &dev->events[dev->tail];
    memset(event, 0, sizeof(*event));
    event->tv_sec = (long)mktime(&now);
    event->tv_usec = 0;
    event->type = type;
    event->code = code;
    event->value = value;
    dev->tail = (dev->tail + 1) % (sizeof(dev->events) / sizeof(dev->events[0]));
    dev->count++;
    spin_unlock(&dev->lock);
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
        dev->clock_id = *(int *)arg;
        return 0;
    }

    return -ENOTTY;
}

static int evdev_poll(size_t events)
{
    int revents = 0;
    for (size_t i = 0; i < sizeof(g_evdev) / sizeof(g_evdev[0]); i++)
    {
        spin_lock(&g_evdev[i].lock);
        if ((events & EPOLLIN) && g_evdev[i].count != 0) revents |= EPOLLIN;
        spin_unlock(&g_evdev[i].lock);
    }
    if (events & EPOLLOUT) revents |= EPOLLOUT;
    return revents;
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

static void evdev_ready(struct _device *device)
{
    if (device == NULL) return;
    evdev_device_t *dev = evdev_by_drive((int)device->vdiskid);
    if (dev == NULL) return;
    write_serial_fmt("[udev-trace] evdev:ready drive=%llu index=%d name=%s\n",
                     (unsigned long long)device->vdiskid,
                     dev->index,
                     dev->name);
    evdev_publish_metadata(dev->index);
}

static void evdev_setup_one(int index, const char *name, bool mouse)
{
    evdev_device_t *dev = &g_evdev[index];
    memset(dev, 0, sizeof(*dev));
    dev->index = index;
    dev->lock = SPIN_INIT;
    strncpy(dev->name, name, sizeof(dev->name) - 1);
    snprintf(dev->phys, sizeof(dev->phys), "xj380/input%d", index);
    snprintf(dev->uniq, sizeof(dev->uniq), "xj380-event%d", index);
    dev->id.bustype = BUS_VIRTUAL;
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
        for (uint16_t key = KEY_ESC; key <= KEY_0; key++) evdev_set_bit(dev->keybit, key);
        for (uint16_t key = KEY_Q; key <= KEY_P; key++) evdev_set_bit(dev->keybit, key);
        for (uint16_t key = KEY_A; key <= KEY_Z; key++) evdev_set_bit(dev->keybit, key);
        for (uint16_t key = KEY_F1; key <= KEY_F12; key++) evdev_set_bit(dev->keybit, key);
        evdev_set_bit(dev->keybit, KEY_BACKSPACE);
        evdev_set_bit(dev->keybit, KEY_TAB);
        evdev_set_bit(dev->keybit, KEY_ENTER);
        evdev_set_bit(dev->keybit, KEY_SPACE);
        evdev_set_bit(dev->keybit, KEY_RIGHTCTRL);
        evdev_set_bit(dev->keybit, KEY_HOME);
        evdev_set_bit(dev->keybit, KEY_UP);
        evdev_set_bit(dev->keybit, KEY_PAGEUP);
        evdev_set_bit(dev->keybit, KEY_LEFT);
        evdev_set_bit(dev->keybit, KEY_RIGHT);
        evdev_set_bit(dev->keybit, KEY_END);
        evdev_set_bit(dev->keybit, KEY_DOWN);
        evdev_set_bit(dev->keybit, KEY_PAGEDOWN);
        evdev_set_bit(dev->keybit, KEY_INSERT);
        evdev_set_bit(dev->keybit, KEY_DELETE);
    }

    device_t device = {};
    device.type = DEVICE_STREAM;
    device.flag = 1;
    device.sector_size = 1;
    device.size = (size_t)-1;
    device.read = evdev_read;
    device.write = evdev_write;
    device.ioctl = evdev_ioctl;
    device.poll = evdev_poll;
    device.map = (mapf)empty;
    device.ready = evdev_ready;
    snprintf(device.drive_name, sizeof(device.drive_name), "event%d", index);
    device.major = 13;
    device.minor = (uint32_t)(64 + index);
    char devnode[64];
    snprintf(devnode, sizeof(devnode), "input/event%d", index);
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
        evdev_publish_metadata(index);
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
