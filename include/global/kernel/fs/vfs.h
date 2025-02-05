#pragma once

#define MAX_VNODE_NUM 1024
#define MAX_CHILD_VNODE_NUM 256

enum
{
    NODE_TYPE_NONE,
    NODE_TYPE_FILE,
    NODE_TYPE_DIR,
};

struct v_node;

typedef v_node VNode;

typedef struct
{
    // For kernel
    int (*lookup)(VNode *node, VNode *search_node);
    int (*super)(int drive, VNode *result);
    // For user
    int (*open)(VNode *parent, const char *name, VNode *node);
    void (*close)(VNode *current);
    int (*resize)(VNode *current, uint64_t size);
    int (*write)(VNode *file, void *addr, size_t offset, size_t size);
    int (*read)(VNode *file, void *addr, size_t offset, size_t size);
    int (*stat)(VNode *file, VNode *node);
    int (*mkdir)(VNode *parent, const char *name, VNode *node);
    int (*creat)(VNode *parent, const char *name, VNode *node);
    int (*fmap)(VNode *file, size_t offset, size_t size);
} VNodeOps;

struct v_node
{
    char *name;
    int name_len;

    VNode *parent;

    VNode **childs;
    int childs_len;

    int nr;
    uint16_t type;

    uint16_t ctime;
    uint16_t mtime;
    uint16_t atime;

    VNodeOps *ops;

    void *desc;
};

struct VFile
{
    void *file;
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

void init_vfs();

void register_fs(const char *name, VNodeOps *ops);
