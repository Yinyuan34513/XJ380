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
#include "task/pcb.h"
#include <cpu/lock.h>
// #include "pipefs.h"
#include <syscall/syscall.h>
#include <device.h>

vfs_node_t rootdir = NULL;

static void empty_func() {}

struct vfs_callback   vfs_empty_callback;
struct vfs_filesystem vfs_empty_filesystem;

vfs_callback_t fs_callbacks[256] = {
    [0] = &vfs_empty_callback,
};

struct llist_header fs_metadata_list;

static int fs_nextid = 1;

#define callbackof(node, _name_) (fs_callbacks[(node)->fsid]->_name_)

spin_t vfs_operate_lock = SPIN_INIT;

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
    if (file->handle != NULL) {
        callbackof(file, stat)(file->handle, file);
    } else {
        callbackof(file, open)(file->parent->handle, file->name, file);
    }
}

static inline void do_update(vfs_node_t file) {
    if (file->type & file_none || file->handle == NULL || file->type & file_dir ||
        file->type & file_symlink || file->type & file_pipe)
        do_open(file);
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
    return (vfs_node_t)list_first(parent->child, data, streq(name, ((vfs_node_t)data)->name));
}

errno_t vfs_mkdir(const char *name) {
    if (name[0] != '/') return VFS_STATUS_FAILED;
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
            callbackof(father, mkdir)(father->handle, buf, current);
            do_update(current);
        } else {
            do_update(current);
            if (!(current->type & file_dir)) goto err;
        }
    }

    free(path);
    return EOK;

err:
    free(path);
    return VFS_STATUS_FAILED;
}

errno_t vfs_link(const char *name, const char *target_name) {
    vfs_node_t current = rootdir;
    char      *path    = strdup(name + 1);

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
    vfs_node_t node ;
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
    node->type      = file_none;
    callbackof(current, link)(current->handle, target_name, node);
    node->linkto = vfs_open(target_name);

    free(path);

    return EOK;

err:
    free(path);
    return VFS_STATUS_FAILED;
}

errno_t vfs_symlink(const char *name, const char *target_name) {
    vfs_node_t current = rootdir;
    char      *path    = strdup(name + 1);

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
    callbackof(current, symlink)(current->handle, target_name, node);
    node->linkto = vfs_open(target_name);

    free(path);

    return EOK;

err:
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

    if (parent == NULL || parent->type != file_dir) { return VFS_STATUS_FAILED; }

    // 创建文件
    vfs_node_t node = vfs_child_append(parent, filename, NULL);
    node->type      = file_none;
    errno_t status  = callbackof(parent, mkfile)(parent->handle, filename, node);
    free(fullpath);
    return status;
}

errno_t vfs_delete(vfs_node_t node) {
    if (node == rootdir) return VFS_STATUS_FAILED;
    node->type |= file_delete;
    return VFS_STATUS_SUCCESS;
}

errno_t vfs_rename(vfs_node_t node, const char *nw) {
    return callbackof(node, rename)(node->handle, nw);
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
    return (vfs_node_t)list_first(dir->child, data, streq(name, ((vfs_node_t)data)->name));
}

vfs_node_t vfs_open(const char *str) {
    if (unlikely(str == NULL)) return NULL;
    if (unlikely(str[0] != '/')) return NULL;
    if (str[1] == '\0') return rootdir; // 根目录

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

        do_update(current);
        if (current->type & file_symlink) {
            if (!current->parent || !current->linkto) { goto err; }

            current->type = file_symlink | file_proxy;

            vfs_node_t target = current->linkto;
            if (!target) goto err;

            target->refcount++;
            current = target;
            continue;
        }
    }

    free(path);
    return current;
err:
    free(path);
    return NULL;
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
    src->node->refcount++;
    nw->node       = src->node;
    nw->offset     = src->offset;
    nw->flags      = src->flags;
    nw->fd         = src->fd;
    vfs_node_t node = nw->node;
    // if (node->type == file_pipe) {
    //     pipe_specific_t *spec = node->handle;
    //     pipe_info_t     *pipe = spec->info;
    //     if (spec->write) {
    //         pipe->write_fds++;
    //     } else {
    //         pipe->read_fds++;
    //     }
    // }
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
    node->refcount = 0;
    node->blksz    = PAGE_SIZE;
    node->mode     = 0777;
    node->linkto   = NULL;
    if (parent) list_prepend(parent->child, node);
    return node;
}

errno_t vfs_close(vfs_node_t node) {
    if (unlikely(node == NULL)) return VFS_STATUS_FAILED;
    if (node == rootdir) return VFS_STATUS_SUCCESS;
    if (unlikely(node->handle == NULL)) return VFS_STATUS_SUCCESS;

    node->refcount--;

    if (node->type & file_proxy) return VFS_STATUS_SUCCESS;
    if (node->type & file_dir) return VFS_STATUS_SUCCESS;
    if (node->refcount != 0) return VFS_STATUS_SUCCESS;
    if (node->type & file_delete) {
        errno_t res = callbackof(node, del)(node->parent->handle, node);
        if (res < 0) return res;
        list_delete(node->parent->child, node);
        node->handle = NULL;
        vfs_free(node);
    } else {
        callbackof(node, close)(node->handle);
        node->handle = NULL;
    }
    return VFS_STATUS_SUCCESS;
}

bool is_virtual_fs(const char *src) {
    return ((uint64_t)src) == DEVFS_REGISTER_ID || ((uint64_t)src) == MODFS_REGISTER_ID ||
           ((uint64_t)src) == TMPFS_REGISTER_ID || ((uint64_t)src) == PIEFS_REGISTER_ID ||
           ((uint64_t)src) == NETFS_REGISTER_ID || ((uint64_t)src) == CPFS_REGISTER_ID ||
           ((uint64_t)src) == PROC_REGISTER_ID;
}

void vfs_free(vfs_node_t vfs) {
    if (vfs == NULL) return;
    list_free_with(vfs->child, (void (*)(void *))vfs_free);
    vfs_close(vfs);
    free(vfs->name);
    free(vfs);
}

void vfs_free_child(vfs_node_t vfs) {
    if (vfs == NULL) return;
    list_free_with(vfs->child, (void (*)(void *))vfs_free);
}

errno_t vfs_mount(const char *src, vfs_node_t node) {
    if (node == NULL) return VFS_STATUS_FAILED;
    if (node->type != file_dir) return VFS_STATUS_FAILED;
    for (int i = 1; i < fs_nextid; i++) {
        if (fs_callbacks[i]->mount(src, node) == 0) {
            node->fsid     = i;
            node->root     = node;
            node->is_mount = true;
            return VFS_STATUS_SUCCESS;
        }
    }
    return VFS_STATUS_FAILED;
}

size_t vfs_read(vfs_node_t file, void *addr, size_t offset, size_t size) {
    if (file == NULL || addr == NULL) return VFS_STATUS_FAILED;
    do_update(file);
    if (file->type == file_dir) return VFS_STATUS_FAILED;
    return callbackof(file, read)(file->handle, addr, offset, size);
}

size_t vfs_write(vfs_node_t file, void *addr, size_t offset, size_t size) {
    if (file == NULL || addr == NULL) return VFS_STATUS_FAILED;
    do_update(file);
    if (file->type == file_dir) return VFS_STATUS_FAILED;
    size_t ret = callbackof(file, write)(file->handle, addr, offset, size);
    do_update(file);
    return ret;
}

errno_t vfs_ioctl(vfs_node_t device, size_t options, void *arg) {
    if (device == NULL) return VFS_STATUS_FAILED;
    do_update(device);
    if (device->type == file_dir) return VFS_STATUS_FAILED;
    return callbackof(device, ioctl)(device->handle, options, arg);
}

errno_t vfs_poll(vfs_node_t node, size_t event) {
    do_update(node);
    if (node->type & file_dir) return -1;
    return callbackof(node, poll)(node->handle, event);
}

errno_t vfs_unmount(const char *path) {
    vfs_node_t node = vfs_open(path);
    if (node == NULL) return VFS_STATUS_FAILED;
    if (node->type != file_dir) return VFS_STATUS_FAILED;
    if (node->fsid == 0) return VFS_STATUS_FAILED;
    if (node->parent) {
        vfs_node_t cur = node;
        node           = node->parent;
        if (cur->root == cur) {
            vfs_free_child(cur);
            callbackof(cur, unmount)(cur->handle);
            cur->fsid     = node->fsid; // 交给上级
            cur->root     = node->root;
            cur->handle   = NULL;
            cur->child    = NULL;
            cur->is_mount = false;
            if (cur->fsid) do_update(cur);
            return VFS_STATUS_SUCCESS;
        }
    }
    return VFS_STATUS_FAILED;
}

size_t vfs_readlink(vfs_node_t node, char *buf, size_t bufsize) {
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
    spin_lock(get_path_lock);
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
    spin_unlock(get_path_lock);
    return buff;
}

char *at_resolve_pathname(int dirfd, char *pathname) {
    if (pathname[0] == '/') { // by absolute pathname
        return strdup(pathname);
    } else if (pathname[0] != '/') {
        if (dirfd == AT_FDCWD) { // relative to cwd
            return vfs_cwd_path_build(pathname);
        } else { // relative to dirfd, resolve accordingly
            vfs_node_t node = (vfs_node_t)queue_get(get_current_task()->parent_group->file_open, dirfd);
            if (!node) return NULL;
            if (node->type != file_dir) return NULL;

            char *dirname = vfs_get_fullpath(node);

            char *prefix = vfs_get_fullpath(node->root);

            int prefixLen   = strlen(prefix);
            int rootDirLen  = strlen(dirname);
            int pathnameLen = strlen(pathname) + 1;

            char *out = (char*)malloc(prefixLen + rootDirLen + 1 + pathnameLen + 1);

            memcpy(out, prefix, prefixLen);
            memcpy(&out[prefixLen], dirname, rootDirLen);
            out[prefixLen + rootDirLen] = '/';
            memcpy(&out[prefixLen + rootDirLen + 1], pathname, pathnameLen);

            free(dirname);
            free(prefix);

            return out;
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

static size_t stdin_read(int drive, uint8_t *buffer, size_t number, size_t lba) {
    bool is_sti = are_interrupts_enabled();
    open_interrupt;

    size_t i = 0;
    for (; i < number; i++) {
        char c = (char)kernel_getch();
        if (c == 0x7f) { c = '\b'; }
        if (c == 0x9) { c = '\t'; }
        if (c == '\b') {
            if (i > 0) {
                buffer[i--] = '\0';
                i--;
            }
            continue;
        }
        buffer[i] = c;
    }

    if (!is_sti) open_interrupt;

    return i;
}

static size_t stdout_write(int drive, uint8_t *buffer, size_t number, size_t lba) {
    write_serial_fmt((char*)buffer);
    return number;
}

void stdio_init()
{
    device_t stdio;
    stdio.type = DEVICE_STREAM;
    strcpy(stdio.drive_name, "stdio");
    stdio.flag        = 1;
    stdio.sector_size = 1;
    stdio.size        = 1;
    stdio.read        = stdin_read;
    stdio.write       = stdout_write;
    stdio.ioctl       = (ioctlf)empty;
    stdio.poll        = (pollf)empty;
    stdio.map         = (mapf)empty;
    regist_device(NULL, stdio);
}