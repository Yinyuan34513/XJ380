#define ALL_IMPLEMENTATION

#include "errno.h"
#include "fs/lwext4/lwext4fs.h"
#include "fs/vfs/vfs.h"
#include "krlibc.h"
#include "mutex.h"
#include "rtc.h"
#include <ext4.h>
#include <ext4_blockdev.h>
#include <ext4_inode.h>
#include <ext4_types.h>
#include <mm/alloc/alloc.h>

static const size_t LWEXT4_READAHEAD_BYTES = 0x80000UL;
static const uint64_t LWEXT4_DIR_CACHE_VALID = 1;

typedef struct lwext4_read_cache
{
    uint8_t *data;
    size_t   size_bytes;
    uint64_t start_block;
    uint32_t block_count;
    bool     valid;
} lwext4_read_cache_t;

typedef struct lwext4_mount
{
    vfs_node_t              device;
    struct ext4_blockdev    blockdev;
    struct ext4_blockdev_iface iface;
    uint8_t                *block_buffer;
    lwext4_read_cache_t     read_cache;
    char                   *device_name;
    char                   *mount_point;
} lwext4_mount_t;

typedef struct lwext4_handle
{
    lwext4_mount_t *mount;
    vfs_node_t      node;
    char           *path;
    bool            is_dir;
    bool            is_symlink;
    union
    {
        ext4_file *file;
        ext4_dir  *dir;
    };
} lwext4_handle_t;

static int     lwext4fs_id;
static mutex_t lwext4_lock;
static uint64_t lwext4_next_anon_inode = 2;
static uint32_t lwext4_next_mount_id;

static inline void lwext4fs_lock()
{
    mutex_lock(&lwext4_lock);
}

static inline void lwext4fs_unlock()
{
    mutex_unlock(&lwext4_lock);
}

static int lwext4_err(int ret)
{
    return ret == EOK ? EOK : -ret;
}

static uint64_t lwext4_time_to_ns(uint32_t seconds)
{
    return (uint64_t)seconds * 1000000000ULL;
}

static char *lwext4_join_path(const char *parent, const char *name)
{
    if (parent == NULL || name == NULL) return NULL;

    size_t parent_len = strlen(parent);
    size_t name_len   = strlen(name);
    bool   need_slash = parent_len > 0 && parent[parent_len - 1] != '/';

    char *path = (char *)malloc(parent_len + (need_slash ? 1 : 0) + name_len + 1);
    if (path == NULL) return NULL;

    strcpy(path, parent);
    if (need_slash) strcat(path, "/");
    strcat(path, name);
    return path;
}

static const char *lwext4_path_from_node(vfs_node_t node, char **owned)
{
    *owned = NULL;
    if (node == NULL) return NULL;

    lwext4_handle_t *handle = (lwext4_handle_t *)node->handle;
    if (handle != NULL && handle->path != NULL) return handle->path;

    *owned = vfs_get_fullpath(node);
    return *owned;
}

static bool lwext4_dir_cache_valid(vfs_node_t node)
{
    return node != NULL && (node->type & file_dir) && node->realsize == LWEXT4_DIR_CACHE_VALID;
}

static void lwext4_mark_dir_cache_valid(vfs_node_t node)
{
    if (node != NULL && (node->type & file_dir)) node->realsize = LWEXT4_DIR_CACHE_VALID;
}

static void lwext4_invalidate_dir_cache(vfs_node_t node)
{
    if (node != NULL && (node->type & file_dir)) node->realsize = 0;
}

static void lwext4_invalidate_parent_dir_cache(vfs_node_t node)
{
    if (node != NULL) lwext4_invalidate_dir_cache(node->parent);
}

static uint16_t lwext4_vfs_type_from_direntry(uint8_t inode_type)
{
    switch (inode_type)
    {
    case EXT4_DE_DIR:
        return file_dir;
    case EXT4_DE_SYMLINK:
        return file_symlink;
    default:
        return file_none;
    }
}

static uint16_t lwext4_vfs_type_from_mode(uint32_t mode)
{
    switch (mode & EXT4_INODE_MODE_TYPE_MASK)
    {
    case EXT4_INODE_MODE_DIRECTORY:
        return file_dir;
    case EXT4_INODE_MODE_SOFTLINK:
        return file_symlink;
    case EXT4_INODE_MODE_BLOCKDEV:
        return file_block;
    case EXT4_INODE_MODE_FIFO:
        return file_pipe;
    case EXT4_INODE_MODE_SOCKET:
        return file_socket;
    default:
        return file_none;
    }
}

static void lwext4_apply_times(vfs_node_t node, const char *path)
{
    if (node == NULL || path == NULL) return;

    uint32_t atime = 0;
    uint32_t mtime = 0;
    uint32_t ctime = 0;
    if (ext4_atime_get(path, &atime) == EOK) node->readtime = lwext4_time_to_ns(atime);
    if (ext4_mtime_get(path, &mtime) == EOK) node->writetime = lwext4_time_to_ns(mtime);
    if (ext4_ctime_get(path, &ctime) == EOK) node->createtime = lwext4_time_to_ns(ctime);
}

static int lwext4_apply_path(lwext4_mount_t *mount, const char *path, vfs_node_t node)
{
    if (mount == NULL || path == NULL || node == NULL) return -EINVAL;

    uint32_t mode = 0;
    int      ret  = ext4_mode_get(path, &mode);
    if (ret != EOK) return lwext4_err(ret);

    node->type        = lwext4_vfs_type_from_mode(mode);
    node->mode        = mode & 07777;
    node->permissions = node->mode;
    node->blksz       = PAGE_SIZE;
    node->dev         = node->root != NULL ? node->root->dev : 0;

    uint32_t uid = 0;
    uint32_t gid = 0;
    if (ext4_owner_get(path, &uid, &gid) == EOK)
    {
        node->owner = uid;
        node->group = gid;
    }
    lwext4_apply_times(node, path);

    struct ext4_inode  inode;
    struct ext4_sblock *sb = NULL;
    uint32_t ino = 0;
    memset(&inode, 0, sizeof(inode));
    if (ext4_raw_inode_fill(path, &ino, &inode) == EOK)
    {
        node->inode = ino;
        if (ext4_get_sblock(mount->mount_point, &sb) == EOK && sb != NULL)
        {
            node->size = (node->type & file_dir) ? 0 : ext4_inode_get_size(sb, &inode);
        }
    }
    else if (node->inode == 0)
    {
        node->inode = lwext4_next_anon_inode++;
    }

    if (node->type & file_symlink)
    {
        char   target[1024];
        size_t read_count = 0;
        if (ext4_readlink(path, target, sizeof(target) - 1, &read_count) == EOK)
        {
            target[read_count] = '\0';
            free(node->linkname);
            node->linkname = strdup(target);
            node->size     = read_count;
        }
    }

    return EOK;
}

static bool lwext4_should_prune_child(vfs_node_t node)
{
    if (node == NULL) return false;
    if (node->is_mount) return false;
    if (node->handle != NULL || node->refcount > 1) return false;
    return (node->type & (file_none | file_dir | file_symlink)) != 0;
}

static void lwext4_populate_dir(lwext4_mount_t *mount, ext4_dir *dir, vfs_node_t node)
{
    if (mount == NULL || dir == NULL || node == NULL) return;

    vfs_child_lock();
    list_foreach(node->child, child_node0)
    {
        vfs_node_t child = (vfs_node_t)child_node0->data;
        if (child != NULL) child->visited = false;
    }
    vfs_child_unlock();

    const ext4_direntry *entry = NULL;
    while ((entry = ext4_dir_entry_next(dir)) != NULL)
    {
        char name[256];
        if (entry->name_length == 0) continue;
        memcpy(name, entry->name, entry->name_length);
        name[entry->name_length] = '\0';
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;

        vfs_node_t child = NULL;
        vfs_child_lock();
        list_foreach(node->child, child_node0)
        {
            vfs_node_t existing = (vfs_node_t)child_node0->data;
            if (existing != NULL && existing->name != NULL && strcmp(existing->name, name) == 0)
            {
                child = existing;
                break;
            }
        }
        vfs_child_unlock();

        if (child == NULL)
        {
            child = vfs_child_append(node, name, NULL);
            if (child == NULL) continue;
        }

        child->visited = true;
        child->fsid    = lwext4fs_id;
        child->root    = node->root;
        child->dev     = node->root != NULL ? node->root->dev : node->dev;
        child->inode   = entry->inode != 0 ? entry->inode : lwext4_next_anon_inode++;
        child->type    = lwext4_vfs_type_from_direntry(entry->inode_type);
        child->blksz   = PAGE_SIZE;

        char       *owned_parent = NULL;
        const char *parent_path  = lwext4_path_from_node(node, &owned_parent);
        char       *child_path   = lwext4_join_path(parent_path, name);
        if (child_path != NULL) lwext4_apply_path(mount, child_path, child);
        free(child_path);
        free(owned_parent);
    }

    do
    {
        vfs_node_t stale = NULL;
        vfs_child_lock();
        list_foreach(node->child, child_node0)
        {
            vfs_node_t child = (vfs_node_t)child_node0->data;
            if (child != NULL && !child->visited && lwext4_should_prune_child(child))
            {
                stale = child;
                break;
            }
        }
        vfs_child_unlock();
        if (stale == NULL) break;
        vfs_detach(stale);
        vfs_free(stale);
    } while (true);

    lwext4_mark_dir_cache_valid(node);
}

static int lwext4_block_open(struct ext4_blockdev *bdev)
{
    if (bdev == NULL || bdev->bdif == NULL) return EINVAL;
    lwext4_mount_t *mount = (lwext4_mount_t *)bdev->bdif->p_user;
    if (mount == NULL || mount->device == NULL) return ENODEV;
    return EOK;
}

static bool lwext4_cache_covers(const lwext4_read_cache_t *cache, uint64_t block, uint32_t count)
{
    return cache->valid && block >= cache->start_block &&
           block + count <= cache->start_block + cache->block_count;
}

static void lwext4_cache_invalidate(lwext4_mount_t *mount)
{
    if (mount != NULL) mount->read_cache.valid = false;
}

static bool lwext4_cache_ensure(lwext4_mount_t *mount)
{
    if (mount == NULL) return false;

    lwext4_read_cache_t *cache = &mount->read_cache;
    if (cache->data != NULL) return true;

    size_t   pages = PADDING_UP(LWEXT4_READAHEAD_BYTES, PAGE_SIZE) / PAGE_SIZE;
    uint64_t phys  = alloc_frames(pages);
    if (phys == 0) return false;

    cache->data = (uint8_t *)phys_to_virt(phys);
    if (cache->data == NULL)
    {
        free_frames(phys, pages);
        return false;
    }

    cache->size_bytes = pages * PAGE_SIZE;
    cache->valid      = false;
    return true;
}

static int lwext4_cache_fill(lwext4_mount_t *mount, struct ext4_blockdev *bdev, uint64_t block, uint32_t min_count)
{
    if (!lwext4_cache_ensure(mount)) return ENOMEM;

    lwext4_read_cache_t *cache        = &mount->read_cache;
    uint32_t             cache_blocks = cache->size_bytes / bdev->bdif->ph_bsize;
    if (cache_blocks == 0 || block >= bdev->bdif->ph_bcnt) return EINVAL;

    uint64_t remaining = bdev->bdif->ph_bcnt - block;
    uint32_t to_read   = cache_blocks;
    if (to_read < min_count) to_read = min_count;
    if (to_read > remaining) to_read = (uint32_t)remaining;

    uint64_t bytes = (uint64_t)to_read * bdev->bdif->ph_bsize;
    if (bytes > (uint64_t)(size_t)-1) return EINVAL;

    size_t got = vfs_read(mount->device, cache->data, (size_t)(block * bdev->bdif->ph_bsize), (size_t)bytes);
    if (got != (size_t)bytes)
    {
        cache->valid = false;
        return EIO;
    }

    cache->start_block = block;
    cache->block_count = to_read;
    cache->valid       = true;
    return EOK;
}

static int lwext4_block_read(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id, uint32_t blk_cnt)
{
    if (bdev == NULL || bdev->bdif == NULL || buf == NULL) return EINVAL;
    lwext4_mount_t *mount = (lwext4_mount_t *)bdev->bdif->p_user;
    if (mount == NULL || mount->device == NULL) return ENODEV;

    uint64_t bytes = (uint64_t)blk_cnt * bdev->bdif->ph_bsize;
    if (bytes > (uint64_t)(size_t)-1) return EINVAL;

    if (blk_cnt <= 8 && lwext4_cache_ensure(mount))
    {
        lwext4_read_cache_t *cache = &mount->read_cache;
        if (!lwext4_cache_covers(cache, blk_id, blk_cnt))
        {
            int ret = lwext4_cache_fill(mount, bdev, blk_id, blk_cnt);
            if (ret != EOK) return ret;
        }

        size_t offset = (size_t)(blk_id - cache->start_block) * bdev->bdif->ph_bsize;
        memcpy(buf, cache->data + offset, (size_t)bytes);
        return EOK;
    }

    lwext4_cache_invalidate(mount);
    size_t got = vfs_read(mount->device, buf, (size_t)(blk_id * bdev->bdif->ph_bsize), (size_t)bytes);
    return got == (size_t)bytes ? EOK : EIO;
}

static int lwext4_block_write(struct ext4_blockdev *bdev, const void *buf, uint64_t blk_id, uint32_t blk_cnt)
{
    if (bdev == NULL || bdev->bdif == NULL || buf == NULL) return EINVAL;
    lwext4_mount_t *mount = (lwext4_mount_t *)bdev->bdif->p_user;
    if (mount == NULL || mount->device == NULL) return ENODEV;

    uint64_t bytes = (uint64_t)blk_cnt * bdev->bdif->ph_bsize;
    if (bytes > (uint64_t)(size_t)-1) return EINVAL;
    lwext4_cache_invalidate(mount);
    size_t wrote = vfs_write(mount->device, (void *)buf, (size_t)(blk_id * bdev->bdif->ph_bsize), (size_t)bytes);
    return wrote == (size_t)bytes ? EOK : EIO;
}

static int lwext4_block_close(struct ext4_blockdev *bdev)
{
    if (bdev == NULL || bdev->bdif == NULL) return EINVAL;
    return EOK;
}

static int lwext4_block_lock(struct ext4_blockdev *bdev)
{
    UNUSED(bdev);
    lwext4fs_lock();
    return EOK;
}

static int lwext4_block_unlock(struct ext4_blockdev *bdev)
{
    UNUSED(bdev);
    lwext4fs_unlock();
    return EOK;
}

static lwext4_mount_t *lwext4_mount_alloc(const char *src, vfs_node_t node)
{
    lwext4_mount_t *mount = (lwext4_mount_t *)calloc(1, sizeof(lwext4_mount_t));
    if (mount == NULL) return NULL;

    mount->device = vfs_open(src);
    if (mount->device == NULL) goto fail;

    mount->mount_point = vfs_get_fullpath(node);
    if (mount->mount_point == NULL) goto fail;

    mount->device_name = (char *)malloc(24);
    if (mount->device_name == NULL) goto fail;
    snprintf(mount->device_name, 24, "xjext%u", lwext4_next_mount_id++);

    mount->block_buffer = (uint8_t *)malloc(512);
    if (mount->block_buffer == NULL) goto fail;

    mount->iface.open     = lwext4_block_open;
    mount->iface.bread    = lwext4_block_read;
    mount->iface.bwrite   = lwext4_block_write;
    mount->iface.close    = lwext4_block_close;
    mount->iface.lock     = NULL;
    mount->iface.unlock   = NULL;
    mount->iface.ph_bsize = 512;
    mount->iface.ph_bcnt  = mount->device->size / 512;
    mount->iface.ph_bbuf  = mount->block_buffer;
    mount->iface.p_user   = mount;

    mount->blockdev.bdif        = &mount->iface;
    mount->blockdev.part_offset = 0;
    mount->blockdev.part_size   = mount->device->size;
    return mount;

fail:
    if (mount->device != NULL) vfs_close(mount->device);
    free(mount->mount_point);
    free(mount->device_name);
    free(mount->block_buffer);
    free(mount);
    return NULL;
}

static void lwext4_mount_free(lwext4_mount_t *mount)
{
    if (mount == NULL) return;
    if (mount->device != NULL) vfs_close(mount->device);
    if (mount->read_cache.data != NULL)
    {
        size_t pages = PADDING_UP(mount->read_cache.size_bytes, PAGE_SIZE) / PAGE_SIZE;
        free_frames((uint64_t)virt_to_phys((uint64_t)mount->read_cache.data), pages);
    }
    free(mount->mount_point);
    free(mount->device_name);
    free(mount->block_buffer);
    free(mount);
}

static lwext4_handle_t *lwext4_handle_alloc(lwext4_mount_t *mount, vfs_node_t node, const char *path)
{
    lwext4_handle_t *handle = (lwext4_handle_t *)calloc(1, sizeof(lwext4_handle_t));
    if (handle == NULL) return NULL;
    handle->mount = mount;
    handle->node  = node;
    handle->path  = strdup(path);
    if (handle->path == NULL)
    {
        free(handle);
        return NULL;
    }
    return handle;
}

static void lwext4_handle_free(lwext4_handle_t *handle)
{
    if (handle == NULL) return;
    free(handle->path);
    free(handle);
}

static errno_t lwext4fs_mount(const char *src, vfs_node_t node)
{
    lwext4_mount_t  *mount  = NULL;
    ext4_dir        *dir    = NULL;
    lwext4_handle_t *handle = NULL;
    uint64_t         now    = 0;
    int              ret    = EOK;

    if (is_virtual_fs(src)) return VFS_STATUS_FAILED;

    lwext4fs_lock();

    mount = lwext4_mount_alloc(src, node);
    if (mount == NULL)
    {
        lwext4fs_unlock();
        return VFS_STATUS_FAILED;
    }

    ret = ext4_device_register(&mount->blockdev, mount->device_name);
    if (ret != EOK) goto fail;

    ret = ext4_mount(mount->device_name, mount->mount_point, false);
    if (ret != EOK)
    {
        ext4_device_unregister(mount->device_name);
        goto fail;
    }

    dir = (ext4_dir *)malloc(sizeof(ext4_dir));
    if (dir == NULL) goto fail_umount;

    ret = ext4_dir_open(dir, mount->mount_point);
    if (ret != EOK)
    {
        free(dir);
        goto fail_umount;
    }

    handle = lwext4_handle_alloc(mount, node, mount->mount_point);
    if (handle == NULL)
    {
        ext4_dir_close(dir);
        free(dir);
        goto fail_umount;
    }

    handle->is_dir = true;
    handle->dir    = dir;

    node->fsid       = lwext4fs_id;
    node->type       = file_dir;
    node->inode      = 2;
    node->root       = node;
    node->dev        = mount->device->rdev;
    node->handle     = handle;
    node->blksz      = PAGE_SIZE;
    now              = realtime_ns();
    node->createtime = now;
    node->readtime   = now;
    node->writetime  = now;

    lwext4_apply_path(mount, mount->mount_point, node);
    lwext4_populate_dir(mount, dir, node);
    ext4_dir_entry_rewind(dir);

    lwext4fs_unlock();
    return VFS_STATUS_SUCCESS;

fail_umount:
    ext4_umount(mount->mount_point);
    ext4_device_unregister(mount->device_name);
fail:
    lwext4_mount_free(mount);
    lwext4fs_unlock();
    return VFS_STATUS_FAILED;
}

static void lwext4fs_unmount(void *root)
{
    lwext4fs_lock();

    lwext4_handle_t *handle = (lwext4_handle_t *)root;
    lwext4_mount_t  *mount  = handle != NULL ? handle->mount : NULL;

    if (handle != NULL)
    {
        if (handle->is_dir && handle->dir != NULL)
        {
            ext4_dir_close(handle->dir);
            free(handle->dir);
        }
        else if (handle->file != NULL)
        {
            ext4_fclose(handle->file);
            free(handle->file);
        }
        lwext4_handle_free(handle);
    }

    if (mount != NULL)
    {
        ext4_umount(mount->mount_point);
        ext4_device_unregister(mount->device_name);
        lwext4_mount_free(mount);
    }

    lwext4fs_unlock();
}

static void lwext4fs_open(void *parent, const char *name, vfs_node_t node)
{
    lwext4fs_lock();

    lwext4_handle_t *parent_handle = (lwext4_handle_t *)parent;
    if (parent_handle == NULL || parent_handle->mount == NULL || parent_handle->path == NULL)
    {
        lwext4fs_unlock();
        return;
    }

    char *path = lwext4_join_path(parent_handle->path, name);
    if (path == NULL)
    {
        lwext4fs_unlock();
        return;
    }

    if (lwext4_apply_path(parent_handle->mount, path, node) != EOK)
    {
        free(path);
        lwext4fs_unlock();
        return;
    }

    lwext4_handle_t *handle = lwext4_handle_alloc(parent_handle->mount, node, path);
    if (handle == NULL)
    {
        free(path);
        lwext4fs_unlock();
        return;
    }

    if (node->type & file_dir)
    {
        handle->is_dir = true;
        handle->dir    = (ext4_dir *)malloc(sizeof(ext4_dir));
        if (handle->dir == NULL || ext4_dir_open(handle->dir, path) != EOK)
        {
            free(handle->dir);
            lwext4_handle_free(handle);
            free(path);
            lwext4fs_unlock();
            return;
        }
        node->handle = handle;
        if (!lwext4_dir_cache_valid(node))
        {
            lwext4_populate_dir(parent_handle->mount, handle->dir, node);
        }
        ext4_dir_entry_rewind(handle->dir);
    }
    else if (node->type & file_symlink)
    {
        handle->is_symlink = true;
        node->handle       = handle;
    }
    else
    {
        handle->file = (ext4_file *)malloc(sizeof(ext4_file));
        if (handle->file == NULL || ext4_fopen2(handle->file, path, O_RDWR) != EOK)
        {
            free(handle->file);
            lwext4_handle_free(handle);
            free(path);
            lwext4fs_unlock();
            return;
        }
        node->size   = ext4_fsize(handle->file);
        node->handle = handle;
    }

    node->fsid = lwext4fs_id;
    node->root = parent_handle->mount != NULL ? node->parent->root : node->root;
    node->dev  = node->root != NULL ? node->root->dev : node->dev;

    free(path);
    lwext4fs_unlock();
}

static bool lwext4fs_close(void *current)
{
    lwext4fs_lock();

    lwext4_handle_t *handle = (lwext4_handle_t *)current;
    if (handle == NULL)
    {
        lwext4fs_unlock();
        return false;
    }

    if (handle->is_dir)
    {
        if (handle->dir != NULL)
        {
            ext4_dir_close(handle->dir);
            free(handle->dir);
        }
    }
    else if (!handle->is_symlink && handle->file != NULL)
    {
        ext4_fclose(handle->file);
        free(handle->file);
    }

    lwext4_handle_free(handle);
    lwext4fs_unlock();
    return true;
}

static size_t lwext4fs_read(void *file, void *addr, size_t offset, size_t size)
{
    lwext4fs_lock();

    lwext4_handle_t *handle = (lwext4_handle_t *)file;
    if (handle == NULL || handle->file == NULL || addr == NULL)
    {
        lwext4fs_unlock();
        return VFS_STATUS_FAILED;
    }

    int ret = ext4_fseek(handle->file, (int64_t)offset, SEEK_SET);
    if (ret != EOK)
    {
        lwext4fs_unlock();
        return VFS_STATUS_FAILED;
    }

    size_t count = 0;
    ret = ext4_fread(handle->file, addr, size, &count);
    lwext4fs_unlock();
    return ret == EOK ? count : (size_t)VFS_STATUS_FAILED;
}

static size_t lwext4fs_write(void *file, const void *addr, size_t offset, size_t size)
{
    lwext4fs_lock();

    lwext4_handle_t *handle = (lwext4_handle_t *)file;
    if (handle == NULL || handle->file == NULL || addr == NULL)
    {
        lwext4fs_unlock();
        return VFS_STATUS_FAILED;
    }

    int ret = ext4_fseek(handle->file, (int64_t)offset, SEEK_SET);
    if (ret != EOK)
    {
        lwext4fs_unlock();
        return VFS_STATUS_FAILED;
    }

    size_t count = 0;
    ret = ext4_fwrite(handle->file, addr, size, &count);
    if (ret == EOK) handle->file->fsize = ext4_fsize(handle->file);

    lwext4fs_unlock();
    return ret == EOK ? count : (size_t)VFS_STATUS_FAILED;
}

static size_t lwext4fs_readlink(vfs_node_t node, void *addr, size_t offset, size_t size)
{
    lwext4fs_lock();

    if (node == NULL || addr == NULL)
    {
        lwext4fs_unlock();
        return VFS_STATUS_FAILED;
    }

    char       *owned = NULL;
    const char *path  = lwext4_path_from_node(node, &owned);
    if (path == NULL)
    {
        lwext4fs_unlock();
        return VFS_STATUS_FAILED;
    }

    char   target[1024];
    size_t count = 0;
    int ret = ext4_readlink(path, target, sizeof(target), &count);
    free(owned);
    if (ret != EOK)
    {
        lwext4fs_unlock();
        return VFS_STATUS_FAILED;
    }

    if (offset >= count)
    {
        lwext4fs_unlock();
        return 0;
    }
    size_t copy_size = count - offset;
    if (copy_size > size) copy_size = size;
    memcpy(addr, target + offset, copy_size);

    lwext4fs_unlock();
    return copy_size;
}

static errno_t lwext4fs_mkdir(void *parent, const char *name, vfs_node_t node)
{
    lwext4fs_lock();

    lwext4_handle_t *parent_handle = (lwext4_handle_t *)parent;
    if (parent_handle == NULL)
    {
        lwext4fs_unlock();
        return -EINVAL;
    }

    char *path = lwext4_join_path(parent_handle->path, name);
    if (path == NULL)
    {
        lwext4fs_unlock();
        return -ENOMEM;
    }

    int ret = ext4_dir_mk(path);
    if (ret == EOK)
    {
        node->type = file_dir;
        lwext4_apply_path(parent_handle->mount, path, node);
        lwext4_invalidate_dir_cache(node->parent);
    }
    free(path);

    lwext4fs_unlock();
    return lwext4_err(ret);
}

static errno_t lwext4fs_mkfile(void *parent, const char *name, vfs_node_t node)
{
    lwext4fs_lock();

    lwext4_handle_t *parent_handle = (lwext4_handle_t *)parent;
    if (parent_handle == NULL)
    {
        lwext4fs_unlock();
        return -EINVAL;
    }

    char *path = lwext4_join_path(parent_handle->path, name);
    if (path == NULL)
    {
        lwext4fs_unlock();
        return -ENOMEM;
    }

    ext4_file fp;
    int ret = ext4_fopen2(&fp, path, O_CREAT | O_EXCL | O_RDWR);
    if (ret == EOK)
    {
        ext4_fclose(&fp);
        node->type = file_none;
        lwext4_apply_path(parent_handle->mount, path, node);
        lwext4_invalidate_dir_cache(node->parent);
    }
    free(path);

    lwext4fs_unlock();
    return lwext4_err(ret);
}

static errno_t lwext4fs_link(void *parent, const char *name, vfs_node_t node)
{
    lwext4fs_lock();

    lwext4_handle_t *parent_handle = (lwext4_handle_t *)parent;
    if (parent_handle == NULL)
    {
        lwext4fs_unlock();
        return -EINVAL;
    }

    char *path = lwext4_join_path(parent_handle->path, node->name);
    if (path == NULL)
    {
        lwext4fs_unlock();
        return -ENOMEM;
    }

    int ret = ext4_flink(name, path);
    if (ret == EOK)
    {
        lwext4_apply_path(parent_handle->mount, path, node);
        lwext4_invalidate_dir_cache(node->parent);
    }
    free(path);

    lwext4fs_unlock();
    return lwext4_err(ret);
}

static errno_t lwext4fs_symlink(void *parent, const char *name, vfs_node_t node)
{
    lwext4fs_lock();

    lwext4_handle_t *parent_handle = (lwext4_handle_t *)parent;
    if (parent_handle == NULL)
    {
        lwext4fs_unlock();
        return -EINVAL;
    }

    char *path = lwext4_join_path(parent_handle->path, node->name);
    if (path == NULL)
    {
        lwext4fs_unlock();
        return -ENOMEM;
    }

    int ret = ext4_fsymlink(name, path);
    if (ret == EOK)
    {
        node->type = file_symlink;
        lwext4_apply_path(parent_handle->mount, path, node);
        lwext4_invalidate_dir_cache(node->parent);
    }
    free(path);

    lwext4fs_unlock();
    return lwext4_err(ret);
}

static errno_t lwext4fs_stat(void *file, vfs_node_t node)
{
    lwext4fs_lock();

    lwext4_handle_t *handle = (lwext4_handle_t *)file;
    if (handle == NULL || node == NULL)
    {
        lwext4fs_unlock();
        return -EINVAL;
    }

    int ret = lwext4_apply_path(handle->mount, handle->path, node);
    if (ret == EOK && !handle->is_dir && !handle->is_symlink && handle->file != NULL)
    {
        node->size = ext4_fsize(handle->file);
    }

    lwext4fs_unlock();
    return ret;
}

static errno_t lwext4fs_ioctl(void *file, size_t req, void *arg)
{
    UNUSED(file, req, arg);
    return -EOPNOTSUPP;
}

static vfs_node_t lwext4fs_dup(vfs_node_t node)
{
    UNUSED(node);
    return NULL;
}

static errno_t lwext4fs_poll(void *file, size_t events)
{
    UNUSED(file, events);
    return -EOPNOTSUPP;
}

static void *lwext4fs_map(void *file, void *addr, size_t offset, size_t size, size_t prot, size_t flags)
{
    return general_map((vfs_read_t)lwext4fs_read, file, (uint64_t)addr, size, prot, flags, offset);
}

static void lwext4fs_resize(void *current, uint64_t size)
{
    lwext4fs_lock();

    lwext4_handle_t *handle = (lwext4_handle_t *)current;
    if (handle != NULL && handle->file != NULL)
    {
        ext4_ftruncate(handle->file, size);
        handle->file->fsize = ext4_fsize(handle->file);
    }

    lwext4fs_unlock();
}

static errno_t lwext4fs_delete(void *parent, vfs_node_t node)
{
    UNUSED(parent);
    lwext4fs_lock();

    if (node == NULL)
    {
        lwext4fs_unlock();
        return -EINVAL;
    }

    char       *owned = NULL;
    const char *path  = lwext4_path_from_node(node, &owned);
    if (path == NULL)
    {
        lwext4fs_unlock();
        return -EINVAL;
    }

    int ret = EOK;
    if (node->type & file_dir)
    {
        ext4_dir dir;
        ret = ext4_dir_open(&dir, path);
        if (ret == EOK)
        {
            const ext4_direntry *entry = NULL;
            while ((entry = ext4_dir_entry_next(&dir)) != NULL)
            {
                char name[256];
                if (entry->name_length == 0) continue;
                memcpy(name, entry->name, entry->name_length);
                name[entry->name_length] = '\0';
                if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0)
                {
                    ret = ENOTEMPTY;
                    break;
                }
            }
            ext4_dir_close(&dir);
        }
        if (ret == EOK) ret = ext4_dir_rm(path);
    }
    else
    {
        ret = ext4_fremove(path);
    }
    if (ret == EOK) lwext4_invalidate_parent_dir_cache(node);
    free(owned);

    lwext4fs_unlock();
    return lwext4_err(ret);
}

static errno_t lwext4fs_rename(void *current, const char *nw)
{
    lwext4fs_lock();

    lwext4_handle_t *handle = (lwext4_handle_t *)current;
    if (handle == NULL || handle->path == NULL || nw == NULL)
    {
        lwext4fs_unlock();
        return -EINVAL;
    }

    int ret = ext4_frename(handle->path, nw);
    if (ret == EOK)
    {
        vfs_node_t node = handle->node;
        lwext4_invalidate_parent_dir_cache(node);
        char *new_path = strdup(nw);
        if (new_path != NULL)
        {
            free(handle->path);
            handle->path = new_path;
        }
    }

    lwext4fs_unlock();
    return lwext4_err(ret);
}

static struct vfs_callback lwext4fs_callbacks = {
    .mount    = lwext4fs_mount,
    .unmount  = lwext4fs_unmount,
    .open     = lwext4fs_open,
    .close    = (vfs_close_t)lwext4fs_close,
    .read     = (vfs_read_t)lwext4fs_read,
    .write    = (vfs_write_t)lwext4fs_write,
    .readlink = lwext4fs_readlink,
    .mkdir    = lwext4fs_mkdir,
    .mkfile   = lwext4fs_mkfile,
    .link     = lwext4fs_link,
    .symlink  = lwext4fs_symlink,
    .stat     = lwext4fs_stat,
    .ioctl    = lwext4fs_ioctl,
    .dup      = lwext4fs_dup,
    .poll     = lwext4fs_poll,
    .map      = (vfs_mapfile_t)lwext4fs_map,
    .resize   = lwext4fs_resize,
    .del      = lwext4fs_delete,
    .rename   = lwext4fs_rename,
};

void lwext4fs_init()
{
    mutex_create(&lwext4_lock, true);
    lwext4fs_id = vfs_regist("ext4", &lwext4fs_callbacks, 0, 0xef53);
    if (lwext4fs_id == VFS_STATUS_FAILED) write_serial_fmt("Failed to register lwext4 filesystem\n");
}
