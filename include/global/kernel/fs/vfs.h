#pragma once

#define MAX_VNODE_NUM 4096
#define MAX_CHILD_VNODE_NUM 512

enum
{
    NODE_TYPE_NONE,
    NODE_TYPE_FILE,
    NODE_TYPE_DIR,
};

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

struct timespec
{
    int64_t tv_sec;
    int64_t tv_nsec;
};

struct stat
{
    uint64_t dev_id;
    uint64_t inode;
    uint64_t nlink;
    uint32_t mode;
    int32_t uid;
    int32_t gid;
    int64_t rdev;
    int64_t size;
    int64_t block_size;
    uint64_t blocks;
    struct timespec atime;
    struct timespec mtime;
    struct timespec ctime;

    int8_t _pad[24];
};

struct v_node;

typedef v_node VNode;

typedef struct
{
    // For kernel
    int (*lookup)(VNode *node, VNode *search_node);
    int (*super)(int drive, VNode *result);
    int (*write_node)(VNode *node);
    // For user
    int (*open)(VNode *parent, const char *name, VNode *node);
    void (*close)(VNode *current);
    int (*resize)(VNode *current, uint64_t size);
    int (*read)(VNode *file, uint8_t *addr, size_t offset, size_t size);
    int (*write)(VNode *file, uint8_t *addr, size_t offset, size_t size);
    int (*lseek)(VNode *node, uint64_t offset, uint64_t origin);
    int (*stat)(VNode *file, struct stat *buf);
    int (*mkdir)(VNode *parent, const char *name, VNode *node);
    int (*creat)(VNode *parent, const char *name, VNode *node);
    int (*fmap)(VNode *file, size_t offset, size_t size);
} VNodeOps;

struct v_node
{
    char *name;
    int name_len;

    uint64_t file_size;
    uint64_t position;

    VNode *parent;

    VNode **childs;
    int childs_len;

    uint16_t type;

    uint16_t ctime;
    uint16_t mtime;
    uint16_t atime;

    VNodeOps *ops;

    void *desc;
};

struct VFile
{
    VNode *file;
    size_t offset;
    bool readable;
    bool writeable;
};

#define MAX_FS_NUM 8

typedef struct
{
    char *name;
    VNodeOps *ops;
} FileSystem;

void rtc_time_to_posix_timespec(struct time_t *time, struct timespec *spec);

void init_vfs();

VNode *path_walk(VNode *parent, const char *path);

void register_fs(const char *name, VNodeOps *ops);
