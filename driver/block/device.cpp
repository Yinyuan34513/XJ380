#include "../../include/include.h"

static device_t devices[DEVICE_NR]; // 设备数组

// 获取空设备
static device_t *get_null_device()
{
    for (size_t i = 1; i < DEVICE_NR; i++)
    {
        device_t *device = &devices[i];
        if (device->type == DEV_NULL)
            return device;
    }
    return nullptr;
}

int device_ioctl(int dev, int cmd, void *args, int flags)
{
    device_t *device = device_get(dev);
    if (device->ioctl)
    {
        return device->ioctl(device->ptr, cmd, args, flags);
    }
    return -1;
}

int device_read(int dev, void *buf, size_t count, int idx, int flags)
{
    device_t *device = device_get(dev);
    if (device->read)
    {
        return device->read(device->ptr, buf, count, idx, flags);
    }
    return -1;
}

int device_write(int dev, void *buf, size_t count, int idx, int flags)
{
    device_t *device = device_get(dev);
    if (device->write)
    {
        return device->write(device->ptr, buf, count, idx, flags);
    }
    return -1;
}

// 安装设备
int device_install(
    int type, int subtype,
    void *ptr, char *name, int parent,
    void *ioctl, void *read, void *write)
{
    device_t *device = get_null_device();
    device->ptr = ptr;
    device->parent = parent;
    device->type = type;
    device->subtype = subtype;
    strncpy(device->name, name, NAMELEN);
    device->ioctl = (ioctl_t)ioctl;
    device->read = (read_t)read;
    device->write = (write_t)write;
    return device->dev;
}

void init_device()
{
    for (size_t i = 0; i < DEVICE_NR; i++)
    {
        device_t *device = &devices[i];
        strcpy((char *)device->name, "null");
        device->type = DEV_NULL;
        device->subtype = DEV_NULL;
        device->dev = i;
        device->parent = 0;
        device->ioctl = nullptr;
        device->read = nullptr;
        device->write = nullptr;

        device->direct = DIRECT_UP;
    }
}

device_t *device_find(int subtype, int idx)
{
    int nr = 0;
    for (size_t i = 0; i < DEVICE_NR; i++)
    {
        device_t *device = &devices[i];
        if (device->subtype != subtype)
            continue;
        if (nr == idx)
            return device;
        nr++;
    }
    return nullptr;
}

device_t *device_get(int dev)
{
    device_t *device = &devices[dev];
    return device;
}
