/**
 * Virtual File System Interface
 * 虚拟文件系统接口
 * Create by min0911Y & zhouzhihao & copi143
 */
#define ALL_IMPLEMENTATION

#include "fs/vfs/vfs.h"
#include "errno.h"
// #include "iso9660.h"
#include "krlibc.h"
#include "fs/vfs/list.h"
#include "llist.h"
#include "pipe.h"
#include "rtc.h"
#include "task/pcb.h"
#include <ioctl.h>
#include <cpu/lock.h>
#include <fs/vfs/anonfs.h>
// #include "pipefs.h"
#include <syscall/syscall.h>
#include <device.h>
#include <task/poll.h>

vfs_node_t rootdir = NULL;

tty_t *defualt_tty = NULL;
int    tty_mode = KD_TEXT;
int    tty_kbmode = K_XLATE;
struct vt_mode current_vt_mode = {0};

static void empty_func() {}

struct vfs_callback   vfs_empty_callback;
struct vfs_filesystem vfs_empty_filesystem;

vfs_callback_t fs_callbacks[256] = {
    [0] = &vfs_empty_callback,
};

extern void p_xapi_output_kernel(const char *str);

struct llist_header fs_metadata_list;

static int fs_nextid = 1;
static spin_t vfs_child_list_lock = SPIN_INIT;

#define VFS_ALIAS_MAX 256

typedef struct
{
    char *alias_path;
    char *target_path;
} vfs_alias_entry_t;//就这样吧

static vfs_alias_entry_t vfs_aliases[VFS_ALIAS_MAX];
static spin_t vfs_alias_lock = SPIN_INIT;

#define callbackof(node, _name_) (fs_callbacks[(node)->fsid]->_name_)

static bool vfs_udev_trace_path(const char *path)
{
    if (path == NULL) return false;
    if ((uintptr_t)path < PAGE_SIZE) return false;
    return strstr(path, "/dev") != NULL || strstr(path, "/sys") != NULL ||
           strstr(path, "/run/udev") != NULL || strstr(path, "/run/dbus") != NULL ||
           strstr(path, "card0") != NULL || strstr(path, "renderD") != NULL ||
           strstr(path, "event") != NULL || strstr(path, "input") != NULL;
}

static const char *vfs_udev_safe_path(const char *path)
{
    if (path == NULL) return "(null)";
    if ((uintptr_t)path < PAGE_SIZE) return "(virtual)";
    return path;
}

static bool vfs_udev_trace_node(vfs_node_t node)
{
    if (node == NULL) return false;
    if (node->type & (file_socket | file_pipe | file_anon | file_epoll)) return true;
    char *path = vfs_get_fullpath(node);
    bool trace = vfs_udev_trace_path(path);
    free(path);
    return trace;
}

static void vfs_udev_log_node(const char *op, const char *path, vfs_node_t node, int64_t ret)
{
    if (!vfs_udev_trace_path(path) && !vfs_udev_trace_node(node)) return;
    write_serial_fmt("[udev-trace] vfs:%s path=%s ret=%lld node=%p parent=%p type=0x%x fsid=%u root=%p mount=%d handle=%p rdev=%llu size=%llu child=%llu ref=%u name=%s\n",
                     op != NULL ? op : "?",
                     vfs_udev_safe_path(path),
                     (long long)ret,
                     node,
                     node != NULL ? node->parent : NULL,
                     node != NULL ? node->type : 0,
                     node != NULL ? node->fsid : 0,
                     node != NULL ? node->root : NULL,
                     node != NULL && node->is_mount ? 1 : 0,
                     node != NULL ? node->handle : NULL,
                     node != NULL ? (unsigned long long)node->rdev : 0,
                     node != NULL ? (unsigned long long)node->size : 0,
                     node != NULL ? (unsigned long long)list_length(node->child) : 0,
                     node != NULL ? node->refcount : 0,
                     node != NULL && node->name != NULL ? node->name : "(null)");
}

void vfs_child_lock() {
    spin_lock(&vfs_child_list_lock);
}

void vfs_child_unlock() {
    spin_unlock(&vfs_child_list_lock);
}

static inline char *pathtok(char **sp) {
    char *s = *sp;
    char *e = *sp;

    // 跳过所有连续的斜杠
    while (*e == '/') {
        e++;
    }

    // 如果已经到达字符串末尾，返回 NULL
    if (*e == '\0') {
        *sp = e; // 更新指针到字符串末尾
        return NULL;
    }

    s = e; // 设置令牌起始位置（第一个非斜杠字符）

    // 查找下一个斜杠或字符串结尾
    while (*e != '\0' && *e != '/') {
        e++;
    }

    // 保存下一个令牌的起始位置
    char *next = e;
    if (*e == '/') {
        next++; // 跳过斜杠指向下一个字符
    }

    // 终止当前令牌
    if (*e != '\0') { *e = '\0'; }

    *sp = next; // 更新指针到下一个令牌位置
    return s;   // 返回当前令牌
}

static inline void do_open(vfs_node_t file) {
    if (file == NULL) return;
    if (file->handle != NULL) {
        callbackof(file, stat)(file->handle, file);
    } else {
        if (file->parent == NULL || file->parent->handle == NULL) return;
        callbackof(file, open)(file->parent->handle, file->name, file);
    }
}

static inline void do_update(vfs_node_t file) {
    if (file == NULL) return;
    if ((file->type & file_symlink) && file->linkname != NULL) return;
    if (file->type & file_none || file->handle == NULL || file->type & file_dir ||
        file->type & file_symlink || file->type & file_pipe)
        do_open(file);
}

static inline bool vfs_ref_drop(vfs_node_t node)
{
    if (node == NULL) return false;
    if (node->refcount == 0)
    {
        write_serial_fmt("vfs_close: refcount underflow avoided for %s\n", node->name != NULL ? node->name : "(null)");
        return false;
    }
    node->refcount--;
    return node->refcount == 0;
}

static void vfs_detach_from_parent(vfs_node_t node)
{
    if (node == NULL || node->parent == NULL) return;
    vfs_child_lock();
    node->parent->child = list_delete(node->parent->child, node);
    vfs_child_unlock();
    node->parent = NULL;
}

static void vfs_node_destroy(vfs_node_t node)
{
    if (node == NULL) return;
    if (node->linkto != NULL && node->linkto != node)
    {
        vfs_close(node->linkto);
        node->linkto = NULL;
    }
    free(node->name);
    free(node->linkname);
    free(node);
}

static bool vfs_node_has_persistent_handle(vfs_node_t node)
{
    if (node == NULL) return false;
    if (node->type & file_proxy) return true;

    vfs_filesystem_t fs = get_filesystem_node(node);
    if (fs == NULL) return false;
    return strcmp(fs->name, "devfs") == 0 || strcmp(fs->name, "tmpfs") == 0 ||
           strcmp(fs->name, "proc") == 0 || strcmp(fs->name, "proc_self") == 0 ||
           strcmp(fs->name, "sysfs") == 0 || strcmp(fs->name, "nmfs") == 0 ||
           strcmp(fs->name, "dnsfs") == 0 || strcmp(fs->name, "unixpath") == 0;
}

static errno_t vfs_release_final(vfs_node_t node, bool force_destroy, bool per_fd_close_done)
{
    if (node == NULL) return VFS_STATUS_FAILED;

    if (node->type & (file_ptmx | file_pts))
    {
        if (!per_fd_close_done && node->handle != NULL) callbackof(node, close)(node->handle);
        node->handle = NULL;
        if (force_destroy || node->parent == NULL)
        {
            vfs_detach_from_parent(node);
            vfs_node_destroy(node);
        }
        return VFS_STATUS_SUCCESS;
    }

    if (node->type & file_pipe)
    {
        if (!per_fd_close_done && node->handle != NULL) callbackof(node, close)(node->handle);
        node->handle = NULL;
        vfs_detach_from_parent(node);
        vfs_node_destroy(node);
        return VFS_STATUS_SUCCESS;
    }

    if (node->type & file_epoll)
    {
        epoll_file_t *ep = (epoll_file_t *)node->handle;
        if (ep != NULL)
        {
            if (ep->watches != NULL)
            {
                while (true)
                {
                    epoll_watch_t *watch = (epoll_watch_t *)queue_dequeue(ep->watches);
                    if (watch == NULL) break;
                    free(watch);
                }
                queue_destroy(ep->watches);
            }
            free(ep);
            node->handle = NULL;
        }
        vfs_detach_from_parent(node);
        vfs_node_destroy(node);
        return VFS_STATUS_SUCCESS;
    }

    if (node->type & file_anon)
    {
        if (node->handle != NULL) callbackof(node, del)(NULL, node);
        vfs_detach_from_parent(node);
        vfs_node_destroy(node);
        return VFS_STATUS_SUCCESS;
    }

    if (node->type & file_delete)
    {
        if (node->handle != NULL && !((node->type & file_symlink) && node->linkname != NULL))
        {
            void *parent_handle = node->delete_parent_handle != NULL ? node->delete_parent_handle :
                                                                   (node->parent != NULL ? node->parent->handle : NULL);
            errno_t res = callbackof(node, del)(parent_handle, node);
            if (res < 0) return res;
            if (node->handle != NULL && !vfs_node_has_persistent_handle(node))
                callbackof(node, close)(node->handle);
        }
        node->handle = NULL;
        vfs_detach_from_parent(node);
        vfs_node_destroy(node);
        return VFS_STATUS_SUCCESS;
    }

    if (node->handle == NULL)
    {
        if (force_destroy)
        {
            vfs_detach_from_parent(node);
            vfs_node_destroy(node);
        }
        return VFS_STATUS_SUCCESS;
    }

    if (node->type & file_proxy)
    {
        if (force_destroy)
        {
            void *parent_handle = node->parent != NULL ? node->parent->handle : NULL;
            callbackof(node, del)(parent_handle, node);
            node->handle = NULL;
            vfs_detach_from_parent(node);
            vfs_node_destroy(node);
        }
        return VFS_STATUS_SUCCESS;
    }

    if (!vfs_node_has_persistent_handle(node))
    {
        callbackof(node, close)(node->handle);
        node->handle = NULL;
    }
    if (force_destroy)
    {
        vfs_detach_from_parent(node);
        vfs_node_destroy(node);
    }
    return VFS_STATUS_SUCCESS;
}

vfs_filesystem_t get_filesystem(char *type) {
    vfs_filesystem_t filesystem = NULL;
    vfs_filesystem_t pos, n;
    llist_for_each(pos, n, &fs_metadata_list, node) {
        if (strcmp(pos->name, type) == 0) { filesystem = pos; }
    }
    return filesystem;
}

vfs_filesystem_t get_filesystem_node(vfs_node_t node) {
    vfs_filesystem_t filesystem = NULL;
    vfs_filesystem_t pos, n;
    llist_for_each(pos, n, &fs_metadata_list, node) {
        if (pos->fsid == node->fsid) { filesystem = pos; }
    }
    return filesystem;
}

vfs_node_t vfs_child_append(vfs_node_t parent, const char *name, void *handle) {
    vfs_node_t node = vfs_node_alloc(parent, name);
    if (node == NULL) return NULL;
    node->handle = handle;
    return node;
}

static vfs_node_t vfs_child_find(vfs_node_t parent, const char *name) {
    if (parent == NULL || name == NULL) return NULL;
    vfs_child_lock();
    vfs_node_t child = (vfs_node_t)list_first(parent->child, data, streq(name, ((vfs_node_t)data)->name));
    vfs_child_unlock();
    return child;
}

errno_t vfs_register_alias(const char *alias_path, const char *target_path)
{
    if (alias_path == NULL || target_path == NULL) return VFS_STATUS_FAILED;
    if (alias_path[0] != '/' || target_path[0] != '/') return VFS_STATUS_FAILED;

    spin_lock(&vfs_alias_lock);
    for (size_t i = 0; i < VFS_ALIAS_MAX; i++)
    {
        if (vfs_aliases[i].alias_path != NULL && streq(vfs_aliases[i].alias_path, alias_path))
        {
            char *new_target = strdup(target_path);
            if (new_target == NULL)
            {
                spin_unlock(&vfs_alias_lock);
                return VFS_STATUS_FAILED;
            }
            free(vfs_aliases[i].target_path);
            vfs_aliases[i].target_path = new_target;
            spin_unlock(&vfs_alias_lock);
            return EOK;
        }
    }

    for (size_t i = 0; i < VFS_ALIAS_MAX; i++)
    {
        if (vfs_aliases[i].alias_path == NULL)
        {
            vfs_aliases[i].alias_path = strdup(alias_path);
            vfs_aliases[i].target_path = strdup(target_path);
            if (vfs_aliases[i].alias_path == NULL || vfs_aliases[i].target_path == NULL)
            {
                free(vfs_aliases[i].alias_path);
                free(vfs_aliases[i].target_path);
                vfs_aliases[i].alias_path = NULL;
                vfs_aliases[i].target_path = NULL;
                spin_unlock(&vfs_alias_lock);
                return VFS_STATUS_FAILED;
            }
            spin_unlock(&vfs_alias_lock);
            return EOK;
        }
    }

    spin_unlock(&vfs_alias_lock);
    return VFS_STATUS_FAILED;
}

static char *vfs_alias_target_dup(const char *path)
{
    char *target = NULL;
    spin_lock(&vfs_alias_lock);
    for (size_t i = 0; i < VFS_ALIAS_MAX; i++)
    {
        if (vfs_aliases[i].alias_path != NULL && streq(vfs_aliases[i].alias_path, path))
        {
            target = strdup(vfs_aliases[i].target_path);
            break;
        }
    }
    spin_unlock(&vfs_alias_lock);
    return target;
}

static char *vfs_join_abs_path(vfs_node_t parent, const char *name)
{
    if (parent == NULL || name == NULL) return NULL;
    char *parent_path = vfs_get_fullpath(parent);
    if (parent_path == NULL) return NULL;
    size_t parent_len = strlen(parent_path);
    size_t name_len = strlen(name);
    char *out = (char *)malloc(parent_len + name_len + 2);
    if (out == NULL)
    {
        free(parent_path);
        return NULL;
    }
    sprintf(out, "%s%s%s", parent_path, parent_len > 1 ? "/" : "", name);
    free(parent_path);
    return out;
}

static void vfs_notify_path(vfs_node_t parent, const char *name, uint32_t mask)
{
    char *path = vfs_join_abs_path(parent, name);
    if (path == NULL) return;
    if (vfs_udev_trace_path(path))
        write_serial_fmt("[udev-trace] vfs:notify path=%s mask=0x%x parent=%p name=%s\n",
                         path,
                         mask,
                         parent,
                         name != NULL ? name : "(null)");
    anonfs_notify_path_event(path, mask);
    free(path);
}

errno_t vfs_mkdir(const char *name) {
    if (name[0] != '/')
    {
        vfs_udev_log_node("mkdir-invalid", name, NULL, VFS_STATUS_FAILED);
        return VFS_STATUS_FAILED;
    }
    char      *path     = strdup(name + 1);
    char      *save_ptr = path;
    vfs_node_t current  = rootdir;
    for (const char *buf = pathtok(&save_ptr); buf; buf = pathtok(&save_ptr)) {
        const vfs_node_t father = current;
        if (streq(buf, ".")) continue;
        if (streq(buf, "..")) {
            if (current->parent && current->type & file_dir) {
                current = current->parent;
                goto upd;
            } else {
                goto err;
            }
        }
        current = vfs_child_find(current, buf);

    upd:
        if (current == NULL) {
            current       = vfs_node_alloc(father, buf);
            current->type = file_dir;
            errno_t mk_ret = callbackof(father, mkdir)(father->handle, buf, current);
            char *created_path = vfs_get_fullpath(current);
            vfs_udev_log_node("mkdir-created", created_path, current, mk_ret);
            free(created_path);
            vfs_notify_path(father, buf, IN_CREATE | IN_ISDIR);
            do_update(current);
        } else {
            do_update(current);
            if (!(current->type & file_dir))
            {
                char *bad_path = vfs_get_fullpath(current);
                vfs_udev_log_node("mkdir-not-dir", bad_path, current, VFS_STATUS_FAILED);
                free(bad_path);
                goto err;
            }
        }
    }

    vfs_udev_log_node("mkdir-ok", name, current, EOK);
    free(path);
    return EOK;

err:
    vfs_udev_log_node("mkdir-fail", name, current, VFS_STATUS_FAILED);
    free(path);
    return VFS_STATUS_FAILED;
}

errno_t vfs_link(const char *name, const char *target_name) {
    vfs_node_t current = rootdir;
    char      *path    = strdup(name + 1);
    errno_t    link_ret = VFS_STATUS_FAILED;
    char      *created_path = NULL;
    vfs_node_t node = NULL;

    char *save_ptr = path;
    char *filename = path + strlen(path);

    while (*--filename != '/' && filename != path) {}
    if (filename != path) {
        *filename++ = '\0';
    } else {
        goto create;
    }

    if (strlen(path) == 0) {
        free(path);
        return -ENOENT;
    }
    for (const char *buf = pathtok(&save_ptr); buf; buf = pathtok(&save_ptr)) {
        if (streq(buf, ".")) continue;
        if (streq(buf, "..")) {
            if (!current->parent || !(current->type & file_dir)) goto err;
            current = current->parent;
            continue;
        }
        vfs_node_t new_current = vfs_child_find(current, buf);
        if (new_current == NULL) {
            new_current       = vfs_node_alloc(current, buf);
            new_current->type = file_dir;
            callbackof(current, mkdir)(current->handle, buf, new_current);
        }
        current = new_current;
        do_update(current);

        if (!(current->type & file_dir)) goto err;
    }

create:
    if (filename == NULL || *filename == '\0') goto err;
    if (vfs_child_find(current, filename) != NULL)
    {
        link_ret = -EEXIST;
        goto err;
    }
    node = vfs_child_append(current, filename, NULL);
    if (node == NULL)
    {
        link_ret = -ENOMEM;
        goto err;
    }
    node->type      = file_none;
    link_ret = callbackof(current, link)(current->handle, target_name, node);
    created_path = vfs_get_fullpath(node);
    vfs_udev_log_node("link-created", created_path, node, link_ret);
    free(created_path);
    if (link_ret < 0)
    {
        vfs_detach_from_parent(node);
        vfs_node_destroy(node);
        free(path);
        return link_ret;
    }
    node->linkto = vfs_open(target_name);

    free(path);

    return EOK;

err:
    vfs_udev_log_node("link-fail", name, current, VFS_STATUS_FAILED);
    free(path);
    return link_ret;
}

errno_t vfs_symlink(const char *name, const char *target_name) {
    vfs_node_t current = rootdir;
    char      *path    = strdup(name + 1);
    errno_t    symlink_ret = -EIO;
    char      *created_path = NULL;

    char *save_ptr = path;
    char *filename = path + strlen(path);

    while (*--filename != '/' && filename != path) {}
    if (filename != path) {
        *filename++ = '\0';
    } else {
        goto create;
    }
    vfs_node_t node;

    if (strlen(path) == 0) {
        free(path);
        return -1;
    }
    for (const char *buf = pathtok(&save_ptr); buf; buf = pathtok(&save_ptr)) {
        if (streq(buf, ".")) continue;
        if (streq(buf, "..")) {
            if (!current->parent || !(current->type & file_dir)) goto err;
            current = current->parent;
            continue;
        }
        vfs_node_t new_current = vfs_child_find(current, buf);
        if (new_current == NULL) {
            new_current       = vfs_node_alloc(current, buf);
            new_current->type = file_dir;
            callbackof(current, mkdir)(current->handle, buf, new_current);
        }
        current = new_current;
        do_update(current);

        if (!(current->type & file_dir)) goto err;
    }

create:
    node = vfs_child_append(current, filename, NULL);
    node->type      = file_symlink;
    node->linkname  = strdup(target_name);
    node->size      = node->linkname != NULL ? strlen(node->linkname) : 0;
    symlink_ret = callbackof(current, symlink)(current->handle, target_name, node);
    created_path = vfs_get_fullpath(node);
    vfs_udev_log_node("symlink-created", created_path, node, symlink_ret);
    if (vfs_udev_trace_path(created_path) || vfs_udev_trace_path(target_name))
        write_serial_fmt("[udev-trace] vfs:symlink-target link=%s target=%s ret=%d\n",
                         created_path != NULL ? created_path : "(null)",
                         target_name != NULL ? target_name : "(null)",
                         symlink_ret);
    free(created_path);
    vfs_notify_path(current, filename, IN_CREATE);
    node->linkto = vfs_open(target_name);

    free(path);

    return EOK;

err:
    vfs_udev_log_node("symlink-fail", name, current, -EIO);
    free(path);
    return -EIO;
}

errno_t vfs_mkfile(const char *name) {
    if (name[0] != '/') return VFS_STATUS_FAILED;

    // 分离路径和文件名
    char *fullpath  = strdup(name);
    char *filename  = fullpath;
    char *lastslash = strrchr(fullpath, '/');

    if (lastslash == fullpath) {
        // 根目录下的文件
        filename   = fullpath + 1;
        *lastslash = '\0';
    } else if (lastslash) {
        *lastslash = '\0';
        filename   = lastslash + 1;
    }

    // 打开父目录
    vfs_node_t parent;
    if (lastslash == fullpath) {
        parent = rootdir;
    } else {
        parent = vfs_open(fullpath);
    }

    if (parent == NULL || parent->type != file_dir)
    {
        if (parent != NULL && parent != rootdir) vfs_close(parent);
        free(fullpath);
        return VFS_STATUS_FAILED;
    }

    // 创建文件
    vfs_node_t node = vfs_child_append(parent, filename, NULL);
    if (node == NULL)
    {
        if (parent != rootdir) vfs_close(parent);
        free(fullpath);
        return -ENOMEM;
    }
    node->type      = file_none;
    errno_t status  = callbackof(parent, mkfile)(parent->handle, filename, node);
    vfs_udev_log_node("mkfile-created", name, node, status);
    if (status == EOK) vfs_notify_path(parent, filename, IN_CREATE);
    else
    {
        vfs_detach_from_parent(node);
        vfs_node_destroy(node);
    }
    if (parent != rootdir) vfs_close(parent);
    free(fullpath);
    return status;
}

errno_t vfs_delete(vfs_node_t node) {
    if (node == rootdir)
    {
        vfs_udev_log_node("delete-root", NULL, node, VFS_STATUS_FAILED);
        return VFS_STATUS_FAILED;
    }
    if (node == NULL || node->parent == NULL)
    {
        vfs_udev_log_node("delete-invalid", NULL, node, VFS_STATUS_FAILED);
        return VFS_STATUS_FAILED;
    }
    if ((node->type & file_dir) && node->child != NULL)
    {
        vfs_udev_log_node("delete-not-empty", NULL, node, -ENOTEMPTY);
        return -ENOTEMPTY;
    }

    char *deleted_path = vfs_get_fullpath(node);
    vfs_udev_log_node("delete-begin", deleted_path, node, 0);
    node->delete_parent_handle = node->parent != NULL ? node->parent->handle : NULL;
    node->type |= file_delete;
    uint32_t delete_mask = (node->type & file_dir) ? (IN_DELETE | IN_ISDIR) : IN_DELETE;
    uint32_t last_delete_ref = vfs_node_has_persistent_handle(node) ? 1 : 0;
    if (node->refcount > last_delete_ref + 1)
    {
        vfs_detach_from_parent(node);
        errno_t ret = vfs_close(node);
        if (ret == VFS_STATUS_SUCCESS && deleted_path != NULL) anonfs_notify_path_event(deleted_path, delete_mask);
        vfs_udev_log_node("delete-deferred", deleted_path, node, ret);
        free(deleted_path);
        return ret;
    }

    errno_t ret = vfs_release_final(node, true, false);
    if (ret < 0)
    {
        node->type &= (uint16_t)~file_delete;
        node->delete_parent_handle = NULL;
    }
    else if (deleted_path != NULL)
    {
        anonfs_notify_path_event(deleted_path, delete_mask);
    }
    if (vfs_udev_trace_path(deleted_path))
        write_serial_fmt("[udev-trace] vfs:delete-finish path=%s ret=%d mask=0x%x\n",
                         deleted_path,
                         ret,
                         delete_mask);
    free(deleted_path);
    return ret;
}

void vfs_detach(vfs_node_t node)
{
    vfs_detach_from_parent(node);
}

errno_t vfs_rename(vfs_node_t node, const char *nw) {
    if (node == NULL || nw == NULL)
    {
        vfs_udev_log_node("rename-invalid", nw, node, VFS_STATUS_FAILED);
        return VFS_STATUS_FAILED;
    }
    if ((node->type & file_symlink) && node->linkname != NULL)
    {
        char *new_name = strrchr(nw, '/');
        new_name = new_name != NULL ? new_name + 1 : (char *)nw;
        if (*new_name == '\0')
        {
            vfs_udev_log_node("rename-empty-name", nw, node, VFS_STATUS_FAILED);
            return VFS_STATUS_FAILED;
        }
        char *copy = strdup(new_name);
        if (copy == NULL)
        {
            vfs_udev_log_node("rename-alloc-fail", nw, node, VFS_STATUS_FAILED);
            return VFS_STATUS_FAILED;
        }

        char *parent_path = strdup(nw);
        if (parent_path == NULL)
        {
            free(copy);
            return VFS_STATUS_FAILED;
        }
        char *last_slash = strrchr(parent_path, '/');
        if (last_slash == parent_path)
        {
            parent_path[1] = '\0';
        }
        else if (last_slash != NULL)
        {
            *last_slash = '\0';
        }
        else
        {
            strcpy(parent_path, ".");
        }

        bool new_parent_opened = !streq(parent_path, ".");
        vfs_node_t new_parent = new_parent_opened ? vfs_open(parent_path) : node->parent;
        free(parent_path);
        if (new_parent == NULL || !(new_parent->type & file_dir))
        {
            if (new_parent != NULL && new_parent_opened) vfs_close(new_parent);
            free(copy);
            vfs_udev_log_node("rename-parent-fail", nw, node, VFS_STATUS_FAILED);
            return VFS_STATUS_FAILED;
        }

        char *old_path = vfs_get_fullpath(node);
        if (new_parent != node->parent)
        {
            vfs_node_t old_parent = node->parent;
            vfs_child_lock();
            old_parent->child = list_delete(old_parent->child, node);
            new_parent->child = list_prepend(new_parent->child, node);
            node->parent = new_parent;
            node->fsid = new_parent->fsid;
            node->root = new_parent->root;
            node->dev = new_parent->dev;
            vfs_child_unlock();
        }
        if (new_parent_opened) vfs_close(new_parent);

        free(node->name);
        node->name = copy;
        vfs_udev_log_node("rename-symlink", nw, node, VFS_STATUS_SUCCESS);
        if (old_path != NULL)
        {
            anonfs_notify_path_event(old_path, IN_MOVED_FROM);
            free(old_path);
        }
        anonfs_notify_path_event(nw, IN_MOVED_TO);
        return VFS_STATUS_SUCCESS;
    }
    char *new_name = strrchr(nw, '/');
    new_name = new_name != NULL ? new_name + 1 : (char *)nw;
    if (*new_name == '\0')
    {
        vfs_udev_log_node("rename-empty-name", nw, node, VFS_STATUS_FAILED);
        return VFS_STATUS_FAILED;
    }

    char *new_name_copy = strdup(new_name);
    if (new_name_copy == NULL)
    {
        vfs_udev_log_node("rename-alloc-fail", nw, node, VFS_STATUS_FAILED);
        return VFS_STATUS_FAILED;
    }

    char *parent_path = strdup(nw);
    if (parent_path == NULL)
    {
        free(new_name_copy);
        return VFS_STATUS_FAILED;
    }
    char *last_slash = strrchr(parent_path, '/');
    if (last_slash == parent_path)
    {
        parent_path[1] = '\0';
    }
    else if (last_slash != NULL)
    {
        *last_slash = '\0';
    }
    else
    {
        strcpy(parent_path, ".");
    }

    vfs_node_t old_parent = node->parent;
    bool new_parent_opened = !streq(parent_path, ".");
    vfs_node_t new_parent = new_parent_opened ? vfs_open(parent_path) : old_parent;
    free(parent_path);
    if (new_parent == NULL || !(new_parent->type & file_dir) || new_parent->fsid != node->fsid)
    {
        if (new_parent != NULL && new_parent_opened) vfs_close(new_parent);
        free(new_name_copy);
        vfs_udev_log_node("rename-parent-fail", nw, node, VFS_STATUS_FAILED);
        return VFS_STATUS_FAILED;
    }

    do_update(node);
    if (node->handle == NULL)
    {
        if (new_parent_opened) vfs_close(new_parent);
        free(new_name_copy);
        vfs_udev_log_node("rename-no-handle", nw, node, VFS_STATUS_FAILED);
        return VFS_STATUS_FAILED;
    }
    char *old_path = vfs_get_fullpath(node);
    errno_t ret = callbackof(node, rename)(node->handle, nw);
    if (vfs_udev_trace_path(old_path) || vfs_udev_trace_path(nw))
        write_serial_fmt("[udev-trace] vfs:rename old=%s new=%s node=%p ret=%d\n",
                         old_path != NULL ? old_path : "(null)",
                         nw,
                         node,
                         ret);
    if (ret == EOK)
    {
        if (new_parent != old_parent)
        {
            vfs_child_lock();
            if (old_parent != NULL) old_parent->child = list_delete(old_parent->child, node);
            new_parent->child = list_prepend(new_parent->child, node);
            node->parent = new_parent;
            node->fsid   = new_parent->fsid;
            node->root   = new_parent->root;
            node->dev    = new_parent->dev;
            vfs_child_unlock();
        }
        free(node->name);
        node->name = new_name_copy;
        if (old_path != NULL) anonfs_notify_path_event(old_path, IN_MOVED_FROM);
        anonfs_notify_path_event(nw, IN_MOVED_TO);
    }
    else
    {
        free(new_name_copy);
    }
    if (new_parent_opened) vfs_close(new_parent);
    free(old_path);
    return ret;
}

errno_t vfs_resize(vfs_node_t node, uint64_t size) {
    if (node == NULL) return VFS_STATUS_FAILED;
    do_update(node);
    if (node->type == file_dir) return VFS_STATUS_FAILED;
    callbackof(node, resize)(node->handle, size);
    do_update(node);
    return VFS_STATUS_SUCCESS;
}

int vfs_regist(const char *name, vfs_callback_t callback, int register_id, uint64_t magic) {
    if (callback == NULL) return VFS_STATUS_FAILED;
    for (size_t i = 0; i < sizeof(struct vfs_callback) / sizeof(void *); i++) {
        if (((void **)callback)[i] == NULL) return VFS_STATUS_FAILED;
    }
    int id           = fs_nextid++;
    fs_callbacks[id] = callback;

    vfs_filesystem_t filesystem = (vfs_filesystem_t)malloc(sizeof(struct vfs_filesystem));
    filesystem->callback        = callback;
    filesystem->id              = register_id;
    filesystem->fsid            = id;
    filesystem->magic           = magic;
    strcpy(filesystem->name, name);
    llist_init_head(&filesystem->node);
    llist_append(&fs_metadata_list, &filesystem->node);
    return id;
}

vfs_node_t vfs_do_search(vfs_node_t dir, const char *name) {
    if (dir == NULL || name == NULL) return NULL;
    vfs_child_lock();
    vfs_node_t child = (vfs_node_t)list_first(dir->child, data, streq(name, ((vfs_node_t)data)->name));
    vfs_child_unlock();
    return child;
}

static vfs_node_t vfs_open_impl(const char *str, bool follow_final_symlink, bool allow_alias) {
    if (unlikely(str == NULL)) return NULL;
    if (unlikely(str[0] != '/')) return NULL;
    if (str[1] == '\0') { // 根目录
        if (rootdir != NULL) rootdir->refcount++;
        return rootdir;
    }

    char *path = strdup(str + 1);
    if (unlikely(path == NULL)) return NULL;

    char      *save_ptr = path;
    vfs_node_t current  = rootdir;

    for (char *buf = pathtok(&save_ptr); buf; buf = pathtok(&save_ptr)) {
        if (streq(buf, ".")) {
            // 当前目录，不需要操作
            continue;
        } else if (streq(buf, "..")) {
            // 父目录
            if (current->parent) { current = current->parent; }
            continue;
        }

        current = vfs_child_find(current, buf);
        if (current == NULL) { goto err; }

        bool final_component_had_handle = current->handle != NULL;
        do_update(current);
        bool is_final_component = save_ptr == NULL || *save_ptr == '\0';
        if (is_final_component && !final_component_had_handle && current->handle != NULL && current->refcount == 1)
            current->refcount = 0;
        if (is_final_component && !follow_final_symlink) break;
        if (current->type & file_symlink) {
            if (!current->parent) { goto err; }

            bool dynamic_symlink = current->name != NULL && streq(current->name, "self") &&
                                   current->parent != NULL && current->parent->parent == NULL &&
                                   current->handle != NULL && current->linkto == NULL;
            if (!dynamic_symlink) current->type = file_symlink | file_proxy;

            vfs_node_t target = dynamic_symlink ? NULL : current->linkto;
            if (target == NULL)
            {
                char target_path[256];
                size_t len = vfs_readlink(current, target_path, sizeof(target_path) - 1);
                if (len == (size_t)VFS_STATUS_FAILED || len == 0 || len >= sizeof(target_path)) goto err;
                target_path[len] = '\0';
                if (target_path[0] == '/')
                {
                    target = vfs_open(target_path);
                }
                else
                {
                    char *parent_path = vfs_get_fullpath(current->parent);
                    if (parent_path == NULL) goto err;
                    size_t parent_len = strlen(parent_path);
                    size_t target_len = strlen(target_path);
                    char *joined_target = (char *)malloc(parent_len + target_len + 2);
                    if (joined_target == NULL)
                    {
                        free(parent_path);
                        goto err;
                    }
                    sprintf(joined_target, "%s%s%s", parent_path,
                            parent_path[parent_len - 1] == '/' ? "" : "/", target_path);
                    char *full_target = normalize_path(joined_target);
                    free(joined_target);
                    free(parent_path);
                    if (full_target == NULL) goto err;
                    target = vfs_open(full_target);
                    free(full_target);
                }
                if (!dynamic_symlink) current->linkto = target;
            }
            if (!target) goto err;

            current = target;
            continue;
        }
    }

    free(path);
    if (current != NULL) current->refcount++;
    return current;
err:
    free(path);
    if (allow_alias) {
        char *alias_target = vfs_alias_target_dup(str);
        if (alias_target != NULL) {
            write_serial_fmt("[busybox-debug] vfs_alias_hit %s -> %s\n", str, alias_target);
            vfs_node_t alias_node = vfs_open_impl(alias_target, follow_final_symlink, false);
            free(alias_target);
            return alias_node;
        }
    }
    return NULL;
}

vfs_node_t vfs_open(const char *str) {
    return vfs_open_impl(str, true, true);
}

vfs_node_t vfs_open_no_follow(const char *str) {
    return vfs_open_impl(str, false, true);
}

void vfs_update(vfs_node_t node) {
    do_update(node);
}

void vfs_deinit() {
    // 目前并不支持
}

fd_file_handle *fd_dup(fd_file_handle *src) {
    fd_file_handle *nw = (fd_file_handle *)malloc(sizeof(fd_file_handle));
    // not_null_assets(new, "fd_dup out of memory.");
    nw->node       = src->node;
    nw->offset     = src->offset;
    nw->flags      = src->flags;
    nw->fd         = src->fd;
    vfs_node_t node = nw->node;
    if ((node->type & (file_ptmx | file_pts)) && fs_callbacks[node->fsid]->dup != (void *)empty_func) {
        fs_callbacks[node->fsid]->dup(node);
        node->refcount++;
        return nw;
    }
    if ((node->type & file_pipe) && node->handle != NULL) {
        pipe_specific_t *spec = (pipe_specific_t *)node->handle;
        pipe_info_t     *pipe = spec != NULL ? spec->info : NULL;
        if (pipe != NULL) {
            spin_lock(&pipe->lock);
            if (spec->write) {
                pipe->write_fds++;
            } else {
                pipe->read_fds++;
            }
            spin_unlock(&pipe->lock);
        }
    }
    node->refcount++;
    return nw;
}

vfs_node_t get_rootdir() {
    return rootdir;
}

void set_rootdir(vfs_node_t node) {
    rootdir         = node;
    rootdir->parent = NULL;
}

void *vfs_map(vfs_node_t node, uint64_t addr, uint64_t len, uint64_t prot, uint64_t flags,
              uint64_t offset) {
    if (unlikely(node == NULL)) return NULL;
    if (unlikely(node->type == file_dir)) return NULL;
    return callbackof(node, map)(node->handle, (void *)addr, offset, len, prot, flags);
}

vfs_node_t vfs_node_alloc(vfs_node_t parent, const char *name) {
    vfs_node_t node = (vfs_node_t)malloc(sizeof(struct vfs_node));
    // not_null_assets(node, "vfs alloc null");
    if (unlikely(node == NULL)) return NULL;
    memset(node, 0, sizeof(struct vfs_node));
    node->parent   = parent;
    node->name     = name ? strdup(name) : NULL;
    node->type     = file_none;
    node->fsid     = parent ? parent->fsid : 0;
    node->root     = parent ? parent->root : node;
    node->dev      = parent ? parent->dev : 0;
    node->refcount = 1;
    node->blksz    = PAGE_SIZE;
    node->mode     = 0777;
    node->linkto   = NULL;
    if (parent)
    {
        vfs_child_lock();
        parent->child = list_prepend(parent->child, node);
        vfs_child_unlock();
    }
    return node;
}

errno_t vfs_close(vfs_node_t node) {
    if (unlikely(node == NULL)) return VFS_STATUS_FAILED;
    bool had_ref = node->refcount > 0;
    bool final_ref = vfs_ref_drop(node);

    if (node == rootdir) return VFS_STATUS_SUCCESS;

    bool per_fd_close_done = had_ref && (node->type & (file_ptmx | file_pts | file_pipe)) && node->handle != NULL;
    if (per_fd_close_done)
        callbackof(node, close)(node->handle);
    uint32_t last_delete_ref = vfs_node_has_persistent_handle(node) ? 1 : 0;
    bool unlinked_final_ref = had_ref && (node->type & file_delete) && node->parent == NULL &&
                              node->refcount <= last_delete_ref;
    if (!final_ref && !unlinked_final_ref) return VFS_STATUS_SUCCESS;

    return vfs_release_final(node, false, per_fd_close_done);
}

bool is_virtual_fs(const char *src) {
    return ((uint64_t)src) == DEVFS_REGISTER_ID || ((uint64_t)src) == MODFS_REGISTER_ID ||
           ((uint64_t)src) == TMPFS_REGISTER_ID || ((uint64_t)src) == TMPFS_REGISTER_ID + 1000 ||
           ((uint64_t)src) == TMPFS_REGISTER_ID + 2000 || ((uint64_t)src) == PIEFS_REGISTER_ID ||
           ((uint64_t)src) == NETFS_REGISTER_ID || ((uint64_t)src) == CPFS_REGISTER_ID ||
           ((uint64_t)src) == PROC_REGISTER_ID || ((uint64_t)src) == DNS_REGISTER_ID ||
           ((uint64_t)src) == NM_REGISTER_ID;
}

void vfs_free(vfs_node_t vfs) {
    if (vfs == NULL) return;
    vfs_child_lock();
    list_t child = vfs->child;
    vfs->child   = NULL;
    for (list_t cur = child; cur != NULL; cur = cur->next)
    {
        vfs_node_t child_node = (vfs_node_t)cur->data;
        if (child_node != NULL) child_node->parent = NULL;
    }
    vfs_child_unlock();
    list_free_with(child, (void (*)(void *))vfs_free);
    if (vfs != rootdir)
        vfs_release_final(vfs, true, false);
}

void vfs_free_child(vfs_node_t vfs) {
    if (vfs == NULL) return;
    vfs_child_lock();
    list_t child = vfs->child;
    vfs->child   = NULL;
    vfs_child_unlock();
    list_free_with(child, (void (*)(void *))vfs_free);
}

errno_t vfs_mount(const char *src, vfs_node_t node) {
    const bool src_virtual = is_virtual_fs(src);
    if (node == NULL)
    {
        vfs_udev_log_node("mount-null", src_virtual ? NULL : src, node, VFS_STATUS_FAILED);
        return VFS_STATUS_FAILED;
    }
    if (node->type != file_dir)
    {
        vfs_udev_log_node("mount-not-dir", src_virtual ? NULL : src, node, VFS_STATUS_FAILED);
        return VFS_STATUS_FAILED;
    }
    char *target_path = vfs_get_fullpath(node);
    vfs_udev_log_node("mount-begin", target_path, node, 0);
    for (int i = 1; i < fs_nextid; i++) {
        if (fs_callbacks[i]->mount(src, node) == 0) {
            node->fsid     = i;
            node->root     = node;
            node->is_mount = true;
            if (vfs_udev_trace_path(target_path))
            {
                vfs_filesystem_t fs = get_filesystem_node(node);
                write_serial_fmt("[udev-trace] vfs:mount-ok target=%s source=0x%llx fsid=%d fs=%s child=%llu handle=%p\n",
                                 target_path != NULL ? target_path : "(null)",
                                 (unsigned long long)(uintptr_t)src,
                                 i,
                                 fs != NULL ? fs->name : "(null)",
                                 (unsigned long long)list_length(node->child),
                                 node->handle);
            }
            free(target_path);
            return VFS_STATUS_SUCCESS;
        }
    }
    vfs_udev_log_node("mount-fail", target_path, node, VFS_STATUS_FAILED);
    free(target_path);
    return VFS_STATUS_FAILED;
}

size_t vfs_read(vfs_node_t file, void *addr, size_t offset, size_t size) {
    if (file == NULL || addr == NULL) return VFS_STATUS_FAILED;
    do_update(file);
    if (file->type == file_dir) return VFS_STATUS_FAILED;
    size_t ret = callbackof(file, read)(file->handle, addr, offset, size);
    if ((ret == (size_t)VFS_STATUS_FAILED || ((ssize_t)ret < 0 && (ssize_t)ret != -EWOULDBLOCK &&
                                               (ssize_t)ret != -EAGAIN)) &&
        vfs_udev_trace_node(file))
    {
        char *path = vfs_get_fullpath(file);
        write_serial_fmt("[udev-trace] vfs:read-error path=%s node=%p offset=%llu size=%llu ret=%lld handle=%p type=0x%x fsid=%u\n",
                         path != NULL ? path : "(null)",
                         file,
                         (unsigned long long)offset,
                         (unsigned long long)size,
                         (long long)ret,
                         file->handle,
                         file->type,
                         file->fsid);
        free(path);
    }
    return ret;
}

size_t vfs_write(vfs_node_t file, void *addr, size_t offset, size_t size) {
    if (file == NULL || addr == NULL) return VFS_STATUS_FAILED;
    do_update(file);
    if (file->type == file_dir) return VFS_STATUS_FAILED;
    size_t ret = callbackof(file, write)(file->handle, addr, offset, size);
    if (vfs_udev_trace_node(file))
    {
        char *path = vfs_get_fullpath(file);
        write_serial_fmt("[udev-trace] vfs:write path=%s node=%p offset=%llu size=%llu ret=%lld handle=%p type=0x%x fsid=%u\n",
                         path != NULL ? path : "(null)",
                         file,
                         (unsigned long long)offset,
                         (unsigned long long)size,
                         (long long)ret,
                         file->handle,
                         file->type,
                         file->fsid);
        free(path);
    }
    if (ret != (size_t)VFS_STATUS_FAILED)
    {
        char *path = vfs_get_fullpath(file);
        if (path != NULL)
        {
            anonfs_notify_path_event(path, IN_MODIFY);
            free(path);
        }
    }
    do_update(file);
    return ret;
}

errno_t vfs_ioctl(vfs_node_t device, size_t options, void *arg) {
    if (device == NULL) return VFS_STATUS_FAILED;
    do_update(device);
    if (device->type == file_dir) return VFS_STATUS_FAILED;
    errno_t ret = callbackof(device, ioctl)(device->handle, options, arg);
    if (vfs_udev_trace_node(device))
    {
        char *path = vfs_get_fullpath(device);
        write_serial_fmt("[udev-trace] vfs:ioctl path=%s node=%p req=0x%llx ret=%d handle=%p type=0x%x fsid=%u\n",
                         path != NULL ? path : "(null)",
                         device,
                         (unsigned long long)options,
                         ret,
                         device->handle,
                         device->type,
                         device->fsid);
        free(path);
    }
    return ret;
}

errno_t vfs_poll(vfs_node_t node, size_t event) {
    do_update(node);
    if (node->type & file_dir) return -1;
    if (node->type & file_epoll)
    {
        epoll_file_t *ep = (epoll_file_t *)node->handle;
        if (ep == NULL || ep->watches == NULL) return EPOLLERR;
        spin_lock(&ep->watches->lock);
        bool has_watches = ep->watches->size != 0;
        spin_unlock(&ep->watches->lock);
        return has_watches ? (event & EPOLLIN) : 0;
    }
    errno_t ret = callbackof(node, poll)(node->handle, event);
    if (ret < 0 && vfs_udev_trace_node(node))
    {
        char *path = vfs_get_fullpath(node);
        write_serial_fmt("[udev-trace] vfs:poll-error path=%s node=%p events=0x%llx ret=%d handle=%p type=0x%x fsid=%u\n",
                         path != NULL ? path : "(null)",
                         node,
                         (unsigned long long)event,
                         ret,
                         node->handle,
                         node->type,
                         node->fsid);
        free(path);
    }
    return ret;
}

errno_t vfs_unmount(const char *path) {
    vfs_node_t node = vfs_open(path);
    vfs_udev_log_node("unmount-lookup", path, node, node != NULL ? 0 : VFS_STATUS_FAILED);
    if (node == NULL) return VFS_STATUS_FAILED;
    if (node->type != file_dir)
    {
        vfs_udev_log_node("unmount-not-dir", path, node, VFS_STATUS_FAILED);
        return VFS_STATUS_FAILED;
    }
    if (node->fsid == 0)
    {
        vfs_udev_log_node("unmount-rootfsid", path, node, VFS_STATUS_FAILED);
        return VFS_STATUS_FAILED;
    }
    if (node->parent) {
        vfs_node_t cur = node;
        node           = node->parent;
        if (cur->root == cur) {
            vfs_udev_log_node("unmount-begin", path, cur, 0);
            vfs_free_child(cur);
            callbackof(cur, unmount)(cur->handle);
            cur->fsid     = node->fsid; // 交给上级
            cur->root     = node->root;
            cur->handle   = NULL;
            cur->child    = NULL;
            cur->is_mount = false;
            if (cur->fsid) do_update(cur);
            vfs_udev_log_node("unmount-ok", path, cur, VFS_STATUS_SUCCESS);
            return VFS_STATUS_SUCCESS;
        }
    }
    vfs_udev_log_node("unmount-fail", path, node, VFS_STATUS_FAILED);
    return VFS_STATUS_FAILED;
}

size_t vfs_readlink(vfs_node_t node, char *buf, size_t bufsize) {
    if (node != NULL && node->linkname != NULL) {
        size_t link_size = strlen(node->linkname);
        size_t copy_size = MIN(link_size, bufsize);
        memcpy(buf, node->linkname, copy_size);
        return copy_size;
    }
    size_t ret = callbackof(node, readlink)(node, buf, 0, bufsize);
    return ret;
}

void *general_map(vfs_read_t read_callback, void *file, uint64_t addr, uint64_t len, uint64_t prot,
                  uint64_t flags, uint64_t offset) {
    UNUSED(flags);
    pcb_t current_task        = get_current_task()->parent_group;
    // current_task->mmap_start += (len + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1));

    uint64_t pt_flags = PTE_USER | PTE_WRITEABLE | PTE_PRESENT;

    if (prot & PROT_READ) pt_flags |= PTE_PRESENT;
    if (prot & PROT_WRITE) pt_flags |= PTE_WRITEABLE;
    if (!(prot & PROT_EXEC)) pt_flags |= PTE_NO_EXECUTE;

    page_map_range_to_random(get_current_directory(), addr & (~(PAGE_SIZE - 1)),
                             (len + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1)), pt_flags);

    ssize_t ret = read_callback(file, (void *)addr, offset, len);
    if (ret < 0) return (void *)-ENOMEM;

    return (void *)addr;
}

spin_t get_path_lock = SPIN_INIT;

// 使用请记得free掉返回的buff
char *vfs_get_fullpath(vfs_node_t node) {
    if (node == NULL) return NULL;
    int inital = 32;
    spin_lock(&get_path_lock);
    vfs_node_t *nodes = (vfs_node_t *)malloc(sizeof(vfs_node_t) * inital);
    // not_null_assets(nodes, "vfs_get_fullpath: null alloc.");
    int count = 0;
    for (vfs_node_t cur = node; cur; cur = cur->parent) {
        if (count >= inital) {
            inital *= 2;
            nodes   = (vfs_node_t *)realloc((void *)nodes, (size_t)(sizeof(vfs_node_t) * inital));
        }
        nodes[count++] = cur;
    }
    // 正常的路径都不应该超过这个数值
    char *buff = (char *)malloc(256);
    strcpy(buff, "/");
    for (int j = count - 1; j >= 0; j--) {
        if (nodes[j] == rootdir) continue;
        strcat(buff, nodes[j]->name);
        if (j != 0) strcat(buff, "/");
    }
    free(nodes);
    spin_unlock(&get_path_lock);
    return buff;
}

char *at_resolve_pathname(int dirfd, char *pathname) {
    if (pathname[0] == '/') { // by absolute pathname
        return normalize_path(pathname);
    } else if (pathname[0] != '/') {
        if (dirfd == AT_FDCWD) { // relative to cwd
            return vfs_cwd_path_build(pathname);
        } else { // relative to dirfd, resolve accordingly
            fd_file_handle *handle = (fd_file_handle *)queue_get(get_current_task()->parent_group->file_open, dirfd);
            if (!handle || !handle->node) return NULL;
            vfs_node_t node = handle->node;
            if (node->type != file_dir) return NULL;

            char *dirname = vfs_get_fullpath(node);
            if (dirname == NULL) return NULL;
            char *joined = pathacat(dirname, pathname);
            free(dirname);
            if (joined == NULL) return NULL;
            char *normalized = normalize_path(joined);
            free(joined);
            return normalized;
        }
    }

    return NULL;
}
char *normalize_path(const char *path)
{
    if (!path) return NULL;

    size_t len    = strlen(path);
    char  *result = (char *)malloc(len + 1);
    if (!result) return NULL;

    char *dup = strdup(path);
    if (!dup)
    {
        free(result);
        return NULL;
    }

    strcpy(result, "/");
    if (strcmp(path, "/") == 0)
    {
        free(dup);
        return result;
    }

    char *start = dup;
    if (*start == '/') start++;

    char *token = strtok(start, "/");
    while (token)
    {
        if (strcmp(token, ".") == 0) {}
        else if (strcmp(token, "..") == 0)
        {
            char *last_slash = strrchr(result, '/');
            if (last_slash != result)
                *last_slash = '\0';
            else
                result[1] = '\0';
        }
        else
        {
            if (result[strlen(result) - 1] != '/') strcat(result, "/");
            strcat(result, token);
        }

        token = strtok(NULL, "/");
    }

    free(dup);
    return result;
}
char *vfs_cwd_path_build(char *src) {
    char *s = src;
    char *path;
    char *bpath = NULL;
    if (s[0] == '/') {
        path = strdup(s);
    } else {
        bpath = vfs_get_fullpath(get_current_task()->cwd);
        path  = pathacat(bpath, s);
    }
    char *normalized_path = normalize_path(path);
    free(path);
    free(bpath);
    return normalized_path;
}

bool vfs_init() {
    for (size_t i = 0; i < sizeof(struct vfs_callback) / sizeof(void *); i++) {
        ((void **)&vfs_empty_callback)[i] = (void *)empty_func;
    }
    llist_init_head(&fs_metadata_list);
    rootdir       = vfs_node_alloc(NULL, "/");
    rootdir->type = file_dir;
    write_serial_fmt("Virtual File System initialize.\n");
    return true;
}

static tty_t *current_tty()
{
    tcb_t task = get_current_task();
    if (task == NULL || task->parent_group == NULL || task->parent_group->tty == NULL) return get_default_tty();
    return task->parent_group->tty;
}

static xtttp_dtt *current_xtttp_terminal()
{
    tcb_t task = get_current_task();
    pcb_t process = task != NULL ? task->parent_group : NULL;
    while (process != NULL && process != kernel_group)
    {
        if (process->xtttp_stc != NULL && process->xtttp_stc->is_shell) return process->xtttp_stc;
        process = process->parent_task;
    }
    return NULL;
}

static void tty_kernel_flush(tty_t *tty)
{
    (void)tty;
}

static void tty_kernel_print(tty_t *tty, const char *msg)
{
    (void)tty;
    write_serial_fmt("%s", msg);
    if (current_xtttp_terminal() != NULL) p_xapi_output_kernel(msg);
}

static void tty_kernel_putc(tty_t *tty, int c)
{
    (void)tty;
    char buf[2] = {(char)c, '\0'};
    write_serial_fmt("%s", buf);
    if (current_xtttp_terminal() != NULL) p_xapi_output_kernel(buf);
}

static void tty_termios_default(termios_t *termios)
{
    memset(termios, 0, sizeof(*termios));
    termios->c_lflag = ECHO | ICANON | IEXTEN | ISIG;
    termios->c_iflag = BRKINT | ICRNL | INPCK | ISTRIP | IXON;
    termios->c_oflag = OPOST;
    termios->c_cflag = CS8 | CREAD | CLOCAL;
    termios->c_line = 0;
    termios->c_cc[VINTR] = 3;
    termios->c_cc[VQUIT] = 28;
    termios->c_cc[VERASE] = 0x7F;
    termios->c_cc[VKILL] = 21;
    termios->c_cc[VEOF] = 4;
    termios->c_cc[VTIME] = 0;
    termios->c_cc[VMIN] = 1;
    termios->c_cc[VSTART] = 17;
    termios->c_cc[VSTOP] = 19;
    termios->c_cc[VSUSP] = 26;
    termios->c_cc[VREPRINT] = 18;
    termios->c_cc[VDISCARD] = 15;
    termios->c_cc[VWERASE] = 23;
    termios->c_cc[VLNEXT] = 22;
}

static void tty_termios_to_termios2(const termios_t *termios, struct termios2 *termios2)
{
    memset(termios2, 0, sizeof(*termios2));
    termios2->c_iflag = termios->c_iflag;
    termios2->c_oflag = termios->c_oflag;
    termios2->c_cflag = termios->c_cflag;
    termios2->c_lflag = termios->c_lflag;
    termios2->c_line = termios->c_line;
    memcpy(termios2->c_cc, termios->c_cc, MIN(sizeof(termios2->c_cc), sizeof(termios->c_cc)));
    termios2->c_ispeed = B38400;
    termios2->c_ospeed = B38400;
}

static void tty_termios_from_termios2(termios_t *termios, const struct termios2 *termios2)
{
    termios->c_iflag = termios2->c_iflag;
    termios->c_oflag = termios2->c_oflag;
    termios->c_cflag = termios2->c_cflag;
    termios->c_lflag = termios2->c_lflag;
    termios->c_line = termios2->c_line;
    memcpy(termios->c_cc, termios2->c_cc, MIN(sizeof(termios->c_cc), sizeof(termios2->c_cc)));
}

tty_t *alloc_default_tty()
{
    tty_t *tty = (tty_t *)malloc(sizeof(tty_t));
    if (tty == NULL) return NULL;
    memset(tty, 0, sizeof(*tty));
    tty->width = 720;
    tty->height = 400;
    tty->print = tty_kernel_print;
    tty->putchar = tty_kernel_putc;
    tty->flush = tty_kernel_flush;
    tty_termios_default(&tty->termios);
    tty->is_sigterm = false;
    return tty;
}

void free_tty(tty_t *tty)
{
    if (tty == NULL || tty == defualt_tty) return;
    free(tty);
}

tty_t *get_default_tty()
{
    return defualt_tty;
}

static size_t stdin_read(int drive, uint8_t *buffer, size_t number, size_t lba) {
    bool is_sti = are_interrupts_enabled();
    open_interrupt;

    xtttp_dtt *terminal = current_xtttp_terminal();
    if (terminal != NULL)
    {
        terminal->wait_for_input = true;
        while (!terminal->input_lock)
        {
            scheduler_yield();
        }
    }

    size_t i = 0;
    for (; i < number; i++) {
        char c;
        if (terminal != NULL)
        {
            c = terminal->input[0];
            if (c == '\0') break;
            size_t remaining = strlen(terminal->input);
            if (remaining > 0)
            {
                memmove(terminal->input, terminal->input + 1, remaining);
            }
        }
        else
        {
            c = (char)get_keyboard_input();
        }
        if (c == '\0') { break; }
        tty_t *tty = current_tty();
        uint32_t lflag = tty != NULL ? tty->termios.c_lflag : 0;
        if (c == 0x9) { c = '\t'; }
        if (terminal == NULL && (c == 0x7f || c == '\b') && (lflag & ICANON)) {
            if (tty != NULL && (lflag & ECHO)) write_serial_fmt("\b \b");
            if (i > 0) {
                i--;
                buffer[i] = '\0';
            }
            continue;
        }
        if (tty != NULL && (lflag & ECHO)) write_serial_fmt("%c", c);
        buffer[i] = c;
        if (c == '\n') {
            i++;
            break;
        }
    }

    if (terminal != NULL)
    {
        if (terminal->input[0] == '\0')
        {
            terminal->input_lock = false;
            terminal->wait_for_input = false;
        }
        else
        {
            terminal->input_lock = true;
            terminal->wait_for_input = false;
        }
    }

    if (!is_sti) close_interrupt;

    pcb_t process = get_current_task() ? get_current_task()->parent_group : NULL;
    if (process != NULL && process->linux_abi) {
        // write_serial_fmt("[] stdin_read requested=%llu ret=%llu\n",
        //                  (unsigned long long)number,
        //                  (unsigned long long)i);
    }

    return i;
}

static size_t stdout_write(int drive, uint8_t *buffer, size_t number, size_t lba) {
    tty_t *tty = current_tty();
    if (tty != NULL && tty->print != NULL)
    {
        char *text = (char *)malloc(number + 1);
        if (text == NULL) return 0;
        memcpy(text, buffer, number);
        text[number] = '\0';
        tty->print(tty, text);
        free(text);
    }
    else
    {
        for (size_t i = 0; i < number; i++) write_serial_fmt("%c", buffer[i]);
    }
    return number;
}

static errno_t tty_ioctl(device_t *device, size_t req, void *arg)
{
    (void)device;
    tty_t *tty = current_tty();
    if (tty == NULL) return -ENOTTY;

    switch (req)
    {
    case TIOCGWINSZ:
    {
        struct winsize *ws = (struct winsize *)arg;
        if (ws != NULL)
        {
            ws->ws_row = 25;
            ws->ws_col = 80;
            ws->ws_xpixel = (unsigned short)tty->width;
            ws->ws_ypixel = (unsigned short)tty->height;
        }
        break;
    }
    case TCGETS:
    {
        struct termios *termios = (struct termios *)arg;
        if (termios == NULL) return -EFAULT;
        memcpy(termios, &tty->termios, sizeof(struct termios));
        break;
    }
    case TCGETS2:
    {
        struct termios2 *termios2 = (struct termios2 *)arg;
        if (termios2 == NULL) return -EFAULT;
        tty_termios_to_termios2(&tty->termios, termios2);
        break;
    }
    case TCSETS:
    case TCSETSF:
    case TCSETSW:
    {
        const struct termios *termios = (const struct termios *)arg;
        if (termios == NULL) return -EFAULT;
        memcpy(&tty->termios, termios, sizeof(struct termios));
        break;
    }
    case TCSETS2:
    case TCSETSF2:
    case TCSETSW2:
    {
        const struct termios2 *termios2 = (const struct termios2 *)arg;
        if (termios2 == NULL) return -EFAULT;
        tty_termios_from_termios2(&tty->termios, termios2);
        break;
    }
    case TIOCGPGRP:
        if (arg == NULL) return -EFAULT;
        *(int *)arg = get_current_task()->parent_group->pid;
        tty->is_sigterm = true;
        break;
    case TIOCSPGRP:
    case TIOCSCTTY:
        break;
    case TIOCGETD:
        if (arg == NULL) return -EFAULT;
        *(int *)arg = tty->termios.c_line;
        break;
    case TIOCSETD:
        if (arg == NULL) return -EFAULT;
        tty->termios.c_line = (uint8_t)(*(int *)arg);
        break;
    case TIOCGSID:
        if (arg == NULL) return -EFAULT;
        *(int *)arg = get_current_task()->parent_group->pid;
        break;
    case TIOCGDEV:
        if (arg == NULL) return -EFAULT;
        *(unsigned int *)arg = (5U << 8) | 1U;
        break;
    case TIOCOUTQ:
    case FIONREAD:
        if (arg == NULL) return -EFAULT;
        *(int *)arg = 0;
        break;
    case TIOCMGET:
        if (arg == NULL) return -EFAULT;
        *(int *)arg = TIOCM_DTR | TIOCM_RTS | TIOCM_CTS | TIOCM_CAR | TIOCM_DSR;
        break;
    case TIOCMBIS:
    case TIOCMBIC:
    case TIOCMSET:
    case TCFLSH:
    case TIOCNXCL:
    case TIOCEXCL:
    case TIOCNOTTY:
    case TCSBRK:
    case TCSBRKP:
    case TIOCSBRK:
    case TIOCCBRK:
        break;
    case KDGETMODE:
        if (arg == NULL) return -EFAULT;
        *(int *)arg = tty_mode;
        break;
    case KDSETMODE:
        tty_mode = (int)(uintptr_t)arg;
        break;
    case KDGKBMODE:
        if (arg == NULL) return -EFAULT;
        *(int *)arg = tty_kbmode;
        break;
    case KDSKBMODE:
        tty_kbmode = (int)(uintptr_t)arg;
        break;
    case VT_SETMODE:
        if (arg == NULL) return -EFAULT;
        memcpy(&current_vt_mode, arg, sizeof(current_vt_mode));
        break;
    case VT_GETMODE:
        if (arg == NULL) return -EFAULT;
        memcpy(arg, &current_vt_mode, sizeof(current_vt_mode));
        break;
    case VT_OPENQRY:
        if (arg == NULL) return -EFAULT;
        *(int *)arg = 1;
        break;
    case VT_ACTIVATE:
    case VT_WAITACTIVE:
        break;
    case VT_GETSTATE:
    {
        if (arg == NULL) return -EFAULT;
        struct vt_state *state = (struct vt_state *)arg;
        state->v_active = 1;
        state->v_state = 1;
        break;
    }
    default:
        return -ENOTTY;
    }
    return EOK;
}

static errno_t tty_poll(size_t events)
{
    int revents = 0;
    if (events & POLLIN)
    {
        xtttp_dtt *terminal = current_xtttp_terminal();
        if (terminal != NULL)
        {
            if (terminal->input[0] != '\0') revents |= POLLIN;
        }
        else
        {
            revents |= POLLIN;
        }
    }
    if (events & POLLOUT) revents |= POLLOUT;
    return revents;
}

static size_t null_read(int drive, uint8_t *buffer, size_t number, size_t lba)
{
    UNUSED(drive);
    UNUSED(buffer);
    UNUSED(lba);
    return 0;
}

static size_t null_write(int drive, uint8_t *buffer, size_t number, size_t lba)
{
    UNUSED(drive);
    UNUSED(buffer);
    UNUSED(lba);
    return number;
}

static size_t urandom_read(int drive, uint8_t *buffer, size_t number, size_t lba)
{
    UNUSED(drive);
    UNUSED(lba);
    if (buffer == NULL) return 0;

    tm tm_now;
    time_read(&tm_now);
    uint64_t seed = nanoTime() ^ ((uint64_t)mktime(&tm_now) << 32);
    tcb_t task = get_current_task();
    if (task != NULL)
    {
        seed ^= (uint64_t)task->tid;
        if (task->parent_group != NULL) seed ^= ((uint64_t)task->parent_group->pid << 16);
    }

    for (size_t i = 0; i < number; i++)
    {
        seed ^= seed << 13;
        seed ^= seed >> 7;
        seed ^= seed << 17;
        buffer[i] = (uint8_t)(seed & 0xffU);
    }
    return number;
}

static size_t urandom_write(int drive, uint8_t *buffer, size_t number, size_t lba)
{
    UNUSED(drive);
    UNUSED(buffer);
    UNUSED(lba);
    return number;
}

void build_tty_device()
{
    device_t stdio = {};
    stdio.type = DEVICE_STREAM;
    strcpy(stdio.drive_name, "stdio");
    stdio.flag        = 1;
    stdio.sector_size = 1;
    stdio.size        = 1;
    stdio.read        = stdin_read;
    stdio.write       = stdout_write;
    stdio.ioctl       = tty_ioctl;
    stdio.poll        = tty_poll;
    stdio.map         = (mapf)empty;
    regist_device(NULL, stdio);

    device_t ttydev = stdio;
    strcpy(ttydev.drive_name, "tty");
    regist_device(NULL, ttydev);

    device_t console = stdio;
    strcpy(console.drive_name, "console");
    regist_device(NULL, console);

    device_t serial_console = stdio;
    strcpy(serial_console.drive_name, "ttyS0");
    regist_device(NULL, serial_console);

    device_t null_dev = {};
    null_dev.type = DEVICE_STREAM;
    strcpy(null_dev.drive_name, "null");
    null_dev.flag        = 1;
    null_dev.sector_size = 1;
    null_dev.size        = 0;
    null_dev.read        = null_read;
    null_dev.write       = null_write;
    null_dev.ioctl       = tty_ioctl;
    null_dev.poll        = tty_poll;
    null_dev.map         = (mapf)empty;
    regist_device(NULL, null_dev);

    device_t urandom_dev = {};
    urandom_dev.type = DEVICE_STREAM;
    strcpy(urandom_dev.drive_name, "urandom");
    urandom_dev.flag        = 1;
    urandom_dev.sector_size = 1;
    urandom_dev.size        = (size_t)-1;
    urandom_dev.read        = urandom_read;
    urandom_dev.write       = urandom_write;
    urandom_dev.ioctl       = tty_ioctl;
    urandom_dev.poll        = tty_poll;
    urandom_dev.map         = (mapf)empty;
    regist_device(NULL, urandom_dev);
}

void init_tty()
{
    defualt_tty = alloc_default_tty();
    build_tty_device();
}

void stdio_init()
{
    init_tty();
}
