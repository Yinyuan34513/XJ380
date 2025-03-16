#include "../../include/include.h"

int sys_read(uint32_t fd, uint8_t *buf, size_t count)
{
    if (fd >= MAX_FD_NUM)
    {
        return -1;
    }
    struct VFile *file = current_task->fds[fd];
    VNode *node = file->file;
    if (node->ops->read == NULL && !file->readable)
    {
        return -1;
    }
    return node->ops->read(node, buf, file->offset, count);
}

int sys_write(uint32_t fd, uint8_t *buf, size_t count)
{
    if (fd >= MAX_FD_NUM)
    {
        return -1;
    }
    if (fd == 1 || fd == 2)
    {
        write_serial_string((const char *)buf);
        return strlen((const char *)buf);
    }
    struct VFile *file = current_task->fds[fd];
    VNode *node = file->file;
    if (node->ops->write == NULL && !file->writeable)
    {
        return -1;
    }
    return node->ops->write(node, buf, file->offset, count);
}

int sys_open(const char *pathname, int flags, int mode)
{
    VNode *node = path_walk(current_task->cwd_node, pathname);
    if (node == NULL)
    {
        return -1;
    }
    int fd = current_task->fd_count++;
    struct VFile *file = current_task->fds[fd];

    if (current_task->fd_count >= MAX_FD_NUM)
        current_task->fd_count = 3;

    file->file = node;
    // TODO: 根据flags和mode设置readable和writeable
    file->readable = 1;
    file->writeable = 1;
    return fd;
}

int sys_close(uint32_t fd)
{
    if (fd >= MAX_FD_NUM)
    {
        return -1;
    }
    current_task->fds[fd] = NULL;
    return 0;
}
