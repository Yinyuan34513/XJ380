#include "../../include/include.h"

VNode node_table[MAX_VNODE_NUM];

VNode *root;

FileSystem filesystems[MAX_FS_NUM];
int fs_idx = 0;

spinlock_t fs_op_lock;

VNode *get_free_node()
{
    for (size_t i = 0; i < MAX_VNODE_NUM; i++)
    {
        VNode *node = &node_table[i];
        if (node->type == NODE_TYPE_NONE)
        {
            return node;
        }
    }
    return nullptr;
}

void put_free_node(VNode *node)
{
    free(node->name);
    node->type = NODE_TYPE_NONE;
}

void add_child_to(VNode *parent, VNode *child)
{
    child->parent = parent;
    if (!parent->childs)
        parent->childs = (VNode **)malloc(sizeof(VNode *) * MAX_CHILD_VNODE_NUM);
    parent->childs[parent->childs_len] = child;
    parent->childs_len++;
}

VNode *search_child_list(VNode *parent, char *name)
{
    if (!parent->childs)
        goto failed;

    for (int i = 0; i < parent->childs_len; i++)
    {
        VNode *node = parent->childs[i];
        if (!strcmp(node->name, name))
        {
            return node;
        }
    }

failed:
    return nullptr;
}

VNode *path_walk(VNode *parent, const char *path)
{
    spin_lock(&fs_op_lock);

    VNode *dir = parent;

    if (!dir)
    {
        dir = root;
    }

    if (dir->type != NODE_TYPE_DIR)
    {
        return nullptr;
    }

    char *new_path = (char *)path;

    while (*new_path == '/' || *new_path == '\\')
        ++new_path;

    if ((!*new_path) || (*new_path == '\0'))
        return dir;

    VNode *node;

    while (true)
    {
        if (dir == nullptr)
        {
            spin_unlock(&fs_op_lock);
            return nullptr;
        }

        char *tmp_path = new_path;
        while ((*new_path && *new_path != '\0') && *new_path != '/')
            ++new_path;

        int tmp_path_len = new_path - tmp_path;

        char *tmpname = (char *)malloc(tmp_path_len + 1);
        strncpy(tmpname, tmp_path, tmp_path_len);
        tmpname[tmp_path_len] = '\0';

        node = search_child_list(dir, tmpname);

        if (node == nullptr)
        {
            node = get_free_node();

            node->name = (char *)malloc(tmp_path_len + 1);
            strncpy(node->name, tmpname, tmp_path_len);
            node->name[tmp_path_len] = '\0';
            node->name_len = tmp_path_len;

            // Lookup
            if (dir->ops->lookup(dir, node) == 0)
            {
                write_serial_string("Cannot find file/dir: ");
                write_serial_string(node->name);
                write_serial_string("\n");
                free(tmpname);
                put_free_node(node);
                spin_unlock(&fs_op_lock);
                return nullptr;
            }

            add_child_to(dir, node);
        }

        free(tmpname);

        if (!*new_path)
            goto last_component;

        while (*new_path == '/')
            ++new_path;

        if (!*new_path)
            goto last_slash;

        dir = node;
    }

last_component:
last_slash:
    spin_unlock(&fs_op_lock);
    return node;
}

void failed_to_mount_root()
{
    put_free_node(root);
    write_serial_string("Cannot mount root file system, halting...\n");
    while (true)
    {
        __asm__ __volatile__("pause");
    }
}

void test_vfs()
{
}

void mount_root()
{
    root = get_free_node();

    for (int i = 0; i < MAX_SUPPORTED_PARTITON_DEV_NUM; i++)
    {
        device_t *device = device_find(DEV_PART, 0);
        if (!device)
        {
            continue;
        }

        for (int i = 0; i < fs_idx; i++)
        {
            FileSystem *filesystem = &filesystems[i];
            if (filesystem->ops && filesystem->ops->super && filesystem->ops->super(device->dev, root) == 0)
            {
                root->ops = filesystem->ops;
                root->type = NODE_TYPE_DIR;
                write_serial_string("Mount root OK\n");
                test_vfs();
                return;
            }
        }
    }

    failed_to_mount_root();
}

void rtc_time_to_posix_timespec(struct time_t *time, struct timespec *spec)
{
    int year = time->tm_year + 1900;
    int month = time->tm_mon + 1;

    if (month <= 2)
    {
        year--;
        month += 12;
    }

    int days = (365 * year) + (year / 4) - (year / 100) + (year / 400);
    days += (30 * month + 3 * (month + 1) / 5 - 2) + time->tm_mday - 719561;

    spec->tv_sec = days * 86400LL + time->tm_hour * 3600 + time->tm_min * 60 + time->tm_sec;
    spec->tv_nsec = 0;
}

void init_vfs()
{
    spin_init(&fs_op_lock);

    xmemset(node_table, 0, sizeof(node_table));

    for (size_t i = 0; i < MAX_VNODE_NUM; i++)
    {
        VNode *node = &node_table[i];
        node->type = NODE_TYPE_NONE;
    }

    mount_root();
}

void register_fs(const char *name, VNodeOps *ops)
{
    FileSystem *filesystem = &filesystems[fs_idx];
    fs_idx++;
    filesystem->name = (char *)malloc(strlen(name) + 1);
    strcpy(filesystem->name, name);

    filesystem->ops = ops;
}
