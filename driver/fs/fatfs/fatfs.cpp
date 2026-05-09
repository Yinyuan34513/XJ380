#define ALL_IMPLEMENTATION
#include "errno.h"
#include "fs/fatfs/ff.h"
#include "krlibc.h"
#include "mutex.h"
#include <fs/vfs/vfs.h>
#include <syscall/syscall.h>

static FATFS volume[10];
static int   fatfs_id = 0;
static const size_t FATFS_FASTSEEK_MIN_BYTES = 0x9000UL;
extern bool ahci_is_qemu_environment();
typedef struct file {
    char *path;
    void *handle;
    bool  is_dir;
} *file_t;

vfs_node_t drive_number_mapping[10] = {NULL};

static int alloc_number() {
    for (int i = 0; i < 10; i++)
        if (drive_number_mapping[i] == NULL) return i;
    write_serial_fmt("No available drive number\n");
    return -1;
}

vfs_node_t fatfs_get_node_by_number(int number) {
    if (number < 0 || number >= 10) return NULL;
    return drive_number_mapping[number];
}
 
static mutex_t fatfs_operate_lock;

static inline void fatfs_lock() {
    mutex_lock(&fatfs_operate_lock);
}

static inline void fatfs_unlock() {
    mutex_unlock(&fatfs_operate_lock);
}

static u32 *fatfs_try_build_clmt(FIL *fp, size_t request_size) {
    if (!ahci_is_qemu_environment()) {
        return NULL;
    }

    if (fp == NULL || fp->obj.fs == NULL || fp->obj.sclust < 2 || request_size < FATFS_FASTSEEK_MIN_BYTES) {
        return NULL;
    }

    size_t bytes_per_cluster = (size_t)fp->obj.fs->csize * FF_MAX_SS;
    if (bytes_per_cluster == 0) {
        return NULL;
    }

    size_t cluster_count = ((size_t)fp->obj.objsize + bytes_per_cluster - 1) / bytes_per_cluster;
    if (cluster_count == 0) {
        cluster_count = 1;
    }

    if (cluster_count > (((size_t)-1) - 4) / 2) {
        return NULL;
    }

    size_t table_items = cluster_count * 2 + 4;
    if (table_items > 0xFFFFFFFFu) {
        return NULL;
    }

    u32 *clmt = (u32 *)malloc(table_items * sizeof(u32));
    if (clmt == NULL) {
        return NULL;
    }

    clmt[0] = (u32)table_items;
    return clmt;
}

int fatfs_mkdir(void *parent, const char *name, vfs_node_t node) {
    fatfs_lock();

    file_t p        = (file_t)parent;
    char  *new_path = (char*)malloc(strlen(p->path) + strlen((char *)name) + 1 + 1);
    sprintf(new_path, "%s/%s", p->path, name);
    FRESULT res = f_mkdir(new_path);
    free(new_path);
    if (res != FR_OK) 
    {
        fatfs_unlock();
        return -1;
    }
    fatfs_unlock();
    return 0;
}

int fatfs_mkfile(void *parent, const char *name, vfs_node_t node) {
    fatfs_lock();

    file_t p        = (file_t)parent;
    char  *new_path = (char*)malloc(strlen(p->path) + strlen((char *)name) + 1 + 1);
    sprintf(new_path, "%s/%s", p->path, name);
    FIL     fp;
    FRESULT res = f_open(&fp, new_path, FA_CREATE_NEW);
    f_close(&fp);
    free(new_path);
    if (res != FR_OK) 
    {
        fatfs_unlock();
        return -1;
    }
    fatfs_unlock();
    return 0;
}

size_t fatfs_readfile(file_t file, void *addr, size_t offset, size_t size) {
    fatfs_lock();
    
    if (file == NULL || addr == NULL) 
    {
        fatfs_unlock();
        return -1;
    }
    FIL    *fp        = (FIL *)file->handle;
    FRESULT res       = FR_OK;
    u32    *temp_clmt = NULL;

    if (fp == NULL) 
    {
        fatfs_unlock();
        return -1;
    }

    if (fp->cltbl == NULL) {
        temp_clmt = fatfs_try_build_clmt(fp, size);
        if (temp_clmt != NULL) {
            fp->cltbl = temp_clmt;
            res       = f_lseek(fp, CREATE_LINKMAP);
            if (res != FR_OK) {
                fp->cltbl = NULL;
                free(temp_clmt);
                temp_clmt = NULL;
                res       = FR_OK;
            }
        }
    }

    res = f_lseek(fp, offset);
    if (res != FR_OK) 
    {
        if (temp_clmt != NULL) {
            fp->cltbl = NULL;
            free(temp_clmt);
        }
        fatfs_unlock();
        return -1;
    }
    uint32_t n;
    res = f_read(fp, addr, size, &n);
    if (temp_clmt != NULL) {
        fp->cltbl = NULL;
        free(temp_clmt);
    }
    if (res != FR_OK) 
    {
        fatfs_unlock();
        return -1;
    }
    fatfs_unlock();
    return n;
}

size_t fatfs_writefile(file_t file, const void *addr, size_t offset, size_t size) {
    fatfs_lock();
    
    if (file == NULL || addr == NULL) 
    {
        fatfs_unlock();
        return -1;
    }
    FRESULT res;
    res = f_lseek((FIL*)file->handle, offset);
    if (res != FR_OK) 
    {
        fatfs_unlock();
        return -1;
    }
    uint32_t n;
    res = f_write((FIL*)file->handle, addr, size, &n);
    if (res != FR_OK) 
    {
        fatfs_unlock();
        return -1;
    }
    f_sync((FIL*)file->handle);
    
    fatfs_unlock();
    return n;
}

static uint64_t ino = 2;

static bool fatfs_should_prune_child(vfs_node_t node) {
    if (node == NULL) return false;
    if (node->is_mount) return false;
    return (node->type & (file_none | file_dir)) != 0;
}

void fatfs_open(void *parent, const char *name, vfs_node_t node) {
    fatfs_lock();
    
    file_t p        = (file_t)parent;
    char  *new_path = (char*)malloc(strlen(p->path) + strlen((char *)name) + 1 + 1);
    file_t nw      = (file_t)malloc(sizeof(struct file));
    sprintf(new_path, "%s/%s", p->path, name);
    void   *fp = NULL;
    FILINFO fno;
    FRESULT res = f_stat(new_path, &fno);
    if (fno.fattrib & AM_DIR) {
        // node.
        node->type = file_dir;
        nw->is_dir = true;
        fp         = malloc(sizeof(DIR));
        res        = f_opendir((DIR*)fp, new_path);
        for (;;) {
            // 读取目录下的内容，再读会自动读下一个文件
            res = f_readdir((DIR*)fp, &fno);
            // 为空时表示所有项目读取完毕，跳出
            if (res != FR_OK || fno.fname[0] == 0) break;
            bool has_equre = false;
            vfs_child_lock();
            list_foreach(node->child, child_node0) {
                vfs_node_t e_child = (vfs_node_t)child_node0->data;
                if (strcmp(e_child->name, fno.fname) == 0) {
                    has_equre = true;
                    break;
                }
            }
            vfs_child_unlock();
            if (has_equre) continue;
            vfs_node_t child_node = vfs_child_append(node, fno.fname, NULL);
            child_node->type      = ((fno.fattrib & AM_DIR) != 0) ? file_dir : file_none;
            child_node->inode     = ino++;
            child_node->size      = fno.fsize;
            child_node->dev       = child_node->root->dev;
        }
        if (node->inode == 0) node->inode = ino++;
        node->blksz = PAGE_SIZE;
    } else {
        node->type = file_none;
        nw->is_dir = false;
        fp         = malloc(sizeof(FIL));
        res        = f_open((FIL*)fp, new_path, FA_READ | FA_WRITE);
        if (node->inode == 0) node->inode = ino++;
        node->size  = f_size((FIL *)fp);
        node->blksz = PAGE_SIZE;
    }
    node->dev    = node->root->dev;
    nw->handle  = fp;
    nw->path    = new_path;
    node->handle = nw;

    fatfs_unlock();
}

bool fatfs_close(file_t handle) {
    fatfs_lock();
    
    if (handle == NULL) {
        fatfs_unlock();
        return false;
    }

    FRESULT res;
    if (handle->is_dir) {
        res = f_closedir((DIR*)handle->handle);
    } else {
        res = f_close((FIL*)handle->handle);
    }
    if (res != FR_OK) 
    {
        fatfs_unlock();
        return false;
    }
    free(handle->path);
    free(handle->handle);
    free(handle);

    fatfs_unlock();
    return true;
}

errno_t fatfs_mount(const char *src, vfs_node_t node) {
    fatfs_lock();
    
    // if (node == rootdir) return -1; // 不支持fatfs作为rootfs
    if (is_virtual_fs(src)) 
    {   
        fatfs_unlock();
        return VFS_STATUS_FAILED;
    }

    int drive                   = alloc_number();
    drive_number_mapping[drive] = vfs_open(src);
    if (drive_number_mapping[drive] == NULL) 
    {   
        fatfs_unlock();
        return VFS_STATUS_FAILED;
    }
    node->dev  = drive_number_mapping[drive]->rdev;
    char *path = (char*)malloc(3);
    sprintf(path, "%d:", drive);
    FRESULT r = f_mount(&volume[drive], path, 1);
    if (r != FR_OK) {
        vfs_close(drive_number_mapping[drive]);
        drive_number_mapping[drive] = NULL;
        free(path);
        fatfs_unlock();
        return -1;
    }
    file_t f = (file_t)malloc(sizeof(struct file));
    f->path  = path;
    f->is_dir = true;
    DIR *h   = (DIR*)malloc(sizeof(DIR));
    f_opendir(h, path);
    f->handle   = h;
    node->fsid  = fatfs_id;
    node->inode = 1;
    node->root  = node;

    FILINFO fno;
    FRESULT res;
    for (;;) {
        // 读取目录下的内容，再读会自动读下一个文件
        res = f_readdir(h, &fno);
        // 为空时表示所有项目读取完毕，跳出
        if (res != FR_OK || fno.fname[0] == 0) break;
        vfs_node_t exist = NULL;
        vfs_child_lock();
        list_foreach(node->child, child_node0) {
            vfs_node_t e_child = (vfs_node_t)child_node0->data;
            if (strcmp(e_child->name, fno.fname) == 0) {
                exist = e_child;
                break;
            }
        }
        vfs_child_unlock();
        if (exist) {
            exist->visited = true;
            continue;
        }
        vfs_node_t child_node = vfs_child_append(node, fno.fname, NULL);
        child_node->type      = ((fno.fattrib & AM_DIR) != 0) ? file_dir : file_none;
        child_node->inode     = ino++;
        child_node->size      = fno.fsize;
        child_node->visited   = true;
        child_node->dev       = child_node->root->dev;
    }
    // node->inode  = ino++;
    node->handle = f;
    // node->blksz  = DEFAULT_PAGE_SIZE;
    
    fatfs_unlock();
    return VFS_STATUS_SUCCESS;
}

void fatfs_unmount(void *root) {
    fatfs_lock();
    
    file_t f                     = (file_t)root;
    int    number                = f->path[0] - '0';
    drive_number_mapping[number] = NULL;
    f_closedir((DIR*)f->handle);
    f_unmount(f->path);
    free(f->path);
    free(f->handle);
    free(f);

    fatfs_unlock();
}

int fatfs_stat(void *handle, vfs_node_t node) {
    fatfs_lock();
    
    file_t  f = (file_t)handle;
    FILINFO fno;
    FRESULT res = f_stat(f->path, &fno);
    if (res != FR_OK) 
    {
        fatfs_unlock();
        return -1;
    }
    node->dev = node->root->dev;
    if (fno.fattrib & AM_DIR) {
        node->type = file_dir;
        DIR *fp    = (DIR*)malloc(sizeof(DIR));
        res        = f_opendir(fp, f->path);
        vfs_child_lock();
        list_foreach(node->child, child_node0) {
            vfs_node_t e_child = (vfs_node_t)child_node0->data;
            e_child->visited   = false;
        }
        vfs_child_unlock();

        for (;;) {
            // 读取目录下的内容，再读会自动读下一个文件
            res = f_readdir(fp, &fno);
            // 为空时表示所有项目读取完毕，跳出
            if (res != FR_OK || fno.fname[0] == 0) break;
            vfs_node_t exist = NULL;
            vfs_child_lock();
            list_foreach(node->child, child_node0) {
                vfs_node_t e_child = (vfs_node_t)child_node0->data;
                if (strcmp(e_child->name, fno.fname) == 0) {
                    exist = e_child;
                    break;
                }
            }
            vfs_child_unlock();
            if (exist) {
                exist->visited = true;
                continue;
            }
            vfs_node_t child_node = vfs_child_append(node, fno.fname, NULL);
            child_node->type      = ((fno.fattrib & AM_DIR) != 0) ? file_dir : file_none;
            child_node->inode     = ino++;
            child_node->size      = fno.fsize;
            child_node->visited   = true;
            child_node->dev       = child_node->root->dev;
        }
        free(fp);
        do {
            vfs_node_t exist = NULL;
            vfs_child_lock();
            list_foreach(node->child, child_node0) {
                vfs_node_t e_child = (vfs_node_t)child_node0->data;
                if (!e_child->visited && fatfs_should_prune_child(e_child)) {
                    exist = e_child;
                    break;
                }
            }
            if (exist != NULL) node->child = list_delete(node->child, exist);//虽然但是，保险点吧
            vfs_child_unlock();
            if (exist == NULL) break;
            vfs_free(exist);
        } while (true);
    } else {
        node->type = file_none;
        node->size = fno.fsize;
        // node->createtime = fno.ftime
    }
    
    fatfs_unlock();
    return 0;
}

int fatfs_delete(file_t parent, vfs_node_t node) {
    fatfs_lock();
    
    file_t file = (file_t)node->handle;

    FRESULT res = f_unlink(file->path);
    
    fatfs_unlock();

    if (res == FR_DENIED) return -ENOTEMPTY;
    if (res != FR_OK) return -ENOENT;
    return VFS_STATUS_SUCCESS;
}

int fatfs_rename(file_t file, const char *nw) {
    if (file == NULL || file->path == NULL || nw == NULL) {
        return VFS_STATUS_FAILED;
    }

    const char *target_path = nw;
    char       *fat_target  = NULL;
    if (nw[0] == '/' && file->path[0] != '\0' && file->path[1] == ':') {
        fat_target = (char *)malloc(strlen(nw) + 3);
        if (fat_target == NULL) {
            return VFS_STATUS_FAILED;
        }
        fat_target[0] = file->path[0];
        fat_target[1] = ':';
        strcpy(fat_target + 2, nw);
        target_path = fat_target;
    }

    fatfs_lock();
    
    FRESULT res = f_rename((const char *)file->path, target_path);

    fatfs_unlock();
    free(fat_target);

    if (res != FR_OK) return -1;
    return 0;
}

static void fatfs_resize(file_t file, uint64_t size) {
    fatfs_lock();

    if (file == NULL || file->is_dir || file->handle == NULL) {
        fatfs_unlock();
        return;
    }

    FIL *fp = (FIL *)file->handle;
    if (f_lseek(fp, (FSIZE_t)size) == FR_OK) {
        f_truncate(fp);
        f_sync(fp);
    }

    fatfs_unlock();
}

int fatfs_ioctl(void *file, size_t cmd, void *arg) {
    return -EOPNOTSUPP;
}

int fatfs_poll(void *file, size_t events) {
    return -EOPNOTSUPP;
}

void *fatfs_map(void *file, void *addr, size_t offset, size_t size, size_t prot, size_t flags) {
    return general_map((vfs_read_t)fatfs_readfile, file, (uint64_t)addr, size, prot, flags, offset);
}

vfs_node_t fatfs_dup(vfs_node_t node) {
    vfs_node_t copy   = vfs_node_alloc(node->parent, node->name);
    file_t     src    = (file_t)node->handle;
    file_t     tar    = (file_t)malloc(sizeof(struct file));
    tar->path         = strdup(src->path);
    tar->handle       = src->handle;
    tar->is_dir       = src->is_dir;
    copy->handle      = tar;
    copy->type        = node->type;
    copy->size        = node->size;
    copy->linkname    = node->linkname == NULL ? NULL : strdup(node->linkname);
    copy->flags       = node->flags;
    copy->permissions = node->permissions;
    copy->owner       = node->owner;
    copy->child       = node->child;
    copy->realsize    = node->realsize;
    copy->inode       = node->inode;
    copy->dev         = node->dev;
    return copy;
}

static int dummy() {
    return -ENOSYS;
}

errno_t fatfs_statfs(vfs_node_t node, struct statfs *buf)
{
    if (node == NULL || buf == NULL) return -EINVAL;

    fatfs_lock();

    file_t file = (file_t)node->handle;
    const char *path = (file != NULL && file->path != NULL) ? file->path : "0:";
    u32    free_clusters = 0;
    FATFS *fs            = NULL;
    FRESULT res          = f_getfree(path, &free_clusters, &fs);
    if (res != FR_OK || fs == NULL)
    {
        fatfs_unlock();
        return -EIO;
    }

    uint64_t sector_size = FF_MAX_SS;
    uint64_t block_size  = (uint64_t)fs->csize * sector_size;
    if (block_size == 0) block_size = PAGE_SIZE;

    memset(buf, 0, sizeof(*buf));
    buf->f_type    = 0x4d44;
    buf->f_bsize   = block_size;
    buf->f_frsize  = block_size;
    buf->f_blocks  = fs->n_fatent > 2 ? fs->n_fatent - 2 : 0;
    buf->f_bfree   = free_clusters;
    buf->f_bavail  = free_clusters;
    buf->f_files   = 1024 * 1024;
    buf->f_ffree   = 1024 * 1024;
    buf->f_namelen = 255;

    fatfs_unlock();
    return EOK;
}

static struct vfs_callback fatfs_callbacks = {
    .mount    = fatfs_mount,
    .unmount  = fatfs_unmount,
    .open     = fatfs_open,
    .close    = (vfs_close_t)fatfs_close,
    .read     = (vfs_read_t)fatfs_readfile,
    .write    = (vfs_write_t)fatfs_writefile,
    .readlink = (vfs_readlink_t)dummy,
    .mkdir    = fatfs_mkdir,
    .mkfile   = fatfs_mkfile,
    .link     = (vfs_mk_t)dummy,
    .symlink  = (vfs_mk_t)dummy,
    .stat     = fatfs_stat,
    .ioctl    = fatfs_ioctl,
    .dup      = fatfs_dup,
    .poll     = fatfs_poll,
    .map      = (vfs_mapfile_t)fatfs_map,
    .resize   = (vfs_resize_t)fatfs_resize,
    .del      = (vfs_del_t)fatfs_delete,
    .rename   = (vfs_rename_t)fatfs_rename,
};

void fatfs_init() {
    mutex_create(&fatfs_operate_lock, true);
    fatfs_id = vfs_regist("fatfs", &fatfs_callbacks, 0, 0x4d44);
    if (fatfs_id == VFS_STATUS_FAILED) { write_serial_fmt("Failed to register fat filesystem\n"); }
}
