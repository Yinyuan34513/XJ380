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
    int (*super)(int drive, VNode **result);
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

    uint64_t ctime;
    uint64_t mtime;
    uint64_t atime;

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

void init_vfs();
