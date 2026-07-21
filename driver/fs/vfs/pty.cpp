#define ALL_IMPLEMENTATION

#include <errno.h>
#include <fs/vfs/vfs.h>
#include <id_alloc.h>
#include <ioctl.h>
#include <krlibc.h>
#include <proto.hpp>
#include <pty.h>
#include <syscall/syscall.h>
#include <task/pcb.h>
#include <task/poll.h>
#include <task/scheduler.h>

#define TCIFLUSH 0
#define TCOFLUSH 1
#define TCIOFLUSH 2
#define TCOOFF 0
#define TCOON 1
#define TCIOFF 2
#define TCION 3

static int                 ptmx_fsid = 0;
static int                 pts_fsid  = 0;
static struct llist_header ptmx_list_head;
static spin_t              pty_global_lock = SPIN_INIT;
static id_allocator_t     *pty_allocator   = NULL;
static vfs_node_t          pts_root        = NULL;

static uint64_t pty_mkdev(uint32_t major, uint32_t minor)
{
    return ((uint64_t)(major & 0xfffU) << 8) | (uint64_t)(minor & 0xffU) |
           ((uint64_t)(minor & 0xfffff00U) << 12);
}

static int pty_dummy()//空
{
    return -ENOSYS;
}

static int pty_str_to_int(const char *str, int *result)//stoi
{
    if (str == NULL || *str == '\0' || result == NULL) return -EINVAL;

    int value = 0;
    for (; *str != '\0'; str++)
    {
        if (*str < '0' || *str > '9') return -EINVAL;
        value = value * 10 + (*str - '0');
    }

    *result = value;
    return 0;
}

static int pty_id_alloc()//分配id
{
    spin_lock(&pty_global_lock);
    int ret = id_alloc(pty_allocator);
    spin_unlock(&pty_global_lock);
    return ret;
}

static void pty_id_free(int id)//你自由了》：|（
{
    spin_lock(&pty_global_lock);
    id_free(pty_allocator, (uint32_t)id);
    spin_unlock(&pty_global_lock);
}

static void pty_termios_default(struct termios *term)//默认
{
    memset(term, 0, sizeof(*term));
    term->c_iflag = ICRNL | IXON | BRKINT | ISTRIP | INPCK;
    term->c_oflag = OPOST | ONLCR;
    term->c_cflag = B38400 | CS8 | CREAD | HUPCL;
    term->c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK;

    term->c_cc[VINTR]  = 3;
    term->c_cc[VQUIT]  = 28;
    term->c_cc[VERASE] = 127;
    term->c_cc[VKILL]  = 21;
    term->c_cc[VEOF]   = 4;
    term->c_cc[VTIME]  = 0;
    term->c_cc[VMIN]   = 1;
    term->c_cc[VSTART] = 17;
    term->c_cc[VSTOP]  = 19;
    term->c_cc[VSUSP]  = 26;//c_cc为啥好了?我也不知道
}

static pty_handle_t *pty_find_locked(int id)
{
    pty_handle_t *pos;
    pty_handle_t *n;

    llist_for_each(pos, n, &ptmx_list_head, list_node)//这个可不兴用啊
    {
        if (pos->id == id) return pos;
    }
    return NULL;
}

static void pty_pair_destroy_locked(pty_handle_t *pair)
{
    if (pair->slave_node != NULL && pair->slave_node->parent != NULL)
    {
        bool slave_is_open = pair->slave_node->handle != NULL;
        vfs_child_lock();
        pair->slave_node->parent->child = list_delete(pair->slave_node->parent->child, pair->slave_node);
        vfs_child_unlock();
        pair->slave_node->handle = NULL;
        pair->slave_node->parent = NULL;
        if (!slave_is_open)
        {
            free(pair->slave_node->name);
            free(pair->slave_node);
        }
    }

    llist_delete(&pair->list_node);
    id_free(pty_allocator, (uint32_t)pair->id);
    free(pair->master_buffer);
    free(pair->slave_buffer);
    free(pair);
}

static size_t ptmx_data_available(pty_handle_t *pair)
{
    return pair->ptr_master;
}

static bool pty_nonblock(pty_handle_t *pair, bool master)
{
    vfs_node_t node = master ? pair->master_node : pair->slave_node;
    return node != NULL && (node->flags & O_NONBLOCK) != 0;
}

static size_t pty_master_append_processed_locked(pty_handle_t *pair, const uint8_t *in, size_t size)
{
    size_t written = 0;
    bool translate = (pair->term.c_oflag & OPOST) && (pair->term.c_oflag & ONLCR);

    for (size_t i = 0; i < size; i++)
    {
        uint8_t ch = in[i];
        if (translate && ch == '\n')
        {
            if (pair->ptr_master + 2 > PTY_BUFF_SIZE) break;
            pair->master_buffer[pair->ptr_master++] = '\r';
            pair->master_buffer[pair->ptr_master++] = '\n';
        }
        else
        {
            if (pair->ptr_master + 1 > PTY_BUFF_SIZE) break;
            pair->master_buffer[pair->ptr_master++] = ch;
        }
        written++;
    }

    return written;
}

static size_t pts_data_available(pty_handle_t *pair)
{
    if (!(pair->term.c_lflag & ICANON)) return pair->ptr_slave;

    for (size_t i = 0; i < pair->ptr_slave; i++)
    {
        if (pair->slave_buffer[i] == '\n' || pair->slave_buffer[i] == pair->term.c_cc[VEOF] ||
            pair->slave_buffer[i] == pair->term.c_cc[VEOL] ||
            pair->slave_buffer[i] == pair->term.c_cc[VEOL2])
            return i + 1;
    }
    return 0;
}

static void pts_ctrl_assign(pty_handle_t *pair)
{
    tcb_t task = get_current_task();
    if (task == NULL || task->parent_group == NULL) return;
    pair->ctrl_session = task->parent_group->sid;
    pair->ctrl_pgid    = (int)task->parent_group->pgid;
    if (pair->front_pgid == 0) pair->front_pgid = pair->ctrl_pgid;
}

static int pty_foreground_pgid(pty_handle_t *pair)
{
    return pair->front_pgid != 0 ? pair->front_pgid : pair->ctrl_pgid;
}

static void pty_packet_mark_data_locked(pty_handle_t *pair)
{
    if (pair != NULL && pair->packet_mode && pair->ptr_master > 0)
        pair->packet_data_pending = true;
}

static void pty_packet_queue_locked(pty_handle_t *pair, uint8_t status)
{
    if (pair != NULL && pair->packet_mode && status != 0)
        pair->packet_status |= status;
}

static void pty_packet_termios_changed_locked(pty_handle_t *pair, const struct termios *old_term)
{
    if (pair == NULL || old_term == NULL) return;
    if ((old_term->c_iflag ^ pair->term.c_iflag) & IXON)
    {
        pty_packet_queue_locked(pair, (pair->term.c_iflag & IXON) ? TIOCPKT_DOSTOP : TIOCPKT_NOSTOP);
    }
}

static int pty_flush_locked(pty_handle_t *pair, int selector)
{
    if (pair == NULL) return -EINVAL;
    switch (selector)
    {
    case TCIFLUSH:
        pair->ptr_slave = 0;
        pty_packet_queue_locked(pair, TIOCPKT_FLUSHREAD);
        return 0;
    case TCOFLUSH:
        pair->ptr_master = 0;
        pair->packet_data_pending = false;
        pty_packet_queue_locked(pair, TIOCPKT_FLUSHWRITE);
        return 0;
    case TCIOFLUSH:
        pair->ptr_slave = 0;
        pair->ptr_master = 0;
        pair->packet_data_pending = false;
        pty_packet_queue_locked(pair, TIOCPKT_FLUSHREAD | TIOCPKT_FLUSHWRITE);
        return 0;
    default:
        return -EINVAL;
    }
}

static int pty_tcxonc_locked(pty_handle_t *pair, bool master, int action)
{
    if (pair == NULL) return -EINVAL;
    bool *stopped = master ? &pair->stop_master_output : &pair->stop_slave_output;
    switch (action)
    {
    case TCOOFF:
        *stopped = true;
        return 0;
    case TCOON:
        *stopped = false;
        return 0;
    case TCIOFF:
    case TCION: {
        uint8_t flow = pair->term.c_cc[action == TCIOFF ? VSTOP : VSTART];
        if (master)
        {
            if (pair->ptr_slave >= PTY_BUFF_SIZE) return -EAGAIN;
            pair->slave_buffer[pair->ptr_slave++] = flow;
        }
        else
        {
            if (pair->ptr_master >= PTY_BUFF_SIZE) return -EAGAIN;
            pty_master_append_processed_locked(pair, &flow, 1);
            pty_packet_mark_data_locked(pair);
        }
        return 0;
    }
    default:
        return -EINVAL;
    }
}

static void pty_termios_to_termios2(const struct termios *src, struct termios2 *dst)
{
    memset(dst, 0, sizeof(*dst));
    dst->c_iflag  = src->c_iflag;
    dst->c_oflag  = src->c_oflag;
    dst->c_cflag  = src->c_cflag;
    dst->c_lflag  = src->c_lflag;
    dst->c_line   = src->c_line;
    dst->c_ispeed = B38400;
    dst->c_ospeed = B38400;
    memcpy(dst->c_cc, src->c_cc, MIN(sizeof(dst->c_cc), sizeof(src->c_cc)));
}

static void pty_termios_from_termios2(struct termios *dst, const struct termios2 *src)
{
    dst->c_iflag = src->c_iflag;
    dst->c_oflag = src->c_oflag;
    dst->c_cflag = src->c_cflag;
    dst->c_lflag = src->c_lflag;
    dst->c_line  = src->c_line;
    memset(dst->c_cc, 0, sizeof(dst->c_cc));
    memcpy(dst->c_cc, src->c_cc, MIN(sizeof(dst->c_cc), sizeof(src->c_cc)));
}

static int pty_install_slave_fd(pty_handle_t *pair, uint64_t flags)
{
    if (pair == NULL || pair->slave_node == NULL) return -ENODEV;
    const uint64_t allowed_flags = O_ACCMODE | O_CLOEXEC | O_NONBLOCK | O_NOCTTY | O_LARGEFILE;
    if ((flags & ~allowed_flags) != 0)
    {
        write_serial_fmt("pty: TIOCGPTPEER invalid flags=0x%llx allowed=0x%llx\n",
                         (unsigned long long)flags,
                         (unsigned long long)allowed_flags);
        return -EINVAL;
    }

    pcb_t process = get_current_task() != NULL ? get_current_task()->parent_group : NULL;
    if (process == NULL || process->file_open == NULL) return -EBADF;

    fd_file_handle *fd_handle = (fd_file_handle *)calloc(1, sizeof(fd_file_handle));
    if (fd_handle == NULL) return -ENOMEM;

    spin_lock(&pair->lock);
    if (pair->locked)
    {
        spin_unlock(&pair->lock);
        free(fd_handle);
        return -EIO;
    }

    vfs_node_t slave = pair->slave_node;
    if (slave->handle == NULL && slave->refcount == 1) slave->refcount = 0;
    pair->slave_fds++;
    slave->handle = pair;
    slave->type   = file_pts;
    slave->fsid   = pts_fsid;
    slave->size   = (uint64_t)-1;
    slave->mode   = 0666;
    slave->inode  = (uint64_t)pair->id;
    slave->rdev   = pty_mkdev(136, (uint32_t)pair->id);
    slave->refcount++;
    spin_unlock(&pair->lock);

    fd_handle->node = slave;
    fd_handle->flags = flags & allowed_flags;
    fd_handle->node->flags = fd_handle->flags;
    fd_handle->fd = queue_enqueue_lowest(process->file_open, fd_handle);
    if (fd_handle->fd == (size_t)-1)
    {
        vfs_close(slave);
        free(fd_handle);
        return -EBADF;
    }
    return (int)fd_handle->fd;
}

static void ptmx_open(void *parent, const char *name, vfs_node_t node)
{
    UNUSED(parent, name);

    pty_handle_t *pair = (pty_handle_t *)calloc(1, sizeof(pty_handle_t));
    if (pair == NULL) return;

    llist_init_head(&pair->list_node);
    pty_termios_default(&pair->term);
    pair->win.ws_row    = 24;
    pair->win.ws_col    = 80;
    pair->tty_kbmode    = K_XLATE;
    pair->master_buffer = (uint8_t *)calloc(1, PTY_BUFF_SIZE);
    pair->slave_buffer  = (uint8_t *)calloc(1, PTY_BUFF_SIZE);
    pair->id            = pty_id_alloc();
    pair->master_fds    = 1;
    pair->lock          = SPIN_INIT;

    if (pair->id < 0 || pair->master_buffer == NULL || pair->slave_buffer == NULL || pts_root == NULL)
    {
        if (pair->id >= 0) pty_id_free(pair->id);
        free(pair->master_buffer);
        free(pair->slave_buffer);
        free(pair);
        return;
    }

    char slave_name[16];
    snprintf(slave_name, sizeof(slave_name), "%d", pair->id);

    vfs_node_t old_slave = vfs_do_search(pts_root, slave_name);
    if (old_slave != NULL)
    {
        vfs_child_lock();
        pts_root->child = list_delete(pts_root->child, old_slave);
        vfs_child_unlock();
        old_slave->parent = NULL;
        old_slave->handle = NULL;
    }

    vfs_node_t slave = vfs_node_alloc(pts_root, slave_name);
    if (slave == NULL)
    {
        pty_id_free(pair->id);
        free(pair->master_buffer);
        free(pair->slave_buffer);
        free(pair);
        return;
    }

    slave->type        = file_pts;
    slave->fsid        = pts_fsid;
    slave->handle      = NULL;
    slave->mode        = 0666;
    slave->size        = (uint64_t)-1;
    slave->inode       = (uint64_t)pair->id;
    slave->rdev        = pty_mkdev(136, (uint32_t)pair->id);
    pair->slave_node   = slave;
    pair->master_node  = node;

    vfs_node_t dev_node = node->parent;
    if (dev_node != NULL)
    {
        vfs_child_lock();
        dev_node->child = list_delete(dev_node->child, node);
        vfs_child_unlock();
        node->parent = NULL;

        vfs_node_t new_ptmx = vfs_node_alloc(dev_node, "ptmx");
        if (new_ptmx != NULL)
        {
            new_ptmx->type   = file_ptmx;
            new_ptmx->fsid   = ptmx_fsid;
            new_ptmx->handle = NULL;
            new_ptmx->mode   = 0666;
            new_ptmx->size   = (uint64_t)-1;
            new_ptmx->rdev   = pty_mkdev(5, 2);
        }
    }

    node->handle       = pair;
    node->type         = file_ptmx;
    node->fsid         = ptmx_fsid;
    node->refcount     = 1;
    node->size         = (uint64_t)-1;
    node->mode         = 0666;
    node->rdev         = pty_mkdev(5, 2);

    spin_lock(&pty_global_lock);
    llist_append(&ptmx_list_head, &pair->list_node);
    spin_unlock(&pty_global_lock);
}

static void ptmx_close(void *handle)
{
    pty_handle_t *pair = (pty_handle_t *)handle;
    if (pair == NULL) return;

    spin_lock(&pair->lock);
    if (pair->master_fds > 0) pair->master_fds--;
    bool destroy = pair->master_fds == 0 && pair->slave_fds == 0;
    spin_unlock(&pair->lock);

    if (destroy)
    {
        spin_lock(&pty_global_lock);
        pty_pair_destroy_locked(pair);
        spin_unlock(&pty_global_lock);
    }
    poll_notify_event();
}

static size_t ptmx_read(void *file, void *addr, size_t offset, size_t size)
{
    UNUSED(offset);
    pty_handle_t *pair = (pty_handle_t *)file;
    if (pair == NULL || addr == NULL) return VFS_STATUS_FAILED;
    if (size == 0) return 0;

    while (true)
    {
        spin_lock(&pair->lock);
        if (pair->packet_mode && pair->packet_status != 0 && size > 0)
        {
            ((uint8_t *)addr)[0] = pair->packet_status;
            pair->packet_status = 0;
            spin_unlock(&pair->lock);
            poll_notify_event();
            return 1;
        }
        if (ptmx_data_available(pair) > 0)
        {
            spin_unlock(&pair->lock);
            break;
        }
        if (pair->slave_fds == 0)
        {
            spin_unlock(&pair->lock);
            return 0;
        }
        if (pty_nonblock(pair, true))
        {
            spin_unlock(&pair->lock);
            return (size_t)-EWOULDBLOCK;
        }
        spin_unlock(&pair->lock);
        scheduler_yield();
    }

    spin_lock(&pair->lock);
    uint8_t *out = (uint8_t *)addr;
    size_t to_copy = MIN(size, ptmx_data_available(pair));
    if (pair->packet_mode && size > 0)
    {
        out[0] = TIOCPKT_DATA;
        to_copy = MIN(size - 1, ptmx_data_available(pair));
        memcpy(out + 1, pair->master_buffer, to_copy);
        memmove(pair->master_buffer, pair->master_buffer + to_copy, pair->ptr_master - to_copy);
        pair->ptr_master -= to_copy;
        to_copy++;
        pair->packet_data_pending = pair->ptr_master > 0;
    }
    else
    {
        memcpy(out, pair->master_buffer, to_copy);
        memmove(pair->master_buffer, pair->master_buffer + to_copy, pair->ptr_master - to_copy);
        pair->ptr_master -= to_copy;
    }
    spin_unlock(&pair->lock);
    poll_notify_event();
    return to_copy;
}

static size_t ptmx_write(void *file, const void *addr, size_t offset, size_t size)
{
    UNUSED(offset);
    pty_handle_t *pair = (pty_handle_t *)file;
    if (pair == NULL || addr == NULL) return VFS_STATUS_FAILED;
    if (size == 0) return 0;

    while (true)
    {
        spin_lock(&pair->lock);
        if (pair->slave_fds == 0)
        {
            spin_unlock(&pair->lock);
            return (size_t)-EIO;
        }
        if (pair->ptr_slave < PTY_BUFF_SIZE)
        {
            spin_unlock(&pair->lock);
            break;
        }
        if (pty_nonblock(pair, true))
        {
            spin_unlock(&pair->lock);
            return (size_t)-EWOULDBLOCK;
        }
        spin_unlock(&pair->lock);
        scheduler_yield();
    }

    spin_lock(&pair->lock);
    const uint8_t *input = (const uint8_t *)addr;
    size_t written = 0;
    for (size_t i = 0; i < size; i++)
    {
        uint8_t ch = input[i];
        if (pair->term.c_iflag & IXON)
        {
            if (ch == pair->term.c_cc[VSTOP])
            {
                pair->stop_master_output = true;
                written++;
                continue;
            }
            if (ch == pair->term.c_cc[VSTART])
            {
                pair->stop_master_output = false;
                written++;
                continue;
            }
        }
        if ((pair->term.c_lflag & ISIG) && pty_foreground_pgid(pair) != 0)
        {
            int signal = 0;
            if (ch == pair->term.c_cc[VINTR]) signal = SIGINT;
            else if (ch == pair->term.c_cc[VQUIT]) signal = SIGQUIT;
            else if (ch == pair->term.c_cc[VSUSP]) signal = SIGTSTP;
            if (signal != 0)
            {
                signal_send_process_group((size_t)pty_foreground_pgid(pair), signal);
                written++;
                continue;
            }
        }
        if ((pair->term.c_iflag & ICRNL) && ch == '\r')
        {
            ch = '\n';
        }

        bool erased = false;
        if ((pair->term.c_lflag & ICANON) && (ch == '\b' || ch == 0x7f))
        {
            if (pair->ptr_slave > 0 && pair->slave_buffer[pair->ptr_slave - 1] != '\n')
            {
                pair->ptr_slave--;
                erased = true;
            }
            if (pair->term.c_lflag & ECHO)
            {
                static const uint8_t erase_seq[] = {'\b', ' ', '\b'};
                (void)pty_master_append_processed_locked(pair, erase_seq, sizeof(erase_seq));
            }
            written++;
            continue;
        }

        if (pair->ptr_slave >= PTY_BUFF_SIZE) break;
        pair->slave_buffer[pair->ptr_slave++] = ch;

        if (pair->term.c_lflag & ECHO)
        {
            (void)pty_master_append_processed_locked(pair, &ch, 1);
        }
        else if (ch == '\n' && (pair->term.c_lflag & ECHONL))
        {
            (void)pty_master_append_processed_locked(pair, &ch, 1);
        }

        if (!erased) written++;
    }
    spin_unlock(&pair->lock);
    poll_notify_event();
    return written;
}

static errno_t ptmx_ioctl(void *handle, size_t request, void *arg)
{
    pty_handle_t *pair = (pty_handle_t *)handle;
    if (pair == NULL) return -ENOTTY;

    uint32_t req = (uint32_t)request;
    if (req == TIOCGPTPEER) return pty_install_slave_fd(pair, (uint64_t)(uintptr_t)arg);

    errno_t ret = EOK;
    spin_lock(&pair->lock);
    switch (req)
    {
    case TIOCSPTLCK:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        pair->locked = (*(int *)arg) != 0;
        break;
    case TIOCGPTN:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        *(int *)arg = pair->id;
        break;
    case TIOCGDEV:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        *(uint32_t *)arg = (uint32_t)pty_mkdev(5, 2);
        break;
    case FIONREAD:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        *(int *)arg = (pair->packet_mode && (pair->packet_status != 0 || pair->packet_data_pending))
                          ? 0
                          : (int)ptmx_data_available(pair);
        break;
    case TIOCOUTQ:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        *(int *)arg = (int)pair->ptr_slave;
        break;
    case TIOCEXCL:
    case TIOCNXCL:
    case TIOCSBRK:
    case TIOCCBRK:
    case TIOCSIG:
        ret = EOK;
        break;
    case TCXONC:
        ret = pty_tcxonc_locked(pair, true, (int)(uintptr_t)arg);
        break;
    case TCFLSH:
        ret = pty_flush_locked(pair, (int)(uintptr_t)arg);
        break;
    case TIOCPKT:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        pair->packet_mode = (*(int *)arg) != 0;
        pair->packet_status = 0;
        pair->packet_data_pending = pair->packet_mode && pair->ptr_master > 0;
        break;
    case TIOCGEXCL:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        *(int *)arg = 0;
        break;
    case TIOCGPKT:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        *(int *)arg = pair->packet_mode ? 1 : 0;
        break;
    case TCGETS:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        memcpy(arg, &pair->term, sizeof(pair->term));
        break;
    case TCGETS2:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        pty_termios_to_termios2(&pair->term, (struct termios2 *)arg);
        break;
    case TCSETS:
    case TCSETSW:
    case TCSETSF: {
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        struct termios old_term = pair->term;
        memcpy(&pair->term, arg, sizeof(pair->term));
        pty_packet_termios_changed_locked(pair, &old_term);
        if (req == TCSETSF) ret = pty_flush_locked(pair, TCIFLUSH);
        break;
    }
    case TCSETS2:
    case TCSETSW2:
    case TCSETSF2:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        pty_termios_from_termios2(&pair->term, (const struct termios2 *)arg);
        if (req == TCSETSF2) ret = pty_flush_locked(pair, TCIFLUSH);
        break;
    case TIOCGPGRP:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        *(int *)arg = pty_foreground_pgid(pair);
        break;
    case TIOCSPGRP:
        if (arg == NULL) ret = -EFAULT;
        else pair->front_pgid = *(int *)arg;
        break;
    case TIOCGETD:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        *(int *)arg = N_TTY;
        break;
    case TIOCSETD:
        break;
    case TIOCGSID:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        *(int *)arg = (int)pair->ctrl_session;
        break;
    case TIOCGPTLCK:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        *(int *)arg = pair->locked ? 1 : 0;
        break;
    case TIOCGWINSZ:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        memcpy(arg, &pair->win, sizeof(pair->win));
        break;
    case TIOCSWINSZ:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        memcpy(&pair->win, arg, sizeof(pair->win));
        break;
    default:
        ret = -ENOTTY;
        break;
    }
    spin_unlock(&pair->lock);
    poll_notify_event();
    return ret;
}

static errno_t ptmx_poll(void *file, size_t events)
{
    pty_handle_t *pair = (pty_handle_t *)file;
    if (pair == NULL) return VFS_STATUS_FAILED;

    int revents = 0;
    spin_lock(&pair->lock);
    if (pair->packet_mode && pair->packet_status != 0)
    {
        if (events & EPOLLIN) revents |= EPOLLIN;
        if (events & EPOLLPRI) revents |= EPOLLPRI;
    }
    if (ptmx_data_available(pair) > 0 && (events & EPOLLIN)) revents |= EPOLLIN;
    if (pair->ptr_slave < PTY_BUFF_SIZE && (events & EPOLLOUT)) revents |= EPOLLOUT;
    if (pair->slave_fds == 0) revents |= EPOLLHUP;
    spin_unlock(&pair->lock);
    return revents;
}

static vfs_node_t ptmx_dup(vfs_node_t node)
{
    pty_handle_t *pair = (pty_handle_t *)node->handle;
    if (pair != NULL)
    {
        spin_lock(&pair->lock);
        pair->master_fds++;
        spin_unlock(&pair->lock);
    }
    return node;
}

static void pts_open(void *parent, const char *name, vfs_node_t node)
{
    UNUSED(parent);

    int id;
    if (pty_str_to_int(name, &id) < 0) return;

    spin_lock(&pty_global_lock);
    pty_handle_t *pair = pty_find_locked(id);
    if (pair == NULL || pair->locked)
    {
        spin_unlock(&pty_global_lock);
        return;
    }

    spin_lock(&pair->lock);
    pair->slave_fds++;
    node->handle = pair;
    node->type   = file_pts;
    node->fsid   = pts_fsid;
    node->size   = (uint64_t)-1;
    node->mode   = 0666;
    node->inode  = (uint64_t)pair->id;
    node->rdev   = pty_mkdev(136, (uint32_t)pair->id);
    if (pair->slave_node == NULL) pair->slave_node = node;
    spin_unlock(&pair->lock);
    spin_unlock(&pty_global_lock);
}

static void pts_close(void *handle)
{
    pty_handle_t *pair = (pty_handle_t *)handle;
    if (pair == NULL) return;

    spin_lock(&pair->lock);
    if (pair->slave_fds > 0) pair->slave_fds--;
    bool destroy = pair->master_fds == 0 && pair->slave_fds == 0;
    spin_unlock(&pair->lock);

    if (destroy)
    {
        spin_lock(&pty_global_lock);
        pty_pair_destroy_locked(pair);
        spin_unlock(&pty_global_lock);
    }
    poll_notify_event();
}

static size_t pts_read(void *file, void *addr, size_t offset, size_t size)
{
    UNUSED(offset);
    pty_handle_t *pair = (pty_handle_t *)file;
    if (pair == NULL || addr == NULL) return VFS_STATUS_FAILED;
    if (size == 0) return 0;

    while (true)
    {
        spin_lock(&pair->lock);
        if (pts_data_available(pair) > 0)
        {
            spin_unlock(&pair->lock);
            break;
        }
        if (pair->master_fds == 0)
        {
            spin_unlock(&pair->lock);
            return 0;
        }
        if (pty_nonblock(pair, false))
        {
            spin_unlock(&pair->lock);
            return (size_t)-EWOULDBLOCK;
        }
        spin_unlock(&pair->lock);
        scheduler_yield();
    }

    spin_lock(&pair->lock);
    size_t to_copy = MIN(size, pts_data_available(pair));
    memcpy(addr, pair->slave_buffer, to_copy);
    memmove(pair->slave_buffer, pair->slave_buffer + to_copy, pair->ptr_slave - to_copy);
    pair->ptr_slave -= to_copy;
    spin_unlock(&pair->lock);
    poll_notify_event();
    return to_copy;
}

static size_t pts_write_inner(pty_handle_t *pair, const uint8_t *in, size_t size)
{
    return pty_master_append_processed_locked(pair, in, size);
}

static size_t pts_write(void *file, const void *addr, size_t offset, size_t size)
{
    UNUSED(offset);
    pty_handle_t *pair = (pty_handle_t *)file;
    if (pair == NULL || addr == NULL) return VFS_STATUS_FAILED;
    if (size == 0) return 0;
    bool translate = (pair->term.c_oflag & OPOST) && (pair->term.c_oflag & ONLCR);
    size_t min_space = translate ? 2 : 1;

    while (true)
    {
        spin_lock(&pair->lock);
        if (pair->stop_master_output || pair->stop_slave_output)
        {
            spin_unlock(&pair->lock);
            if (pty_nonblock(pair, false)) return (size_t)-EWOULDBLOCK;
            scheduler_yield();
            continue;
        }
        if (pair->master_fds == 0)
        {
            spin_unlock(&pair->lock);
            return (size_t)-EIO;
        }
        if (PTY_BUFF_SIZE - pair->ptr_master >= min_space)
        {
            spin_unlock(&pair->lock);
            break;
        }
        if (pty_nonblock(pair, false))
        {
            spin_unlock(&pair->lock);
            return (size_t)-EWOULDBLOCK;
        }
        spin_unlock(&pair->lock);
        scheduler_yield();
    }

    spin_lock(&pair->lock);
    size_t written = pts_write_inner(pair, (const uint8_t *)addr, size);
    if (written > 0) pty_packet_mark_data_locked(pair);
    spin_unlock(&pair->lock);
    poll_notify_event();
    return written;
}

static errno_t pts_ioctl(void *handle, size_t request, void *arg)
{
    pty_handle_t *pair = (pty_handle_t *)handle;
    if (pair == NULL) return -ENOTTY;

    uint32_t req = (uint32_t)request;
    errno_t ret = EOK;
    spin_lock(&pair->lock);
    switch (req)
    {
    case TIOCGWINSZ:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        memcpy(arg, &pair->win, sizeof(pair->win));
        break;
    case TIOCSWINSZ:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        memcpy(&pair->win, arg, sizeof(pair->win));
        break;
    case TIOCSCTTY:
        pts_ctrl_assign(pair);
        break;
    case TCGETS:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        memcpy(arg, &pair->term, sizeof(pair->term));
        break;
    case TCGETS2:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        pty_termios_to_termios2(&pair->term, (struct termios2 *)arg);
        break;
    case TCSETS:
    case TCSETSW:
    case TCSETSF: {
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        struct termios old_term = pair->term;
        memcpy(&pair->term, arg, sizeof(pair->term));
        pty_packet_termios_changed_locked(pair, &old_term);
        if (req == TCSETSF) ret = pty_flush_locked(pair, TCIFLUSH);
        break;
    }
    case TCSETS2:
    case TCSETSW2:
    case TCSETSF2:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        pty_termios_from_termios2(&pair->term, (const struct termios2 *)arg);
        if (req == TCSETSF2) ret = pty_flush_locked(pair, TCIFLUSH);
        break;
    case TIOCGPGRP:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        *(int *)arg = pty_foreground_pgid(pair);
        break;
    case TIOCGDEV:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        *(uint32_t *)arg = (uint32_t)pty_mkdev(136, (uint32_t)pair->id);
        break;
    case FIONREAD:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        *(int *)arg = (int)pts_data_available(pair);
        break;
    case TIOCOUTQ:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        *(int *)arg = (int)pair->ptr_master;
        break;
    case TIOCEXCL:
    case TIOCNXCL:
    case TIOCSBRK:
    case TIOCCBRK:
    case TIOCSIG:
        ret = EOK;
        break;
    case TCXONC:
        ret = pty_tcxonc_locked(pair, false, (int)(uintptr_t)arg);
        break;
    case TCFLSH:
        ret = pty_flush_locked(pair, (int)(uintptr_t)arg);
        break;
    case TIOCPKT:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        pair->packet_mode = (*(int *)arg) != 0;
        pair->packet_status = 0;
        pair->packet_data_pending = pair->packet_mode && pair->ptr_master > 0;
        break;
    case TIOCGEXCL:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        *(int *)arg = 0;
        break;
    case TIOCGPKT:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        *(int *)arg = pair->packet_mode ? 1 : 0;
        break;
    case TIOCSPGRP:
        if (arg == NULL) ret = -EFAULT;
        else pair->front_pgid = *(int *)arg;
        break;
    case TIOCGETD:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        *(int *)arg = N_TTY;
        break;
    case TIOCSETD:
        break;
    case TIOCGSID:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        *(int *)arg = (int)pair->ctrl_session;
        break;
    case KDGKBMODE:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        *(int *)arg = pair->tty_kbmode;
        break;
    case KDSKBMODE:
        pair->tty_kbmode = (int)(uintptr_t)arg;
        break;
    case VT_SETMODE:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        memcpy(&pair->vt_mode, arg, sizeof(pair->vt_mode));
        break;
    case VT_GETMODE:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        memcpy(arg, &pair->vt_mode, sizeof(pair->vt_mode));
        break;
    case VT_ACTIVATE:
    case VT_WAITACTIVE:
    case TIOCNOTTY:
        break;
    case VT_GETSTATE:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        ((struct vt_state *)arg)->v_active = 2;
        ((struct vt_state *)arg)->v_state  = 0;
        break;
    case VT_OPENQRY:
        if (arg == NULL)
        {
            ret = -EFAULT;
            break;
        }
        *(int *)arg = 2;
        break;
    default:
        ret = -ENOTTY;
        break;
    }
    spin_unlock(&pair->lock);
    poll_notify_event();
    return ret;
}

static errno_t pts_poll(void *file, size_t events)
{
    pty_handle_t *pair = (pty_handle_t *)file;
    if (pair == NULL) return VFS_STATUS_FAILED;

    int revents = 0;
    spin_lock(&pair->lock);
    if ((pair->master_fds == 0 || pts_data_available(pair) > 0) && (events & EPOLLIN)) revents |= EPOLLIN;
    if (pair->master_fds == 0) revents |= EPOLLHUP;
    if (!pair->stop_master_output && !pair->stop_slave_output && pair->ptr_master < PTY_BUFF_SIZE &&
        (events & EPOLLOUT))
        revents |= EPOLLOUT;
    spin_unlock(&pair->lock);
    return revents;
}

static vfs_node_t pts_dup(vfs_node_t node)
{
    pty_handle_t *pair = (pty_handle_t *)node->handle;
    if (pair != NULL)
    {
        spin_lock(&pair->lock);
        pair->slave_fds++;
        spin_unlock(&pair->lock);
    }
    return node;
}

static errno_t pty_stat(void *handle, vfs_node_t node)
{
    UNUSED(handle);
    node->size = (uint64_t)-1;
    return EOK;
}

static struct vfs_callback ptmx_callbacks = {
    .mount    = (vfs_mount_t)pty_dummy,
    .unmount  = (vfs_unmount_t)pty_dummy,
    .open     = ptmx_open,
    .close    = ptmx_close,
    .read     = ptmx_read,
    .write    = ptmx_write,
    .readlink = (vfs_readlink_t)pty_dummy,
    .mkdir    = (vfs_mk_t)pty_dummy,
    .mkfile   = (vfs_mk_t)pty_dummy,
    .link     = (vfs_mk_t)pty_dummy,
    .symlink  = (vfs_mk_t)pty_dummy,
    .stat     = pty_stat,
    .ioctl    = ptmx_ioctl,
    .dup      = ptmx_dup,
    .poll     = ptmx_poll,
    .map      = (vfs_mapfile_t)pty_dummy,
    .resize   = (vfs_resize_t)pty_dummy,
    .del      = (vfs_del_t)pty_dummy,
    .rename   = (vfs_rename_t)pty_dummy,
};

static struct vfs_callback pts_callbacks = {
    .mount    = (vfs_mount_t)pty_dummy,
    .unmount  = (vfs_unmount_t)pty_dummy,
    .open     = pts_open,
    .close    = pts_close,
    .read     = pts_read,
    .write    = pts_write,
    .readlink = (vfs_readlink_t)pty_dummy,
    .mkdir    = (vfs_mk_t)pty_dummy,
    .mkfile   = (vfs_mk_t)pty_dummy,
    .link     = (vfs_mk_t)pty_dummy,
    .symlink  = (vfs_mk_t)pty_dummy,
    .stat     = pty_stat,
    .ioctl    = pts_ioctl,
    .dup      = pts_dup,
    .poll     = pts_poll,
    .map      = (vfs_mapfile_t)pty_dummy,
    .resize   = (vfs_resize_t)pty_dummy,
    .del      = (vfs_del_t)pty_dummy,
    .rename   = (vfs_rename_t)pty_dummy,
};

void pty_init()
{
    if (pty_allocator != NULL) return;

    pty_allocator = id_allocator_create(MAX_PTY_DEVICE);
    llist_init_head(&ptmx_list_head);

    ptmx_fsid = vfs_regist("ptyfs_master", &ptmx_callbacks, 0, 0x50544d58);
    pts_fsid  = vfs_regist("ptyfs_slave", &pts_callbacks, 0, 0x50545346);
    if (pty_allocator == NULL || ptmx_fsid == VFS_STATUS_FAILED || pts_fsid == VFS_STATUS_FAILED)
    {
        write_serial_fmt("pty: init failed\n");
        return;
    }

    vfs_node_t dev = vfs_open("/dev");
    if (dev == NULL)
    {
        write_serial_fmt("pty: /dev not ready\n");
        return;
    }

    vfs_node_t ptmx = vfs_do_search(dev, "ptmx");
    if (ptmx == NULL) ptmx = vfs_node_alloc(dev, "ptmx");
    if (ptmx != NULL)
    {
        ptmx->type   = file_ptmx;
        ptmx->fsid   = ptmx_fsid;
        ptmx->handle = NULL;
        ptmx->mode   = 0666;
        ptmx->size   = (uint64_t)-1;
        ptmx->rdev   = pty_mkdev(5, 2);
    }

    vfs_mkdir("/dev/pts");
    pts_root = vfs_open("/dev/pts");
    if (pts_root != NULL)
    {
        pts_root->type   = file_dir;
        pts_root->fsid   = pts_fsid;
        pts_root->handle = pts_root;
        pts_root->mode   = 0755;
        pts_root->rdev   = 0;
    }

    vfs_close(dev);
    write_serial_fmt("pty: initialized\n");
}
