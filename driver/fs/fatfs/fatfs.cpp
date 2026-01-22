#define ALL_IMPLEMENTATION
#include "errno.h"
#include "fs/fatfs/ff.h"
#include "krlibc.h"
#include <fs/vfs/vfs.h>

static FATFS volume[10];
static int   fatfs_id = 0;
typedef struct file {
    char *path;
    void *handle;
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
 
spin_t fatfs_operate_lock = SPIN_INIT;

int fatfs_mkdir(void *parent, const char *name, vfs_node_t node) {
    spin_lock(&fatfs_operate_lock);

    file_t p        = (file_t)parent;
    char  *new_path = (char*)malloc(strlen(p->path) + strlen((char *)name) + 1 + 1);
    sprintf(new_path, "%s/%s", p->path, name);
    FRESULT res = f_mkdir(new_path);
    free(new_path);
    if (res != FR_OK) 
    {
        spin_unlock(&fatfs_operate_lock);
        return -1;
    }
    spin_unlock(&fatfs_operate_lock);
    return 0;
}

int fatfs_mkfile(void *parent, const char *name, vfs_node_t node) {
    spin_lock(&fatfs_operate_lock);

    file_t p        = (file_t)parent;
    char  *new_path = (char*)malloc(strlen(p->path) + strlen((char *)name) + 1 + 1);
    sprintf(new_path, "%s/%s", p->path, name);
    FIL     fp;
    FRESULT res = f_open(&fp, new_path, FA_CREATE_NEW);
    f_close(&fp);
    free(new_path);
    if (res != FR_OK) 
    {
        spin_unlock(&fatfs_operate_lock);
        return -1;
    }
    spin_unlock(&fatfs_operate_lock);
    return 0;
}

size_t fatfs_readfile(file_t file, void *addr, size_t offset, size_t size) {
    spin_lock(&fatfs_operate_lock);
    
    if (file == NULL || addr == NULL) 
    {
        spin_unlock(&fatfs_operate_lock);
        return -1;
    }
    FRESULT res;
    res = f_lseek((FIL*)file->handle, offset);
    if (res != FR_OK) 
    {
        spin_unlock(&fatfs_operate_lock);
        return -1;
    }
    uint32_t n;
    res = f_read((FIL*)file->handle, addr, size, &n);
    if (res != FR_OK) 
    {
        spin_unlock(&fatfs_operate_lock);
        return -1;
    }
    spin_unlock(&fatfs_operate_lock);
    return n;
}

size_t fatfs_writefile(file_t file, const void *addr, size_t offset, size_t size) {
    spin_lock(&fatfs_operate_lock);
    
    if (file == NULL || addr == NULL) 
    {
        spin_unlock(&fatfs_operate_lock);
        return -1;
    }
    FRESULT res;
    res = f_lseek((FIL*)file->handle, offset);
    if (res != FR_OK) 
    {
        spin_unlock(&fatfs_operate_lock);
        return -1;
    }
    uint32_t n;
    res = f_write((FIL*)file->handle, addr, size, &n);
    if (res != FR_OK) 
    {
        spin_unlock(&fatfs_operate_lock);
        return -1;
    }
    f_sync((FIL*)file->handle);
    
    spin_unlock(&fatfs_operate_lock);
    return n;
}

static uint64_t ino = 2;

void fatfs_open(void *parent, const char *name, vfs_node_t node) {
    spin_lock(&fatfs_operate_lock);
    
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
        fp         = malloc(sizeof(DIR));
        res        = f_opendir((DIR*)fp, new_path);
        for (;;) {
            // 读取目录下的内容，再读会自动读下一个文件
            res = f_readdir((DIR*)fp, &fno);
            // 为空时表示所有项目读取完毕，跳出
            if (res != FR_OK || fno.fname[0] == 0) break;
            bool has_equre = false;
            list_foreach(node->child, child_node0) {
                vfs_node_t e_child = (vfs_node_t)child_node0->data;
                if (strcmp(e_child->name, fno.fname) == 0) {
                    has_equre = true;
                    break;
                }
            }
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

    spin_unlock(&fatfs_operate_lock);
}

bool fatfs_close(file_t handle) {
    spin_lock(&fatfs_operate_lock);
    
    FILINFO fno;
    FRESULT res = f_stat(handle->path, &fno);
    if (fno.fattrib & AM_DIR) {
        res = f_closedir((DIR*)handle->handle);
    } else {
        res = f_close((FIL*)handle->handle);
    }
    if (res != FR_OK) 
    {
        spin_unlock(&fatfs_operate_lock);
        return false;
    }
    free(handle->path);
    free(handle->handle);
    free(handle);

    spin_unlock(&fatfs_operate_lock);
    return true;
}

errno_t fatfs_mount(const char *src, vfs_node_t node) {
    spin_lock(&fatfs_operate_lock);
    
    // if (node == rootdir) return -1; // 不支持fatfs作为rootfs
    if (is_virtual_fs(src)) 
    {   
        spin_unlock(&fatfs_operate_lock);
        return VFS_STATUS_FAILED;
    }

    int drive                   = alloc_number();
    drive_number_mapping[drive] = vfs_open(src);
    if (drive_number_mapping[drive] == NULL) 
    {   
        spin_unlock(&fatfs_operate_lock);
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
        spin_unlock(&fatfs_operate_lock);
        return -1;
    }
    file_t f = (file_t)malloc(sizeof(struct file));
    f->path  = path;
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
        list_foreach(node->child, child_node0) {
            vfs_node_t e_child = (vfs_node_t)child_node0->data;
            if (strcmp(e_child->name, fno.fname) == 0) {
                exist = e_child;
                break;
            }
        }
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
    
    spin_unlock(&fatfs_operate_lock);
    return VFS_STATUS_SUCCESS;
}

void fatfs_unmount(void *root) {
    spin_lock(&fatfs_operate_lock);
    
    file_t f                     = (file_t)root;
    int    number                = f->path[0] - '0';
    drive_number_mapping[number] = NULL;
    f_closedir((DIR*)f->handle);
    f_unmount(f->path);
    free(f->path);
    free(f->handle);
    free(f);

    spin_unlock(&fatfs_operate_lock);
}

int fatfs_stat(void *handle, vfs_node_t node) {
    spin_lock(&fatfs_operate_lock);
    
    file_t  f = (file_t)handle;
    FILINFO fno;
    FRESULT res = f_stat(f->path, &fno);
    if (res != FR_OK) 
    {
        spin_unlock(&fatfs_operate_lock);
        return -1;
    }
    node->dev = node->root->dev;
    if (fno.fattrib & AM_DIR) {
        node->type = file_dir;
        DIR *fp    = (DIR*)malloc(sizeof(DIR));
        res        = f_opendir(fp, f->path);
        list_foreach(node->child, child_node0) {
            vfs_node_t e_child = (vfs_node_t)child_node0->data;
            e_child->visited   = false;
        }

        for (;;) {
            // 读取目录下的内容，再读会自动读下一个文件
            res = f_readdir(fp, &fno);
            // 为空时表示所有项目读取完毕，跳出
            if (res != FR_OK || fno.fname[0] == 0) break;
            vfs_node_t exist = NULL;
            list_foreach(node->child, child_node0) {
                vfs_node_t e_child = (vfs_node_t)child_node0->data;
                if (strcmp(e_child->name, fno.fname) == 0) {
                    exist = e_child;
                    break;
                }
            }
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
            list_foreach(node->child, child_node0) {
                vfs_node_t e_child = (vfs_node_t)child_node0->data;
                if (!e_child->visited) {
                    exist = e_child;
                    break;
                }
            }
            if (exist == NULL) break;
            list_delete(node->child, exist);
            vfs_free(exist);
        } while (true);
    } else {
        node->type = file_none;
        node->size = fno.fsize;
        // node->createtime = fno.ftime
    }
    
    spin_unlock(&fatfs_operate_lock);
    return 0;
}

int fatfs_delete(file_t parent, vfs_node_t node) {
    spin_lock(&fatfs_operate_lock);
    
    file_t file = (file_t)node->handle;

    FRESULT res = f_unlink(file->path);
    
    spin_unlock(&fatfs_operate_lock);

    if (res == FR_DENIED) return -ENOTEMPTY;
    if (res != FR_OK) return -ENOENT;
    return VFS_STATUS_SUCCESS;
}

int fatfs_rename(file_t file, const char *nw) {
    spin_lock(&fatfs_operate_lock);
    
    FRESULT res = f_rename((const char *)file->path, nw);

    spin_unlock(&fatfs_operate_lock);

    if (res != FR_OK) return -1;
    return 0;
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
    .del   = (vfs_del_t)fatfs_delete,
    .rename   = (vfs_rename_t)fatfs_rename,
    .map      = (vfs_mapfile_t)fatfs_map,
    .stat     = fatfs_stat,
    .ioctl    = fatfs_ioctl,
    .poll     = fatfs_poll,
    .dup      = fatfs_dup,
};

void fatfs_init() {
    fatfs_id = vfs_regist("fatfs", &fatfs_callbacks, 0, 0x4d44);
    if (fatfs_id == VFS_STATUS_FAILED) { write_serial_fmt("Failed to register fat filesystem\n"); }
}
