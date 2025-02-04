#include "../../include/include.h"

VNode node_table[MAX_VNODE_NUM];

VNode *root;

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

VNode *find_node(int nr)
{
    for (int i = 0; i < MAX_VNODE_NUM; i++)
    {
        if (node_table[i].nr == nr)
        {
            return &node_table[i];
        }
    }

    return nullptr;
}

void add_child_to(VNode *parent, VNode *child)
{
    child->parent = parent;
    parent->childs[parent->childs_len] = child;
    parent->childs_len++;
}

VNode *search_child_list(VNode *parent, char *name)
{
    for (int i = 0; i < parent->childs_len; i++)
    {
        VNode *node = parent->childs[i];
        if (!strcmp(node->name, name))
        {
            return node;
        }
    }

    return nullptr;
}

VNode *path_walk(VNode *parent, const char *path)
{
    VNode *dir = parent;

    if (!dir)
    {
        dir = root;
    }

    const char *backup_path = path;
    char *new_path = (char *)backup_path;

    while (*new_path == '/' || *new_path == '\\')
        ++new_path;

    if ((!*new_path) || (*new_path == '\0'))
        return dir;

    VNode *node;

    while (true)
    {
        if (dir == nullptr)
        {
            return nullptr;
        }

        char *tmp_path = new_path;
        while ((*new_path && *new_path != '\0') && (*new_path != '/' || *new_path != '\\'))
            ++new_path;

        int tmp_path_len = tmp_path - new_path;

        char *tmpname = (char *)malloc(tmp_path_len + 1);
        strncpy(tmpname, tmp_path, tmp_path_len);
        tmpname[tmp_path_len] = '\0';

        node = search_child_list(dir, tmpname);

        free(tmpname);

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
                free(node->name);
                free(node);
                return nullptr;
            }

            add_child_to(dir, node);
        }

        while (*new_path == '/' || *new_path == '\\')
            ++new_path;

        if ((!*path) || (*path == '\0'))
        {
            return node;
        }

        dir = node;
    }
}

extern VNodeOps fat32_ops;

void mount_root()
{
    root = get_free_node();

    // if (0)
    // {
    // }
    // else
    // {
    //     put_free_node(root);
    //     write_serial_string("Cannot mount root file system, halting...\n");
    //     while (true)
    //     {
    //         __asm__ __volatile__("hlt");
    //     }
    // }
}

void init_vfs()
{
    xmemset(node_table, 0, sizeof(node_table));
    for (size_t i = 0; i < MAX_VNODE_NUM; i++)
    {
        VNode *node = &node_table[i];
        node->type = NODE_TYPE_NONE;
        node->childs = (VNode **)malloc(sizeof(VNode) * MAX_CHILD_VNODE_NUM);
    }

    mount_root();
}
