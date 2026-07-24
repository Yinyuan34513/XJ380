#define ALL_IMPLEMENTATION

#include <errno.h>
#include <fs/vfs/anonfs.h>
#include <fs/vfs/sys.h>
#include <device.h>
#include <krlibc.h>
#include <net/socket.h>
#include <proto.hpp>
#include <task/poll.h>

static int        sysfs_id   = 0;
static vfs_node_t sysfs_root = NULL;
static spin_t     sysfs_lock = SPIN_INIT;
static int        sysfs_seqnum = 1;

static size_t sysfs_child_count(vfs_node_t node)
{
    return node != NULL ? list_length(node->child) : 0;
}

static void sysfs_trace_node(const char *tag, const char *path, vfs_node_t node)
{
    write_serial_fmt("[udev-trace] sysfs:%s path=%s node=%p type=0x%x fsid=%u handle=%p size=%llu child=%llu ref=%u link=%s\n",
                     tag != NULL ? tag : "?",
                     path != NULL ? path : "(null)",
                     node,
                     node != NULL ? node->type : 0,
                     node != NULL ? node->fsid : 0,
                     node != NULL ? node->handle : NULL,
                     node != NULL ? (unsigned long long)node->size : 0,
                     (unsigned long long)sysfs_child_count(node),
                     node != NULL ? node->refcount : 0,
                     node != NULL && node->linkname != NULL ? node->linkname : "");
}

static int sysfs_dummy()
{
    return -ENOSYS;
}

static sysfs_handle_t *sysfs_alloc_handle(vfs_node_t node)
{
    sysfs_handle_t *handle = (sysfs_handle_t *)calloc(1, sizeof(sysfs_handle_t));
    if (handle == NULL) return NULL;
    handle->node = node;
    if (node != NULL && node->name != NULL) strncpy(handle->name, node->name, sizeof(handle->name) - 1);
    return handle;
}

static void sysfs_apply_stat(vfs_node_t node, sysfs_handle_t *handle)
{
    if (node == NULL) return;
    node->fsid = sysfs_id;
    node->mode = node->mode == 0 ? 0755 : node->mode;
    if (node->type & file_dir) node->size = 0;
    else if (node->type & file_symlink) node->size = node->linkname ? strlen(node->linkname) : 0;
    else node->size = handle != NULL ? handle->size : 0;
}

static bool sysfs_node_is_uevent(sysfs_handle_t *handle)
{
    return handle != NULL && strcmp(handle->name, "uevent") == 0;
}

static errno_t sysfs_mount(const char *src, vfs_node_t node)
{
    if ((uint64_t)src != TMPFS_REGISTER_ID + 2000) return VFS_STATUS_FAILED;
    sysfs_trace_node("mount-before", "/sys", node);
    sysfs_root = node;
    node->type = file_dir;
    node->mode = 0755;
    node->fsid = sysfs_id;
    node->handle = sysfs_alloc_handle(node);
    sysfs_trace_node("mount-after", "/sys", node);
    return node->handle != NULL ? EOK : -ENOMEM;
}

static void sysfs_open(void *parent, const char *name, vfs_node_t node)
{
    (void)parent;
    (void)name;
    if (node != NULL && node->handle == NULL && !(node->type & file_symlink))
        node->handle = sysfs_alloc_handle(node);
    sysfs_apply_stat(node, (sysfs_handle_t *)node->handle);
    if (name != NULL && (strncmp(name, "card", 4) == 0 || strncmp(name, "event", 5) == 0 ||
                         strcmp(name, "uevent") == 0 || strcmp(name, "dev") == 0))
    {
        sysfs_trace_node("open", name, node);
    }
}

static errno_t sysfs_stat(void *file, vfs_node_t node)
{
    sysfs_apply_stat(node, (sysfs_handle_t *)file);
    sysfs_handle_t *handle = (sysfs_handle_t *)file;
    if (handle != NULL && (strcmp(handle->name, "uevent") == 0 || strcmp(handle->name, "dev") == 0 ||
                           strncmp(handle->name, "card", 4) == 0 || strncmp(handle->name, "event", 5) == 0))
    {
        sysfs_trace_node("stat", handle->name, node);
    }
    return EOK;
}

static size_t sysfs_read(void *file, void *addr, size_t offset, size_t size)
{
    sysfs_handle_t *handle = (sysfs_handle_t *)file;
    if (handle == NULL || handle->content == NULL)
    {
        write_serial_fmt("[udev-trace] sysfs:read-empty handle=%p offset=%llu size=%llu\n",
                         handle,
                         (unsigned long long)offset,
                         (unsigned long long)size);
        return 0;
    }
    if (offset >= handle->size)
    {
        return 0;
    }
    size_t copy = MIN(size, handle->size - offset);
    memcpy(addr, handle->content + offset, copy);
    return copy;
}

static bool sysfs_reserve(sysfs_handle_t *handle, size_t need)
{
    if (handle == NULL) return false;
    if (need <= handle->capacity) return true;
    size_t new_capacity = handle->capacity == 0 ? 256 : handle->capacity;
    while (new_capacity < need) new_capacity *= 2;
    char *new_content = (char *)calloc(1, new_capacity);
    if (new_content == NULL) return false;
    if (handle->content != NULL && handle->size != 0) memcpy(new_content, handle->content, handle->size);
    free(handle->content);
    handle->content = new_content;
    handle->capacity = new_capacity;
    return true;
}

static size_t sysfs_write_storage(sysfs_handle_t *handle, const void *addr, size_t offset, size_t size)
{
    if (handle == NULL || offset > (size_t)-1 - size) return VFS_STATUS_FAILED;
    if (!sysfs_reserve(handle, offset + size + 1)) return VFS_STATUS_FAILED;
    memcpy(handle->content + offset, addr, size);
    handle->size = MAX(handle->size, offset + size);
    handle->content[handle->size] = '\0';
    if (handle->node != NULL) handle->node->size = handle->size;
    return size;
}

static bool sysfs_uevent_write_requests_add(const void *addr, size_t size)
{
    const char *command = (const char *)addr;
    if (command == NULL || size == 0) return false;

    while (size > 0 && (*command == ' ' || *command == '\t' || *command == '\n'))
    {
        command++;
        size--;
    }
    return size >= 3 && command[0] == 'a' && command[1] == 'd' && command[2] == 'd' &&
           (size == 3 || command[3] == '\0' || command[3] == '\n' || command[3] == ' ' ||
            command[3] == '\t');
}

static size_t sysfs_uevent_write(sysfs_handle_t *handle, const void *addr, size_t size)
{
    if (handle == NULL || handle->node == NULL || handle->node->parent == NULL) return VFS_STATUS_FAILED;
    if (!sysfs_uevent_write_requests_add(addr, size)) return size;

    char *parent_path = vfs_get_fullpath(handle->node->parent);
    if (parent_path == NULL) return VFS_STATUS_FAILED;
    const char *devpath = strncmp(parent_path, "/sys", 4) == 0 ? parent_path + 4 : parent_path;
    const char *content = handle->content != NULL ? handle->content : "";
    size_t event_capacity = strlen(devpath) + strlen(content) + 96;
    char *event = (char *)malloc(event_capacity);
    if (event == NULL)
    {
        free(parent_path);
        return VFS_STATUS_FAILED;
    }

    int current_seq = __atomic_fetch_add(&sysfs_seqnum, 1, __ATOMIC_SEQ_CST);
    snprintf(event,
             event_capacity,
             "add@%s\nACTION=add\nSEQNUM=%d\n%s%s",
             devpath,
             current_seq,
             content,
             content[0] != '\0' && content[strlen(content) - 1] == '\n' ? "" : "\n");

    size_t source_len = strlen(event);
    size_t event_len = 0;
    bool last_was_nul = false;
    for (size_t i = 0; i < source_len; i++)
    {
        char value = event[i] == '\n' ? '\0' : event[i];
        if (value == '\0')
        {
            if (last_was_nul) continue;
            last_was_nul = true;
        }
        else
        {
            last_was_nul = false;
        }
        event[event_len++] = value;
    }
    if (event_len == 0 || event[event_len - 1] != '\0') event[event_len++] = '\0';

    netlink_kernel_uevent_send(event, (int)event_len);
    free(event);
    free(parent_path);
    return size;
}

static size_t sysfs_write(void *file, const void *addr, size_t offset, size_t size)
{
    sysfs_handle_t *handle = (sysfs_handle_t *)file;
    if (handle == NULL || offset > (size_t)-1 - size) return VFS_STATUS_FAILED;

    write_serial_fmt("[udev-trace] sysfs:write name=%s offset=%llu size=%llu is_uevent=%d\n",
                     handle->name,
                     (unsigned long long)offset,
                     (unsigned long long)size,
                     sysfs_node_is_uevent(handle) ? 1 : 0);

    if (sysfs_node_is_uevent(handle)) return sysfs_uevent_write(handle, addr, size);
    return sysfs_write_storage(handle, addr, offset, size);
}

static size_t sysfs_readlink(vfs_node_t node, void *addr, size_t offset, size_t size)
{
    if (node == NULL || node->linkname == NULL)
    {
        sysfs_trace_node("readlink-miss", node != NULL ? node->name : "(null)", node);
        return VFS_STATUS_FAILED;
    }
    size_t len = strlen(node->linkname);
    if (offset >= len) return 0;
    size_t copy = MIN(size, len - offset);
    memcpy(addr, node->linkname + offset, copy);
    write_serial_fmt("[udev-trace] sysfs:readlink name=%s target=%s offset=%llu req=%llu ret=%llu\n",
                     node->name != NULL ? node->name : "(null)",
                     node->linkname,
                     (unsigned long long)offset,
                     (unsigned long long)size,
                     (unsigned long long)copy);
    return copy;
}

static errno_t sysfs_mkdir(void *parent, const char *name, vfs_node_t node)
{
    (void)parent;
    if (node == NULL) return -EINVAL;
    node->type = file_dir;
    node->mode = 0755;
    node->fsid = sysfs_id;
    node->handle = sysfs_alloc_handle(node);
    sysfs_trace_node("mkdir", name, node);
    return node->handle != NULL ? EOK : -ENOMEM;
}

static errno_t sysfs_mkfile(void *parent, const char *name, vfs_node_t node)
{
    (void)parent;
    if (node == NULL) return -EINVAL;
    node->type = file_none | file_proxy;
    node->mode = 0644;
    node->fsid = sysfs_id;
    node->handle = sysfs_alloc_handle(node);
    sysfs_trace_node("mkfile", name, node);
    return node->handle != NULL ? EOK : -ENOMEM;
}

static errno_t sysfs_symlink(void *parent, const char *target, vfs_node_t node)
{
    (void)parent;
    if (node == NULL || target == NULL) return -EINVAL;
    node->type = file_symlink | file_proxy;
    node->mode = 0777;
    node->fsid = sysfs_id;
    node->linkname = strdup(target);
    node->size = node->linkname != NULL ? strlen(node->linkname) : 0;
    sysfs_trace_node("symlink", target, node);
    return node->linkname != NULL ? EOK : -ENOMEM;
}

static void sysfs_close(void *file)
{
    (void)file;
}

static errno_t sysfs_del(void *parent, vfs_node_t node)
{
    (void)parent;
    if (node == NULL) return EOK;
    sysfs_handle_t *handle = (sysfs_handle_t *)node->handle;
    if (handle != NULL)
    {
        free(handle->content);
        free(handle);
        node->handle = NULL;
    }
    return EOK;
}

static errno_t sysfs_poll(void *file, size_t events)
{
    (void)file;
    return events & (EPOLLIN | EPOLLOUT);
}

static struct vfs_callback sysfs_callbacks = {
    .mount    = sysfs_mount,
    .unmount  = (vfs_unmount_t)empty,
    .open     = sysfs_open,
    .close    = sysfs_close,
    .read     = sysfs_read,
    .write    = sysfs_write,
    .readlink = sysfs_readlink,
    .mkdir    = sysfs_mkdir,
    .mkfile   = sysfs_mkfile,
    .link     = (vfs_mk_t)sysfs_dummy,
    .symlink  = sysfs_symlink,
    .stat     = sysfs_stat,
    .ioctl    = (vfs_ioctl_t)sysfs_dummy,
    .dup      = (vfs_dup_t)empty,
    .poll     = sysfs_poll,
    .map      = (vfs_mapfile_t)empty,
    .resize   = (vfs_resize_t)empty,
    .del      = sysfs_del,
    .rename   = (vfs_rename_t)empty,
};

void sysfs_init()
{
    if (sysfs_id == 0)
        sysfs_id = vfs_regist("sysfs", &sysfs_callbacks, TMPFS_REGISTER_ID + 2000, 0x62656566737973ULL);
    if (sysfs_id <= 0) return;
    write_serial_fmt("[udev-trace] sysfs:init begin id=%d root=%p\n", sysfs_id, sysfs_root);
    vfs_node_t old_root = vfs_open("/sys");
    if (old_root == NULL) vfs_mkdir("/sys");
    else vfs_close(old_root);
    vfs_node_t root = vfs_open("/sys");
    if (root == NULL) return;
    if (!root->is_mount || root->fsid != sysfs_id || root != sysfs_root)
    {
        vfs_mount((const char *)(uintptr_t)(TMPFS_REGISTER_ID + 2000), root);
        write_serial_string("sysfs: mounted /sys on current root\n");
    }
    sysfs_trace_node("init-root", "/sys", root);
    vfs_close(root);

    sysfs_ensure_dir("/sys/class");
    sysfs_ensure_dir("/sys/class/drm");
    sysfs_ensure_dir("/sys/class/graphics");
    sysfs_ensure_dir("/sys/class/input");
    sysfs_ensure_dir("/sys/class/misc");
    sysfs_ensure_dir("/sys/class/net");
    sysfs_ensure_dir("/sys/dev");
    sysfs_ensure_dir("/sys/dev/char");
    sysfs_ensure_dir("/sys/dev/block");
    sysfs_ensure_dir("/sys/devices");
    sysfs_ensure_dir("/sys/devices/platform");
    sysfs_ensure_dir("/sys/devices/platform/i8042");
    sysfs_ensure_dir("/sys/devices/usb");
    sysfs_ensure_dir("/sys/devices/virtual");
    sysfs_ensure_dir("/sys/devices/virtual/drm");
    sysfs_ensure_dir("/sys/devices/virtual/graphics");
    sysfs_ensure_dir("/sys/devices/virtual/input");
    sysfs_ensure_dir("/sys/devices/virtual/misc");
    sysfs_ensure_dir("/sys/bus");
    sysfs_ensure_dir("/sys/bus/pci");
    sysfs_ensure_dir("/sys/bus/pci/devices");
    sysfs_ensure_dir("/sys/bus/pci/drivers");
    sysfs_ensure_dir("/sys/bus/usb");
    sysfs_ensure_dir("/sys/bus/usb/devices");
    sysfs_ensure_dir("/sys/bus/usb/drivers");
    sysfs_ensure_dir("/sys/fs");
    sysfs_ensure_dir("/sys/fs/cgroup");
    sysfs_ensure_dir("/sys/fs/fuse");
    sysfs_ensure_dir("/sys/fs/fuse/connections");
    sysfs_ensure_dir("/sys/kernel");
    sysfs_ensure_dir("/sys/kernel/debug");
    sysfs_ensure_dir("/sys/kernel/tracing");
    sysfs_ensure_dir("/sys/module");
    write_serial_fmt("[udev-trace] sysfs:init done root=%p\n", sysfs_root);
}

bool sysfs_is_ready()
{
    return sysfs_id != 0 && sysfs_root != NULL;
}

static vfs_node_t sysfs_ensure_path(const char *path, bool is_dir, const char *target)
{
    if (path == NULL || strncmp(path, "/sys", 4) != 0) return NULL;
    if (sysfs_id == 0) sysfs_init();

    vfs_node_t existing = vfs_open_no_follow(path);
    if (existing != NULL)
    {
        if (strstr(path, "/drm") != NULL || strstr(path, "/input") != NULL || strstr(path, "/dev/char") != NULL ||
            strstr(path, "/uevent") != NULL)
            sysfs_trace_node("ensure-existing", path, existing);
        return existing;
    }

    char *copy = strdup(path);
    if (copy == NULL) return NULL;
    char *last = strrchr(copy, '/');
    if (last == NULL || last == copy)
    {
        free(copy);
        return NULL;
    }
    *last = '\0';
    char *name = last + 1;
    vfs_node_t parent = sysfs_ensure_dir(copy);
    if (parent == NULL)
    {
        free(copy);
        return NULL;
    }

    errno_t ret = is_dir ? vfs_mkdir(path) : (target != NULL ? vfs_symlink(path, target) : vfs_mkfile(path));
    vfs_close(parent);
    free(copy);
    if (ret != EOK)
    {
        write_serial_fmt("[udev-trace] sysfs:ensure create_failed path=%s dir=%d target=%s ret=%d\n",
                         path,
                         is_dir ? 1 : 0,
                         target != NULL ? target : "",
                         ret);
        return NULL;
    }
    vfs_node_t created = vfs_open_no_follow(path);
    sysfs_trace_node("ensure-created", path, created);
    return created;
}

static bool sysfs_is_absolute_sys_path(const char *path)
{
    return path != NULL && strncmp(path, "/sys/", 5) == 0;
}

static const char *sysfs_next_component(const char *path, char *component, size_t component_size)
{
    if (path == NULL || component == NULL || component_size == 0) return NULL;
    while (*path == '/') path++;
    if (*path == '\0') return NULL;

    size_t len = 0;
    while (path[len] != '\0' && path[len] != '/') len++;
    size_t copy = MIN(len, component_size - 1);
    memcpy(component, path, copy);
    component[copy] = '\0';
    return path + len;
}

static size_t sysfs_common_prefix_len(const char *from, const char *to)
{
    size_t common = 0;
    const char *from_pos = from;
    const char *to_pos = to;

    while (true)
    {
        char from_part[64];
        char to_part[64];
        const char *from_next = sysfs_next_component(from_pos, from_part, sizeof(from_part));
        const char *to_next = sysfs_next_component(to_pos, to_part, sizeof(to_part));
        if (from_next == NULL || to_next == NULL) break;
        if (strcmp(from_part, to_part) != 0) break;

        common++;
        from_pos = from_next;
        to_pos = to_next;
    }

    return common;
}

static size_t sysfs_component_count_after(const char *path, size_t skip)
{
    size_t count = 0;
    const char *pos = path;

    while (true)
    {
        char part[64];
        const char *next = sysfs_next_component(pos, part, sizeof(part));
        if (next == NULL) break;
        if (skip == 0) count++;
        else skip--;
        pos = next;
    }

    return count;
}

static const char *sysfs_path_after_components(const char *path, size_t skip)
{
    const char *pos = path;

    while (skip > 0)
    {
        char part[64];
        const char *next = sysfs_next_component(pos, part, sizeof(part));
        if (next == NULL) return "";
        pos = next;
        skip--;
    }

    while (*pos == '/') pos++;
    return pos;
}

static void sysfs_relative_symlink_target(const char *link_path, const char *target_path, char *out, size_t out_size)
{
    if (out == NULL || out_size == 0) return;
    out[0] = '\0';

    if (!sysfs_is_absolute_sys_path(link_path) || !sysfs_is_absolute_sys_path(target_path))
    {
        snprintf(out, out_size, "%s", target_path != NULL ? target_path : "");
        return;
    }

    char link_parent[256];
    snprintf(link_parent, sizeof(link_parent), "%s", link_path);
    char *last_slash = strrchr(link_parent, '/');
    if (last_slash == NULL || last_slash == link_parent)
    {
        snprintf(out, out_size, "%s", target_path);
        return;
    }
    *last_slash = '\0';

    const char *from = link_parent + 5;
    const char *to = target_path + 5;
    size_t common = sysfs_common_prefix_len(from, to);
    size_t up = sysfs_component_count_after(from, common);
    const char *down = sysfs_path_after_components(to, common);

    size_t pos = 0;
    for (size_t i = 0; i < up; i++)
    {
        if (pos + 3 >= out_size) goto fallback;
        out[pos++] = '.';
        out[pos++] = '.';
        out[pos++] = '/';
    }

    if (down[0] != '\0')
    {
        size_t down_len = strlen(down);
        if (pos + down_len >= out_size) goto fallback;
        memcpy(out + pos, down, down_len);
        pos += down_len;
        out[pos] = '\0';
    }
    else if (pos != 0)
    {
        out[pos - 1] = '\0';
    }
    else
    {
        snprintf(out, out_size, ".");
    }
    return;

fallback:
    snprintf(out, out_size, "%s", target_path);
}

vfs_node_t sysfs_ensure_dir(const char *path)
{
    return sysfs_ensure_path(path, true, NULL);
}

vfs_node_t sysfs_ensure_file(const char *path)
{
    return sysfs_ensure_path(path, false, NULL);
}

vfs_node_t sysfs_ensure_symlink(const char *path, const char *target)
{
    char relative_target[256];
    const char *link_target = target;
    if (target != NULL)
    {
        sysfs_relative_symlink_target(path, target, relative_target, sizeof(relative_target));
        link_target = relative_target;
    }

    vfs_node_t node = sysfs_ensure_path(path, false, link_target);
    if (node == NULL || link_target == NULL) return node;
    if ((node->type & file_symlink) && (node->linkname == NULL || strcmp(node->linkname, link_target) != 0))
    {
        if (node->linkto != NULL && node->linkto != node)
        {
            vfs_close(node->linkto);
            node->linkto = NULL;
        }
        free(node->linkname);
        node->linkname = strdup(link_target);
        node->size = node->linkname != NULL ? strlen(node->linkname) : 0;
    }
    if (sysfs_is_absolute_sys_path(path) && sysfs_is_absolute_sys_path(target))
        write_serial_fmt("[udev-trace] sysfs:symlink-rel link=%s target=%s stored=%s\n",
                         path,
                         target,
                         link_target);
    return node;
}

vfs_node_t sysfs_child_append(vfs_node_t parent, const char *name, bool is_dir)
{
    if (parent == NULL || name == NULL) return NULL;
    char *parent_path = vfs_get_fullpath(parent);
    if (parent_path == NULL) return NULL;
    char path[256];
    snprintf(path, sizeof(path), "%s%s%s", parent_path, strcmp(parent_path, "/") == 0 ? "" : "/", name);
    free(parent_path);
    return is_dir ? sysfs_ensure_dir(path) : sysfs_ensure_file(path);
}

vfs_node_t sysfs_child_append_symlink(vfs_node_t parent, const char *name, const char *target)
{
    if (parent == NULL || name == NULL || target == NULL) return NULL;
    char *parent_path = vfs_get_fullpath(parent);
    if (parent_path == NULL) return NULL;
    char path[256];
    snprintf(path, sizeof(path), "%s%s%s", parent_path, strcmp(parent_path, "/") == 0 ? "" : "/", name);
    free(parent_path);
    return sysfs_ensure_symlink(path, target);
}

int sysfs_write_node(vfs_node_t node, const void *buf, size_t len, size_t offset)
{
    if (node == NULL || buf == NULL) return -EINVAL;
    size_t ret = sysfs_write_storage((sysfs_handle_t *)node->handle, buf, offset, len);
    return ret == len ? 0 : -EIO;
}

char *sysfs_node_path(vfs_node_t node)
{
    return vfs_get_fullpath(node);
}

static const char *sysfs_basename(const char *path)
{
    if (path == NULL) return "";
    const char *slash = strrchr(path, '/');
    return slash != NULL ? slash + 1 : path;
}

static const char *sysfs_devpath_from_path(const char *path)
{
    if (path == NULL) return "";
    return strncmp(path, "/sys", 4) == 0 ? path + 4 : path;
}

static bool sysfs_device_is_driver_owned(const char *dev_path, const char *dev_name, const device_t *device)
{
    if (device == NULL || device->type != DEVICE_STREAM) return false;
    if (dev_name != NULL && strncmp(dev_name, "event", 5) == 0) return true;
    return dev_path != NULL && strstr(dev_path, "input/event") != NULL;
}

static void sysfs_write_text_file(const char *path, const char *content)
{
    if (path == NULL || content == NULL) return;
    vfs_node_t node = sysfs_ensure_file(path);
    if (node == NULL)
    {
        write_serial_fmt("sysfs: failed to create %s\n", path);
        return;
    }
    write_serial_fmt("[udev-trace] sysfs:write-text path=%s len=%llu\n",
                     path,
                     (unsigned long long)strlen(content));
    sysfs_write_node(node, content, strlen(content), 0);
    vfs_close(node);
}

static void sysfs_normalize_uevent_payload(char *payload, size_t *payload_len)
{
    if (payload == NULL || payload_len == NULL) return;

    size_t src_len = strlen(payload);
    size_t dst_len = 0;
    bool last_was_nul = false;
    for (size_t i = 0; i < src_len; i++)
    {
        char c = payload[i];
        if (c == '\n') c = '\0';
        if (c == '\0')
        {
            if (last_was_nul) continue;
            last_was_nul = true;
        }
        else
        {
            last_was_nul = false;
        }
        payload[dst_len++] = c;
    }

    if (dst_len == 0 || payload[dst_len - 1] != '\0') payload[dst_len++] = '\0';
    *payload_len = dst_len;
}

vfs_node_t sysfs_regist_dev(char type,
                            int major,
                            int minor,
                            const char *real_device_path,
                            const char *dev_name,
                            const char *other_uevent_content,
                            const char *subsystem_path,
                            const char *class_path,
                            const char *class_name,
                            const char *parent_device_path)
{
    if (real_device_path == NULL || dev_name == NULL || class_path == NULL || class_name == NULL) return NULL;
    if (sysfs_id == 0) sysfs_init();
    write_serial_fmt("[udev-trace] sysfs:regist-dev type=%c major=%d minor=%d real=%s devname=%s class=%s class_name=%s parent=%s\n",
                     type,
                     major,
                     minor,
                     real_device_path,
                     dev_name,
                     class_path,
                     class_name,
                     parent_device_path != NULL ? parent_device_path : "");
    const char *subsystem_name = subsystem_path != NULL ? sysfs_basename(subsystem_path) : sysfs_basename(class_path);
    const char *devpath_for_event = sysfs_devpath_from_path(real_device_path);

    vfs_node_t dev_dir = sysfs_ensure_dir(real_device_path);
    if (dev_dir == NULL)
    {
        write_serial_fmt("sysfs: failed to create device dir %s\n", real_device_path);
        return NULL;
    }
    sysfs_trace_node("regist-device-dir", real_device_path, dev_dir);

    char devnum_link[256];
    snprintf(devnum_link, sizeof(devnum_link), "/sys/dev/%s/%d:%d", type == 'b' ? "block" : "char", major, minor);
    vfs_node_t devnum_node = sysfs_ensure_symlink(devnum_link, real_device_path);
    if (devnum_node == NULL)
        write_serial_fmt("sysfs: failed to create %s -> %s\n", devnum_link, real_device_path);
    else
        vfs_close(devnum_node);

    const char *extra = other_uevent_content != NULL ? other_uevent_content : "";
    char devtype_line[64] = {};
    if (strstr(extra, "DEVTYPE=") == NULL)
    {
        if (type == 'b') snprintf(devtype_line, sizeof(devtype_line), "DEVTYPE=disk\n");
    }

    char content[512];
    snprintf(content,
             sizeof(content),
             "MAJOR=%d\nMINOR=%d\nDEVNAME=%s\nDEVPATH=%s\n%s%s%s",
             major,
             minor,
             dev_name,
             devpath_for_event,
             devtype_line,
             extra,
             (extra[0] != '\0' && extra[strlen(extra) - 1] == '\n')
                 ? ""
                 : "\n");

    char uevent_path[256];
    snprintf(uevent_path, sizeof(uevent_path), "%s/uevent", real_device_path);
    sysfs_write_text_file(uevent_path, content);

    char dev_attr[64];
    snprintf(dev_attr, sizeof(dev_attr), "%d:%d\n", major, minor);
    char dev_attr_path[256];
    snprintf(dev_attr_path, sizeof(dev_attr_path), "%s/dev", real_device_path);
    sysfs_write_text_file(dev_attr_path, dev_attr);

    if (subsystem_path != NULL)
    {
        vfs_node_t subsystem_dir = sysfs_ensure_dir(subsystem_path);
        if (subsystem_dir != NULL) vfs_close(subsystem_dir);
    }
    vfs_node_t class_dir = sysfs_ensure_dir(class_path);
    if (class_dir != NULL) vfs_close(class_dir);

    char class_link[256];
    snprintf(class_link, sizeof(class_link), "%s/%s", class_path, class_name);
    vfs_node_t class_node = sysfs_ensure_symlink(class_link, real_device_path);
    if (class_node != NULL) vfs_close(class_node);

    char subsystem_link[256];
    snprintf(subsystem_link, sizeof(subsystem_link), "%s/subsystem", real_device_path);
    vfs_node_t subsystem_node = sysfs_ensure_symlink(subsystem_link, subsystem_path != NULL ? subsystem_path : class_path);
    if (subsystem_node != NULL) vfs_close(subsystem_node);

    char device_link[256];
    snprintf(device_link, sizeof(device_link), "%s/device", real_device_path);
    vfs_node_t device_node =
        sysfs_ensure_symlink(device_link, parent_device_path != NULL ? parent_device_path : "/sys/devices/virtual");
    if (device_node != NULL) vfs_close(device_node);

    anonfs_notify_path_event(real_device_path, IN_CREATE | IN_ISDIR);
    int current_seq = __atomic_fetch_add(&sysfs_seqnum, 1, __ATOMIC_SEQ_CST);
    char action[1024];
    snprintf(action,
             sizeof(action),
             "add@%s\nACTION=add\nSEQNUM=%d\nTAGS=:systemd:\nSUBSYSTEM=%s\n%s",
             devpath_for_event,
             current_seq,
             subsystem_name,
             content);
    size_t action_len = 0;
    sysfs_normalize_uevent_payload(action, &action_len);
    if (action_len > 0) netlink_kernel_uevent_send(action, (int)MIN(action_len, sizeof(action)));
    write_serial_fmt("[udev-trace] sysfs:uevent-publish devpath=%s len=%llu seq=%d subsystem=%s\n",
                     devpath_for_event,
                     (unsigned long long)action_len,
                     current_seq,
                     subsystem_name);
    write_serial_fmt("sysfs: published %s %d:%d devname=%s class=%s path=%s\n",
                     type == 'b' ? "block" : "char",
                     major,
                     minor,
                     dev_name,
                     subsystem_name,
                     real_device_path);
    return dev_dir;
}

void sysfs_publish_kernel_device(const char *dev_path, const char *dev_name, char type, int major, int minor)
{
    if (dev_name == NULL || dev_name[0] == '\0') return;
    char real_path[256];
    char class_path[128];
    const char *class_name = "misc";

    if (strncmp(dev_name, "card", 4) == 0 || strstr(dev_path != NULL ? dev_path : "", "dri") != NULL)
    {
        class_name = "drm";
        snprintf(class_path, sizeof(class_path), "/sys/class/drm");
        snprintf(real_path, sizeof(real_path), "/sys/devices/virtual/drm/%s", dev_name);
    }
    else if (strncmp(dev_name, "fb", 2) == 0)
    {
        class_name = "graphics";
        snprintf(class_path, sizeof(class_path), "/sys/class/graphics");
        snprintf(real_path, sizeof(real_path), "/sys/devices/virtual/graphics/%s", dev_name);
    }
    else if (strncmp(dev_name, "event", 5) == 0 || strstr(dev_path != NULL ? dev_path : "", "input") != NULL)
    {
        class_name = "input";
        snprintf(class_path, sizeof(class_path), "/sys/class/input");
        if (strncmp(dev_name, "event", 5) == 0)
        {
            int event_index = 0;
            const char *p = dev_name + 5;
            while (*p >= '0' && *p <= '9')
            {
                event_index = event_index * 10 + (*p - '0');
                p++;
            }
            snprintf(real_path,
                     sizeof(real_path),
                     "/sys/devices/virtual/input/input%d/%s",
                     event_index,
                     dev_name);
        }
        else
        {
            snprintf(real_path, sizeof(real_path), "/sys/devices/virtual/input/%s", dev_name);
        }
    }
    else
    {
        snprintf(class_path, sizeof(class_path), "/sys/class/misc");
        snprintf(real_path, sizeof(real_path), "/sys/devices/virtual/misc/%s", dev_name);
    }

    char uevent[128];
    if (strcmp(class_name, "input") == 0)
    {
        const char *kind = (strncmp(dev_name, "event0", 6) == 0) ? "ID_INPUT_KEYBOARD=1\n"
                           : (strncmp(dev_name, "event1", 6) == 0) ? "ID_INPUT_MOUSE=1\n"
                                                                   : "";
        snprintf(uevent, sizeof(uevent), "ID_INPUT=1\n%sSUBSYSTEM=input\n", kind);
    }
    else if (strcmp(class_name, "drm") == 0)
    {
        snprintf(uevent, sizeof(uevent), "SUBSYSTEM=drm\nDEVTYPE=drm_minor\n");
    }
    else
    {
        snprintf(uevent, sizeof(uevent), "SUBSYSTEM=%s\n", class_name);
    }
    vfs_node_t node = sysfs_regist_dev(type,
                                       major,
                                       minor,
                                       real_path,
                                       dev_path != NULL ? dev_path : dev_name,
                                       uevent,
                                       class_path,
                                       class_path,
                                       dev_name,
                                       "/sys/devices/virtual");
    if (node != NULL) vfs_close(node);
}

extern device_t device_ctl[256];

void sysfs_publish_all_devices()
{
    if (sysfs_id == 0) sysfs_init();
    for (size_t i = 0; i < 256; i++)
    {
        if (device_ctl[i].flag == 0 || device_ctl[i].drive_name[0] == '\0') continue;
        const char *path = device_ctl[i].path != NULL ? device_ctl[i].path : device_ctl[i].drive_name;
        char node_name[64] = {};
        strncpy(node_name, device_ctl[i].drive_name, sizeof(node_name) - 1);
        const char *slash = strrchr(path, '/');
        if (slash != NULL && slash[1] != '\0')
        {
            memset(node_name, 0, sizeof(node_name));
            strncpy(node_name, slash + 1, sizeof(node_name) - 1);
        }
        if (sysfs_device_is_driver_owned(path, node_name, &device_ctl[i]))
        {
            write_serial_fmt("[udev-trace] sysfs:publish-all-skip-driver-owned path=%s name=%s major=%u minor=%u\n",
                             path != NULL ? path : "(null)",
                             node_name,
                             device_ctl[i].major,
                             device_ctl[i].minor);
            continue;
        }
        sysfs_publish_kernel_device(path,
                                    node_name,
                                    device_ctl[i].type == DEVICE_BLOCK ? 'b' : 'c',
                                    (int)device_ctl[i].major,
                                    (int)device_ctl[i].minor);
    }
}
