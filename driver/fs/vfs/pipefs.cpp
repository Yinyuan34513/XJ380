#define ALL_IMPLEMENTATION
#include "pipe.h"
#include <proto.hpp>
#include <task/poll.h>
#include <fs/vfs/vfs.h>
#include <errno.h>

vfs_node_t pipefs_root = NULL;
int        pipefs_id   = 0;
int        pipefd_id   = 0;

void pipefs_open(void *parent, const char *name, vfs_node_t node) {
    (void)parent;
    (void)name;
    node->type = file_pipe;
}

size_t pipefs_read(void *file, void *addr, size_t offset, size_t size) {
    (void)offset;
    if (size > PIPE_BUFF) size = PIPE_BUFF;

    pipe_specific_t *spec = (pipe_specific_t *)file;
    if (!spec) return -EINVAL;
    pipe_info_t *pipe = spec->info;
    if (!pipe) return -EINVAL;

    while (true) {
        spin_lock(&pipe->lock);
        if (pipe->ptr != 0) {
            uint32_t to_read = MIN(size, pipe->ptr);
            memcpy(addr, pipe->buf, to_read);
            if (pipe->ptr > to_read) {
                memmove(pipe->buf, pipe->buf + to_read, pipe->ptr - to_read);
            }
            pipe->ptr -= to_read;
            spin_unlock(&pipe->lock);
            return to_read;
        }
        if (pipe->write_fds == 0) {
            spin_unlock(&pipe->lock);
            return 0;
        }
        spin_unlock(&pipe->lock);
        scheduler_yield();
    }
}

size_t pipe_write_inner(void *file, const void *addr, size_t size) {
    pipe_specific_t *spec = (pipe_specific_t *)file;
    pipe_info_t     *pipe = spec->info;

    while (true) {
        spin_lock(&pipe->lock);
        if (pipe->read_fds == 0) {
            spin_unlock(&pipe->lock);
            return (size_t)-EPIPE;
        }
        if ((PIPE_BUFF - pipe->ptr) >= size) {
            memcpy(&pipe->buf[pipe->ptr], addr, size);
            pipe->ptr += size;
            spin_unlock(&pipe->lock);
            return size;
        }
        spin_unlock(&pipe->lock);
        scheduler_yield();
    }
}

size_t pipefs_write(void *file, const void *addr, size_t offset, size_t size) {
    (void)offset;
    size_t ret       = 0;
    size_t chunks    = size / PIPE_BUFF;
    size_t remainder = size % PIPE_BUFF;
    if (chunks)
        for (size_t i = 0; i < chunks; i++) {
            size_t cycle = 0;
            while (cycle != PIPE_BUFF)
            {
                size_t wrote = pipe_write_inner(file, (const char *)addr + i * PIPE_BUFF + cycle, PIPE_BUFF - cycle);
                if (wrote == (size_t)-EPIPE) return ret ? ret : wrote;
                cycle += wrote;
            }
            ret += cycle;
        }

    if (remainder) {
        size_t cycle = 0;
        while (cycle != remainder)
        {
            size_t wrote = pipe_write_inner(file, (const char *)addr + chunks * PIPE_BUFF + cycle, remainder - cycle);
            if (wrote == (size_t)-EPIPE) return ret ? ret : wrote;
            cycle += wrote;
        }
        ret += cycle;
    }

    return ret;
}

int pipefs_ioctl(void *file, ssize_t cmd, ssize_t arg) {
    switch (cmd) {
    default: return -ENOSYS;
    }
}

bool pipefs_close(void *current) {
    pipe_specific_t *spec = (pipe_specific_t *)current;
    if (spec == NULL) return true;

    pipe_info_t *pipe = spec->info;
    if (pipe == NULL) {
        free(spec);
        return true;
    }

    bool free_spec = false;
    bool free_pipe = false;

    spin_lock(&pipe->lock);
    if (spec->write) {
        if (pipe->write_fds > 0) pipe->write_fds--;
        free_spec = (pipe->write_fds == 0);
    } else {
        if (pipe->read_fds > 0) pipe->read_fds--;
        free_spec = (pipe->read_fds == 0);
    }
    free_pipe = (pipe->write_fds == 0 && pipe->read_fds == 0);

    spin_unlock(&pipe->lock);

    if (free_spec) free(spec);
    if (free_pipe) {
        free(pipe->buf);
        free(pipe);
    }

    return true;
}

int pipefs_poll(void *file, size_t events) {
    pipe_specific_t *spec = (pipe_specific_t *)file;
    pipe_info_t     *pipe = spec->info;

    int out = 0;

    spin_lock(&pipe->lock);
    if (events & EPOLLIN) {
        if (!pipe->write_fds) out |= EPOLLHUP;
        if (pipe->ptr > 0) out |= EPOLLIN;
    }

    if (events & EPOLLOUT) {
        if (!pipe->read_fds) out |= EPOLLHUP;
        if (pipe->ptr < PIPE_BUFF) out |= EPOLLOUT;
    }
    spin_unlock(&pipe->lock);
    return out;
}

int pipefs_mount(const char *handle, vfs_node_t node) {
    if ((uint64_t)handle != PIEFS_REGISTER_ID) return VFS_STATUS_FAILED;
    node->fsid  = pipefs_id;
    pipefs_root = node;
    return VFS_STATUS_SUCCESS;
}

static int dummy() {
    return -ENOSYS;
}

errno_t pipefs_stat(void *file, vfs_node_t node) {
    pipe_specific_t *spec = (pipe_specific_t *)file;
    pipe_info_t     *pipe = spec->info;
    if(pipe == NULL) return EOK;
    node->size = pipe->ptr;
    return EOK;
}

static struct vfs_callback pipefs_callbacks = {
    .mount    = pipefs_mount,
    .unmount  = (vfs_unmount_t)empty,
    .open     = (vfs_open_t)pipefs_open,
    .close    = (vfs_close_t)pipefs_close,
    .read     = pipefs_read,
    .write    = pipefs_write,
    .readlink = (vfs_readlink_t)dummy,
    .mkdir    = (vfs_mk_t)empty,
    .mkfile   = (vfs_mk_t)empty,
    .link     = (vfs_mk_t)dummy,
    .symlink  = (vfs_mk_t)dummy,
    .del   = (vfs_del_t)empty,
    .rename   = (vfs_rename_t)empty,
    .map      = (vfs_mapfile_t)empty,
    .stat     = pipefs_stat,
    .ioctl    = (vfs_ioctl_t)pipefs_ioctl,
    .poll     = pipefs_poll,
    .dup      = (vfs_dup_t)empty,
    .resize   = (vfs_resize_t)dummy,
};

void pipefs_setup() {
    pipefs_id = vfs_regist("pipefs", &pipefs_callbacks, PIEFS_REGISTER_ID, 0x50495045);
    vfs_mkdir("/pipe");
    vfs_node_t node = vfs_open("/pipe");
    if (node == NULL) {
        write_serial_string("pipefs: open /pipe failed\n");
        return;
    }
    if (vfs_mount((const char *)PIEFS_REGISTER_ID, node) == VFS_STATUS_FAILED) {
        write_serial_string("pipefs: mount failed\n");
        return;
    }
}
