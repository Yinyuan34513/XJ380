/**
 * Device Virtual File System
 * 设备虚拟文件系统
 */
#define ALL_IMPLEMENTATION
#include "fs/vfs/devfs.h"
#include "device.h"
#include "errno.h"
// #include "kprint.h"
#include "krlibc.h"
// #include "sprintf.h"
#include "fs/vfs/vfs.h"

extern device_t device_ctl[256];

int             devfs_id    = 0;
static int      devfs_inode = 2;
vfs_node_t      devfs_root  = NULL;
device_handle_t root_handle = NULL;

static uint64_t devfs_mkdev(const device_t *device)
{
    if (device == NULL) return 0;
    return ((uint64_t)device->major << 8) | (uint64_t)(device->minor & 0xffU);
}

static size_t devfs_child_count(vfs_node_t node)
{
    return node != NULL ? list_length(node->child) : 0;
}

static void devfs_trace_node(const char *tag, const char *path, vfs_node_t node)
{
    write_serial_fmt("[udev-trace] devfs:%s path=%s node=%p type=0x%x fsid=%u handle=%p rdev=%llu child=%llu ref=%u\n",
                     tag != NULL ? tag : "?",
                     path != NULL ? path : "(null)",
                     node,
                     node != NULL ? node->type : 0,
                     node != NULL ? node->fsid : 0,
                     node != NULL ? node->handle : NULL,
                     node != NULL ? (unsigned long long)node->rdev : 0,
                     (unsigned long long)devfs_child_count(node),
                     node != NULL ? node->refcount : 0);
}

static char *devfs_strdup_root_path()
{
    char *buf = (char *)malloc(5);
    if (buf != NULL) strcpy(buf, "/dev");
    return buf;
}

static char *devfs_parent_path(const char *path)
{
    if (path == NULL || *path == '\0') return devfs_strdup_root_path();

    if (strcmp(path, "/dev") == 0) return devfs_strdup_root_path();

    const char *normalized = path;
    if (strncmp(path, "/dev/", 5) != 0)
    {
        char *buf = (char *)malloc(strlen(path) + 6);
        if (buf == NULL) return NULL;
        sprintf(buf, "/dev/%s", path);
        normalized = buf;
    }

    char *dup = strdup(normalized);
    if (normalized != path) free((void *)normalized);
    if (dup == NULL) return NULL;

    char *slash = strrchr(dup, '/');
    if (slash == NULL || slash == dup)
    {
        free(dup);
        return devfs_strdup_root_path();
    }

    *slash = '\0';
    if (strcmp(dup, "/dev") == 0) return dup;

    if (dup[0] == '\0')
    {
        free(dup);
        return devfs_strdup_root_path();
    }
    return dup;
}

static void devfs_apply_device_node(vfs_node_t node, device_t *device)
{
    if (node == NULL || device == NULL) return;
    node->type = device->type == DEVICE_STREAM ? file_stream :
                 (device->type == DEVICE_FB ? file_fbdev : file_block);
    node->size = device->type == DEVICE_STREAM ? (uint64_t)-1 : disk_size(device->vdiskid);
    node->realsize = device->sector_size;
    node->fsid = devfs_id;
    node->rdev = devfs_mkdev(device);
    node->mode = device->type == DEVICE_BLOCK ? 0660 : 0666;
}

static errno_t devfs_register0(const char *path, size_t id) {
    device_t *device = &device_ctl[id];
    if (device->flag == 0 || device->drive_name[0] == '\0') return EOK;

    write_serial_fmt("[udev-trace] devfs:register request raw_path=%s id=%llu drive=%s major=%u minor=%u\n",
                     path != NULL ? path : "(null)",
                     (unsigned long long)id,
                     device->drive_name,
                     device->major,
                     device->minor);

    char *buf = devfs_parent_path(path);
    if (buf == NULL)
    {
        write_serial_fmt("[udev-trace] devfs:register parent_alloc_failed raw_path=%s\n", path != NULL ? path : "(null)");
        return -ENOMEM;
    }

    write_serial_fmt("[udev-trace] devfs:register parent=%s basename=%s\n", buf, device->drive_name);

    vfs_node_t node = vfs_open(buf);
    if (node == NULL)
    {
        write_serial_fmt("[udev-trace] devfs:register parent_miss mkdir=%s\n", buf);
        vfs_mkdir(buf);
        node = vfs_open(buf);
    }
    if (node == NULL) {
        write_serial_fmt("[udev-trace] devfs:register parent_open_failed parent=%s\n", buf);
        free(buf);
        return ENOENT;
    }
    devfs_trace_node("register-parent", buf, node);

    device_handle_t parent = (device_handle_t)node->handle;
    if (parent == NULL) {
        write_serial_fmt("[udev-trace] devfs:register parent_no_handle parent=%s type=0x%x fsid=%u\n",
                         buf,
                         node->type,
                         node->fsid);
        vfs_close(node);
        free(buf);
        return ENODEV;
    }

    if (!parent->is_dir) {
        write_serial_fmt("[udev-trace] devfs:register parent_not_dir parent=%s handle=%p\n", buf, parent);
        vfs_close(node);
        free(buf);
        return ENOTDIR;
    }

    vfs_node_t existing = vfs_do_search(node, device->drive_name);
    if (existing != NULL)
    {
        devfs_trace_node("register-existing-before", device->drive_name, existing);
        bool new_handle = existing->handle == NULL || existing->fsid != devfs_id;
        if (new_handle) existing->handle = calloc(1, sizeof(struct device_handle));
        device_handle_t existing_handle = (device_handle_t)existing->handle;
        if (existing_handle != NULL)
        {
            existing_handle->is_dir = false;
            existing_handle->device = device;
            if (existing_handle->name == NULL) existing_handle->name = strdup(device->drive_name);
            if (new_handle)
            {
                llist_init_head(&existing_handle->child);
                llist_init_head(&existing_handle->curr);
                llist_append(&parent->child, &existing_handle->curr);
            }
        }
        devfs_apply_device_node(existing, device);
        existing->root = node->root;
        existing->dev  = node->dev;
        devfs_trace_node("register-existing-after", device->drive_name, existing);
        vfs_close(node);
        free(buf);
        write_serial_fmt("devfs: published /dev/%s major=%u minor=%u\n",
                         device->path != NULL ? device->path : device->drive_name,
                         device->major,
                         device->minor);
        return EOK;
    }

    device_handle_t handle = (device_handle_t)calloc(1, sizeof(struct device_handle));
    if (handle == NULL)
    {
        write_serial_fmt("[udev-trace] devfs:register handle_alloc_failed path=%s name=%s\n", buf, device->drive_name);
        vfs_close(node);
        free(buf);
        return -ENOMEM;
    }
    // not_null_assets(handle, "device handle is null.");
    handle->is_dir = false;
    handle->device = device;
    handle->name   = strdup(device->drive_name);
    llist_init_head(&handle->child);
    llist_init_head(&handle->curr);
    vfs_node_t dev_node = vfs_child_append(node, device->drive_name, handle);
    devfs_apply_device_node(dev_node, device);
    if (dev_node != NULL)
    {
        dev_node->root = node->root;
        dev_node->dev  = node->dev;
    }
    llist_append(&parent->child, &handle->curr);
    devfs_trace_node("register-created", device->drive_name, dev_node);
    devfs_trace_node("register-parent-after", buf, node);
    write_serial_fmt("devfs: published /dev/%s major=%u minor=%u\n",
                     device->path != NULL ? device->path : device->drive_name,
                     device->major,
                     device->minor);
    vfs_close(node);
    free(buf);
    return EOK;
}

static errno_t devfs_mount(const char *handle, vfs_node_t node) {
    if ((uint64_t)handle != DEVFS_REGISTER_ID) return VFS_STATUS_FAILED;
    if (devfs_root != NULL && devfs_root != node)
        write_serial_fmt("devfs: remounting /dev on current root\n");
    devfs_trace_node("mount-before", "/dev", node);
    root_handle         =  (device_handle_t)calloc(1, sizeof(struct device_handle));
    if (root_handle == NULL) return -ENOMEM;
    root_handle->device = NULL;
    root_handle->is_dir = true;
    llist_init_head(&root_handle->child);
    llist_init_head(&root_handle->curr);

    node->fsid   = devfs_id;
    node->handle = root_handle;
    node->dev    = 0;
    node->type   = file_dir;
    devfs_root   = node;
    devfs_trace_node("mount-after-root", "/dev", node);

    for (size_t i = 0; i < 256; i++) {
        device_t *device = &device_ctl[i];
        if (device->flag == 0 || device->drive_name[0] == '\0') continue;
        // write_serial_fmt("mount dev name: %s\n", device->drive_name);
        devfs_register0(device->path, i);
    }

    return VFS_STATUS_SUCCESS;
}

static errno_t devfs_mkdir(void *handle, const char *name, vfs_node_t node) {
    if (handle == NULL) return VFS_STATUS_FAILED;
    device_handle_t dev_t =  (device_handle_t)handle;
    device_handle_t nw   =  (device_handle_t)calloc(1, sizeof(struct device_handle));
    if (nw == NULL)
    {
        write_serial_fmt("[udev-trace] devfs:mkdir alloc_failed name=%s parent_handle=%p\n",
                         name != NULL ? name : "(null)",
                         handle);
        return -ENOMEM;
    }
    nw->name             = strdup(name);
    nw->is_dir           = true;
    nw->device           = NULL;
    llist_init_head(&nw->child);
    llist_init_head(&nw->curr);

    llist_append(&dev_t->child, &nw->curr);
    node->handle = nw;
    node->fsid   = devfs_id;
    devfs_trace_node("mkdir", name, node);
    return EOK;
}

static errno_t devfs_stat(void *handle, vfs_node_t node) {
    if (node->type == file_dir)
    {
        devfs_trace_node("stat-dir", node != NULL ? node->name : "(null)", node);
        return VFS_STATUS_SUCCESS;
    }
    if (handle == NULL)
    {
        devfs_trace_node("stat-no-handle", node != NULL ? node->name : "(null)", node);
        return VFS_STATUS_FAILED;
    }
    device_handle_t dev_t =  (device_handle_t)handle;

    node->handle = dev_t;
    node->type   = dev_t->device->type == DEVICE_STREAM ? file_stream :
                   (dev_t->device->type == DEVICE_FB ? file_fbdev : file_block);
    node->size =
        dev_t->device->type == DEVICE_STREAM ? (uint64_t)-1 : disk_size(dev_t->device->vdiskid);
    node->realsize = dev_t->device->sector_size;
    node->fsid     = devfs_id;
    node->rdev     = devfs_mkdev(dev_t->device);
    write_serial_fmt("[udev-trace] devfs:stat name=%s type=0x%x major=%u minor=%u rdev=%llu handle=%p\n",
                     dev_t->name != NULL ? dev_t->name : "(null)",
                     node->type,
                     dev_t->device != NULL ? dev_t->device->major : 0,
                     dev_t->device != NULL ? dev_t->device->minor : 0,
                     (unsigned long long)node->rdev,
                     handle);
    return VFS_STATUS_SUCCESS;
}

static void devfs_open(void *parent, const char *name, vfs_node_t node) {
    if (node == devfs_root) {
        node->handle = root_handle;
        node->type   = file_dir;
        devfs_trace_node("open-root", name, node);
        return;
    }
    if (node->type == file_dir)
    {
        devfs_trace_node("open-dir", name, node);
        return;
    }
    if (parent == NULL)
    {
        devfs_trace_node("open-no-parent", name, node);
        return;
    }

    device_handle_t dir_t =  (device_handle_t)parent;
    device_handle_t pos, nxt;

    device_handle_t dev_t = NULL;
    llist_for_each(pos, nxt, &dir_t->child, curr) {
        if (strcmp(pos->name, name) == 0) { dev_t = pos; }
    }
    if (dev_t == NULL) {
        node->handle = NULL;
        write_serial_fmt("[udev-trace] devfs:open miss name=%s parent_handle=%p node=%p child=%llu\n",
                         name != NULL ? name : "(null)",
                         parent,
                         node,
                         (unsigned long long)devfs_child_count(node != NULL ? node->parent : NULL));
        return;
    }

    node->handle = dev_t;
    node->type   = dev_t->device->type == DEVICE_STREAM ? file_stream :
                   (dev_t->device->type == DEVICE_FB ? file_fbdev : file_block);
    node->size =
        dev_t->device->type == DEVICE_STREAM ? (uint64_t)-1 : disk_size(dev_t->device->vdiskid);
    node->fsid     = devfs_id;
    node->rdev     = devfs_mkdev(dev_t->device);
    node->refcount = 1;
    write_serial_fmt("[udev-trace] devfs:open hit name=%s node=%p type=0x%x handle=%p major=%u minor=%u rdev=%llu\n",
                     name != NULL ? name : "(null)",
                     node,
                     node->type,
                     node->handle,
                     dev_t->device != NULL ? dev_t->device->major : 0,
                     dev_t->device != NULL ? dev_t->device->minor : 0,
                     (unsigned long long)node->rdev);
}

spin_t RD_lk =SPIN_INIT;
size_t devfs_read(void *file, void *addr, size_t offset, size_t size) {
    if (file == NULL || addr == NULL) return VFS_STATUS_FAILED;
    if (size == 0) return 0;

    spin_lock(&RD_lk);
    device_handle_t handle =  (device_handle_t)file;
    if (handle->is_dir) 
    {
        spin_unlock(&RD_lk);
        return VFS_STATUS_FAILED;
    }
    device_t *deivce = handle->device;
    if (deivce == NULL || deivce->flag == 0 || deivce->read == NULL)
    {
        spin_unlock(&RD_lk);
        return VFS_STATUS_FAILED;
    }
    device_t device = *deivce;
    spin_unlock(&RD_lk);

    if (device.type == DEVICE_STREAM) {
        size_t bounce_size = size == 0 ? 1 : size;
        uint8_t *bounce = (uint8_t *)malloc(bounce_size);
        if (bounce == NULL) {
            return VFS_STATUS_FAILED;
        }
        size_t got = device.read(device.vdiskid, bounce, size, offset);
        if ((int64_t)got < 0) {
            free(bounce);
            return got;
        }
        if (got > size) {
            write_serial_fmt("[udev-trace] devfs:read clamp name=%s requested=%llu got=%llu major=%u minor=%u\n",
                             handle->name != NULL ? handle->name : "(null)",
                             (unsigned long long)size,
                             (unsigned long long)got,
                             device.major,
                             device.minor);
            got = size;
        }
        if (got != 0) {
            memcpy(addr, bounce, got);
        }
        free(bounce);
        return got;
    }
    if (offset > device.size)
    {
        return VFS_STATUS_SUCCESS;
    }
    if (size > device.size - offset) {
        size = device.size - offset;
    }
    return blk_device_read(device.vdiskid, addr, offset, size);
}
spin_t WR_lk =SPIN_INIT;
size_t devfs_write(void *file, const void *addr, size_t offset, size_t size) {
    spin_lock(&WR_lk);
    device_handle_t handle =  (device_handle_t)file;
    if (handle->is_dir)
    {
        spin_unlock(&WR_lk);
        return VFS_STATUS_FAILED;
    }
    device_t *deivce = handle->device;
    if (deivce->flag == 0)
    {
        spin_unlock(&WR_lk);
        return VFS_STATUS_FAILED;
    }
    device_t device = *deivce;
    spin_unlock(&WR_lk);

    if (device.type == DEVICE_STREAM) {
        return device.write(device.vdiskid, (uint8_t *)addr, size, offset);
    }
    if (offset > device.size)
    {
        return VFS_STATUS_SUCCESS;
    }
    if (size > device.size - offset) {
        size = device.size - offset;
    }
    return blk_device_write(device, addr, offset, size);
}

static errno_t devfs_ioctl(void *file, size_t req, void *arg) {
    device_handle_t handle =  (device_handle_t)file;
    if (handle == NULL || handle->device == NULL)
    {
        write_serial_fmt("[udev-trace] devfs:ioctl no_device req=0x%llx handle=%p\n",
                         (unsigned long long)req,
                         handle);
        return VFS_STATUS_FAILED;
    }
    if (handle->device->flag == 0) return VFS_STATUS_FAILED;
    int ret = handle->device->ioctl(handle->device, req, arg);
    write_serial_fmt("[udev-trace] devfs:ioctl name=%s req=0x%llx ret=%d major=%u minor=%u\n",
                     handle->name != NULL ? handle->name : "(null)",
                     (unsigned long long)req,
                     ret,
                     handle->device->major,
                     handle->device->minor);
    return ret;
}

static vfs_node_t devfs_dup(vfs_node_t node) {
    vfs_node_t new_node = vfs_node_alloc(node->parent, node->name);
    if (new_node == NULL) return NULL;
    new_node->type        = node->type;
    new_node->handle      = node->handle;
    new_node->size        = node->size;
    new_node->child       = node->child;
    new_node->flags       = node->flags;
    new_node->permissions = node->permissions;
    new_node->realsize    = node->realsize;
    return new_node;
}

static errno_t devfs_poll(void *file, size_t events) {
    device_handle_t handle =  (device_handle_t)file;
    if (handle->device->flag == 0) return VFS_STATUS_FAILED;
    if (handle->device->poll != NULL && handle->device->poll != (void *)empty) {
        return handle->device->poll(events);
    }
    return VFS_STATUS_SUCCESS;
}

static void *devfs_map(void *file, void *addr, size_t offset, size_t size, size_t prot,
                       size_t flags) {
    device_handle_t handle =  (device_handle_t)file;
    if (handle->device->flag == 0) return NULL;
    if (handle->device->map) { return handle->device->map(handle->device->vdiskid, addr, size, offset); }
    return NULL;
}

static int dummy() {
    return -ENOSYS;
}

static struct vfs_callback devfs_callbacks = {
    .mount    = devfs_mount,
    .unmount  = (vfs_unmount_t)empty,
    .mkdir    = devfs_mkdir,
    .close    = (vfs_close_t)empty,
    .stat     = devfs_stat,
    .open     = devfs_open,
    .read     = devfs_read,
    .write    = devfs_write,
    .readlink = (vfs_readlink_t)dummy,
    .mkfile   = (vfs_mk_t)empty,
    .link     = (vfs_mk_t)dummy,
    .symlink  = (vfs_mk_t)dummy,
    .ioctl    = devfs_ioctl,
    .dup      = devfs_dup,
    .resize   = (vfs_resize_t)dummy,
    .del   = (vfs_del_t)empty,
    .rename   = (vfs_rename_t)empty,
    .poll     = devfs_poll,
    .map      = devfs_map,
};

void devfs_setup() {
    if (devfs_id == 0) devfs_id = vfs_regist("devfs", &devfs_callbacks, DEVFS_REGISTER_ID, 0x454d444d);
    write_serial_fmt("[udev-trace] devfs:setup begin id=%d root=%p\n", devfs_id, devfs_root);
    vfs_mkdir("/dev");
    vfs_node_t dev = vfs_open("/dev");
    if (dev == NULL) {
        write_serial_fmt("'dev' handle is null.");
        return;
    }
    if (vfs_mount((char*)DEVFS_REGISTER_ID, dev) == VFS_STATUS_FAILED) {
        write_serial_fmt("Cannot mount device file system.");
        return;
    }
    vfs_mkdir("/dev/ptx");
    devfs_trace_node("setup-done", "/dev", dev);
    vfs_close(dev);
}

errno_t devfs_delete(const char *path) {
    char *buf = NULL;
    if (path == NULL || *path == '\0')
    {
        buf = devfs_strdup_root_path();
    }
    else if (strncmp(path, "/dev/", 5) == 0)
    {
        buf = strdup(path);
    }
    else
    {
        buf = (char*)malloc(strlen(path) + 6);
        if (buf != NULL) sprintf(buf, "/dev/%s", path);
    }
    if (buf == NULL) return -ENOMEM;

    vfs_node_t node = vfs_open(buf);
    if (node == NULL) {
        write_serial_fmt("[udev-trace] devfs:delete miss path=%s\n", buf);
        free(buf);
        return -ENOENT;
    }
    devfs_trace_node("delete", buf, node);
    vfs_detach(node);
    vfs_free(node);
    free(buf);
    return EOK;
}

errno_t devfs_register(const char *path, size_t id) {
    device_t *device = &device_ctl[id];
    free(device->path);
    device->path = strdup(path != NULL && path[0] != '\0' ? path : device->drive_name);
    write_serial_fmt("[udev-trace] devfs:register entry path=%s id=%llu root=%p\n",
                     device->path != NULL ? device->path : "(null)",
                     (unsigned long long)id,
                     devfs_root);
    if (devfs_root == NULL) { return EOK; }
    return devfs_register0(device->path, id);
}
