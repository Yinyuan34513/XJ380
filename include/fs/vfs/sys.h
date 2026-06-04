#pragma once

#include <fs/vfs/vfs.h>

typedef struct sysfs_handle
{
    vfs_node_t node;
    void      *private_data;
    char       name[128];
    char      *content;
    size_t     size;
    size_t     capacity;
} sysfs_handle_t;

void       sysfs_init();
bool       sysfs_is_ready();
vfs_node_t sysfs_ensure_dir(const char *path);
vfs_node_t sysfs_ensure_file(const char *path);
vfs_node_t sysfs_ensure_symlink(const char *path, const char *target);
vfs_node_t sysfs_child_append(vfs_node_t parent, const char *name, bool is_dir);
vfs_node_t sysfs_child_append_symlink(vfs_node_t parent, const char *name, const char *target);
int        sysfs_write_node(vfs_node_t node, const void *buf, size_t len, size_t offset);
char      *sysfs_node_path(vfs_node_t node);
vfs_node_t sysfs_regist_dev(char type,
                            int major,
                            int minor,
                            const char *real_device_path,
                            const char *dev_name,
                            const char *other_uevent_content,
                            const char *subsystem_path,
                            const char *class_path,
                            const char *class_name,
                            const char *parent_device_path);
void       sysfs_publish_kernel_device(const char *dev_path, const char *dev_name, char type, int major, int minor);
void       sysfs_publish_all_devices();
