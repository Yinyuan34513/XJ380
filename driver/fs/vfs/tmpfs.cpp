#define ALL_IMPLEMENTATION
#include <cpu/lock.h>
#include <errno.h>
#include <fs/vfs/vfs.h>
#include <mm/hhdm.h>
#include <mm/page.h>
#include <proto.hpp>
#include <syscall/syscall.h>
#include <task/pcb.h>

typedef struct tmpfs_entry {
    vfs_node_t          node;
    struct tmpfs_entry *parent;
    struct tmpfs_entry *next;
    char               *name;
    size_t              size;
    uint64_t           *pages;
    size_t              page_count;
    size_t              page_capacity;
    bool                is_dir;
    uint64_t            inode;
} tmpfs_entry_t;

static int           tmpfs_id       = 0;
static uint64_t      tmpfs_next_ino = 2;
static spin_t        tmpfs_lock     = SPIN_INIT;
static tmpfs_entry_t *tmpfs_entries = NULL;

static tmpfs_entry_t *tmpfs_find_by_node(vfs_node_t node)
{
    for (tmpfs_entry_t *entry = tmpfs_entries; entry != NULL; entry = entry->next) {
        if (entry->node == node) return entry;
    }
    return NULL;
}

static tmpfs_entry_t *tmpfs_find_child(tmpfs_entry_t *parent, const char *name)
{
    if (parent == NULL || name == NULL) return NULL;
    for (tmpfs_entry_t *entry = tmpfs_entries; entry != NULL; entry = entry->next) {
        if (entry->parent == parent && entry->name != NULL && strcmp(entry->name, name) == 0) {
            return entry;
        }
    }
    return NULL;
}

static tmpfs_entry_t *tmpfs_alloc_entry(vfs_node_t node, tmpfs_entry_t *parent, const char *name, bool is_dir)
{
    tmpfs_entry_t *entry = (tmpfs_entry_t *)calloc(1, sizeof(tmpfs_entry_t));
    if (entry == NULL) return NULL;

    entry->node   = node;
    entry->parent = parent;
    entry->name   = strdup(name != NULL ? name : "");
    if (entry->name == NULL) {
        free(entry);
        return NULL;
    }
    entry->is_dir = is_dir;
    entry->inode  = tmpfs_next_ino++;
    entry->next   = tmpfs_entries;
    tmpfs_entries = entry;
    return entry;
}

static void tmpfs_unlink_entry(tmpfs_entry_t *target)
{
    tmpfs_entry_t **cursor = &tmpfs_entries;
    while (*cursor != NULL) {
        if (*cursor == target) {
            *cursor = target->next;
            return;
        }
        cursor = &(*cursor)->next;
    }
}

static bool tmpfs_has_child(tmpfs_entry_t *parent)
{
    for (tmpfs_entry_t *entry = tmpfs_entries; entry != NULL; entry = entry->next) {
        if (entry->parent == parent) return true;
    }
    return false;
}

static char *tmpfs_basename_dup(const char *path)
{
    if (path == NULL) return NULL;
    const char *slash = strrchr(path, '/');
    return strdup(slash != NULL ? slash + 1 : path);
}

static size_t tmpfs_pages_for_size(size_t size)
{
    return size == 0 ? 0 : (size + PAGE_SIZE - 1) / PAGE_SIZE;
}

static bool tmpfs_reserve_page_slots(tmpfs_entry_t *entry, size_t page_count)
{
    if (page_count <= entry->page_capacity) return true;

    size_t new_capacity = entry->page_capacity == 0 ? 4 : entry->page_capacity;
    while (new_capacity < page_count) {
        if (new_capacity > ((size_t)-1) / 2) return false;
        new_capacity *= 2;
    }

    uint64_t *new_pages = (uint64_t *)realloc(entry->pages, new_capacity * sizeof(uint64_t));
    if (new_pages == NULL) return false;
    memset(new_pages + entry->page_capacity, 0, (new_capacity - entry->page_capacity) * sizeof(uint64_t));
    entry->pages         = new_pages;
    entry->page_capacity = new_capacity;
    return true;
}

static bool tmpfs_ensure_pages(tmpfs_entry_t *entry, size_t size)
{
    size_t needed_pages = tmpfs_pages_for_size(size);
    if (needed_pages <= entry->page_count) return true;
    if (!tmpfs_reserve_page_slots(entry, needed_pages)) return false;

    size_t old_page_count = entry->page_count;
    for (size_t i = old_page_count; i < needed_pages; i++) {
        uint64_t frame = alloc_frames(1);
        if (frame == 0) {
            for (size_t j = old_page_count; j < i; j++) {
                free_frames(entry->pages[j], 1);
                entry->pages[j] = 0;
            }
            return false;
        }
        memset(phys_to_virt(frame), 0, PAGE_SIZE);
        entry->pages[i] = frame;
    }

    entry->page_count = needed_pages;
    return true;
}

static void tmpfs_copy_from_pages(tmpfs_entry_t *entry, void *addr, size_t offset, size_t size)
{
    uint8_t *dst = (uint8_t *)addr;
    size_t copied = 0;
    while (copied < size) {
        size_t absolute = offset + copied;
        size_t page_index = absolute / PAGE_SIZE;
        size_t page_offset = absolute & (PAGE_SIZE - 1);
        size_t chunk = MIN(size - copied, PAGE_SIZE - page_offset);
        uint8_t *src = (uint8_t *)phys_to_virt(entry->pages[page_index]);
        memcpy(dst + copied, src + page_offset, chunk);
        copied += chunk;
    }
}

static void tmpfs_copy_to_pages(tmpfs_entry_t *entry, const void *addr, size_t offset, size_t size)
{
    const uint8_t *src = (const uint8_t *)addr;
    size_t copied = 0;
    while (copied < size) {
        size_t absolute = offset + copied;
        size_t page_index = absolute / PAGE_SIZE;
        size_t page_offset = absolute & (PAGE_SIZE - 1);
        size_t chunk = MIN(size - copied, PAGE_SIZE - page_offset);
        uint8_t *dst = (uint8_t *)phys_to_virt(entry->pages[page_index]);
        memcpy(dst + page_offset, src + copied, chunk);
        copied += chunk;
    }
}

static void tmpfs_zero_range(tmpfs_entry_t *entry, size_t start, size_t end)
{
    size_t cleared = 0;
    size_t size = end - start;
    while (cleared < size) {
        size_t absolute = start + cleared;
        size_t page_index = absolute / PAGE_SIZE;
        size_t page_offset = absolute & (PAGE_SIZE - 1);
        size_t chunk = MIN(size - cleared, PAGE_SIZE - page_offset);
        uint8_t *dst = (uint8_t *)phys_to_virt(entry->pages[page_index]);
        memset(dst + page_offset, 0, chunk);
        cleared += chunk;
    }
}

static void tmpfs_apply_stat(tmpfs_entry_t *entry, vfs_node_t node)
{
    node->fsid  = tmpfs_id;
    node->type  = entry->is_dir ? file_dir : file_none;
    node->size  = entry->is_dir ? 0 : entry->size;
    node->inode = entry->inode;
    node->blksz = PAGE_SIZE;
    node->dev   = node->root != NULL ? node->root->dev : 0;
    node->mode  = node->mode == 0 ? (entry->is_dir ? 0777 : 0666) : node->mode;
}

static errno_t tmpfs_mount(const char *src, vfs_node_t node)
{
    if ((uint64_t)src != TMPFS_REGISTER_ID) return VFS_STATUS_FAILED;

    spin_lock(&tmpfs_lock);
    tmpfs_entry_t *entry = tmpfs_alloc_entry(node, NULL, node->name != NULL ? node->name : "", true);
    if (entry == NULL) {
        spin_unlock(&tmpfs_lock);
        return VFS_STATUS_FAILED;
    }
    node->handle = entry;
    node->fsid   = tmpfs_id;
    node->type   = file_dir;
    node->inode  = 1;
    node->blksz  = PAGE_SIZE;
    spin_unlock(&tmpfs_lock);
    return VFS_STATUS_SUCCESS;
}

static void tmpfs_open(void *parent, const char *name, vfs_node_t node)
{
    spin_lock(&tmpfs_lock);
    tmpfs_entry_t *entry = tmpfs_find_by_node(node);
    if (entry == NULL) {
        tmpfs_entry_t *parent_entry = (tmpfs_entry_t *)parent;
        entry = tmpfs_find_child(parent_entry, name);
        if (entry != NULL) entry->node = node;
    }
    if (entry != NULL) {
        node->handle = entry;
        tmpfs_apply_stat(entry, node);
    }
    spin_unlock(&tmpfs_lock);
}

static errno_t tmpfs_stat(void *file, vfs_node_t node)
{
    if (file == NULL || node == NULL) return VFS_STATUS_FAILED;
    spin_lock(&tmpfs_lock);
    tmpfs_entry_t *entry = (tmpfs_entry_t *)file;
    tmpfs_apply_stat(entry, node);
    spin_unlock(&tmpfs_lock);
    return EOK;
}

static errno_t tmpfs_mkdir(void *parent, const char *name, vfs_node_t node)
{
    spin_lock(&tmpfs_lock);
    tmpfs_entry_t *parent_entry = (tmpfs_entry_t *)parent;
    if (parent_entry == NULL || !parent_entry->is_dir || tmpfs_find_child(parent_entry, name) != NULL) {
        spin_unlock(&tmpfs_lock);
        return VFS_STATUS_FAILED;
    }
    tmpfs_entry_t *entry = tmpfs_alloc_entry(node, parent_entry, name, true);
    if (entry == NULL) {
        spin_unlock(&tmpfs_lock);
        return VFS_STATUS_FAILED;
    }
    node->handle = entry;
    tmpfs_apply_stat(entry, node);
    spin_unlock(&tmpfs_lock);
    return VFS_STATUS_SUCCESS;
}

static errno_t tmpfs_mkfile(void *parent, const char *name, vfs_node_t node)
{
    spin_lock(&tmpfs_lock);
    tmpfs_entry_t *parent_entry = (tmpfs_entry_t *)parent;
    if (parent_entry == NULL || !parent_entry->is_dir || tmpfs_find_child(parent_entry, name) != NULL) {
        spin_unlock(&tmpfs_lock);
        return VFS_STATUS_FAILED;
    }
    tmpfs_entry_t *entry = tmpfs_alloc_entry(node, parent_entry, name, false);
    if (entry == NULL) {
        spin_unlock(&tmpfs_lock);
        return VFS_STATUS_FAILED;
    }
    node->handle = entry;
    tmpfs_apply_stat(entry, node);
    spin_unlock(&tmpfs_lock);
    return VFS_STATUS_SUCCESS;
}

static size_t tmpfs_read(void *file, void *addr, size_t offset, size_t size)
{
    if (file == NULL || addr == NULL) return VFS_STATUS_FAILED;
    spin_lock(&tmpfs_lock);
    tmpfs_entry_t *entry = (tmpfs_entry_t *)file;
    if (entry->is_dir) {
        spin_unlock(&tmpfs_lock);
        return VFS_STATUS_FAILED;
    }
    if (offset >= entry->size) {
        spin_unlock(&tmpfs_lock);
        return 0;
    }
    size_t available = entry->size - offset;
    if (size > available) size = available;
    tmpfs_copy_from_pages(entry, addr, offset, size);
    spin_unlock(&tmpfs_lock);
    return size;
}

static bool tmpfs_reserve(tmpfs_entry_t *entry, size_t want)
{
    return tmpfs_ensure_pages(entry, want);
}

static size_t tmpfs_write(void *file, const void *addr, size_t offset, size_t size)
{
    if (file == NULL || addr == NULL) return VFS_STATUS_FAILED;
    spin_lock(&tmpfs_lock);
    tmpfs_entry_t *entry = (tmpfs_entry_t *)file;
    if (entry->is_dir || offset > ((size_t)-1) - size || !tmpfs_reserve(entry, offset + size)) {
        spin_unlock(&tmpfs_lock);
        return VFS_STATUS_FAILED;
    }
    tmpfs_copy_to_pages(entry, addr, offset, size);
    if (offset + size > entry->size) entry->size = offset + size;
    spin_unlock(&tmpfs_lock);
    return size;
}

static void tmpfs_resize(void *file, uint64_t size)
{
    spin_lock(&tmpfs_lock);
    tmpfs_entry_t *entry = (tmpfs_entry_t *)file;
    if (entry != NULL && !entry->is_dir && size <= (uint64_t)((size_t)-1) && tmpfs_reserve(entry, (size_t)size)) {
        if ((size_t)size > entry->size) tmpfs_zero_range(entry, entry->size, (size_t)size);
        else if ((size_t)size < entry->size) tmpfs_zero_range(entry, (size_t)size, entry->size);
        entry->size = (size_t)size;
    }
    spin_unlock(&tmpfs_lock);
}

static void *tmpfs_map(void *file, void *addr, size_t offset, size_t size, size_t prot, size_t flags)
{
    (void)flags;
    tmpfs_entry_t *entry = (tmpfs_entry_t *)file;
    if (entry == NULL || entry->is_dir) return (void *)SYSCALL_FAULT_(ENODEV);
    if ((offset & (PAGE_SIZE - 1)) != 0 || offset > (size_t)-1 - size)
        return (void *)SYSCALL_FAULT_(EINVAL);

    spin_lock(&tmpfs_lock);
    size_t end = offset + size;
    if (!tmpfs_ensure_pages(entry, end)) {
        spin_unlock(&tmpfs_lock);
        return (void *)SYSCALL_FAULT_(ENOMEM);
    }
    if (end > entry->size) entry->size = end;

    pcb_t process = get_current_task() != NULL ? get_current_task()->parent_group : NULL;
    if (process == NULL || process->pagedir == NULL) {
        spin_unlock(&tmpfs_lock);
        return (void *)SYSCALL_FAULT_(EFAULT);
    }

    uint64_t pte_flags = PTE_USER;
    if (prot != PROT_NONE) pte_flags |= PTE_PRESENT;
    if (prot & PROT_WRITE) pte_flags |= PTE_WRITEABLE;
    if (!(prot & PROT_EXEC)) pte_flags |= PTE_NO_EXECUTE;

    for (size_t pos = 0; pos < size; pos += PAGE_SIZE) {
        size_t page_index = (offset + pos) / PAGE_SIZE;
        page_map_to(process->pagedir, (uint64_t)addr + pos, entry->pages[page_index], pte_flags);
    }

    spin_unlock(&tmpfs_lock);
    return addr;
}

static errno_t tmpfs_delete(void *parent, vfs_node_t node)
{
    (void)parent;
    spin_lock(&tmpfs_lock);
    tmpfs_entry_t *entry = tmpfs_find_by_node(node);
    if (entry == NULL) {
        spin_unlock(&tmpfs_lock);
        return -ENOENT;
    }
    if (entry->is_dir && tmpfs_has_child(entry)) {
        spin_unlock(&tmpfs_lock);
        return -ENOTEMPTY;
    }
    tmpfs_unlink_entry(entry);
    free(entry->name);
    for (size_t i = 0; i < entry->page_count; i++) {
        if (entry->pages[i] != 0) free_frames(entry->pages[i], 1);
    }
    free(entry->pages);
    free(entry);
    node->handle = NULL;
    spin_unlock(&tmpfs_lock);
    return VFS_STATUS_SUCCESS;
}

static errno_t tmpfs_rename(void *current, const char *nw)
{
    if (current == NULL || nw == NULL) return VFS_STATUS_FAILED;
    char *new_name = tmpfs_basename_dup(nw);
    if (new_name == NULL || new_name[0] == '\0') {
        free(new_name);
        return VFS_STATUS_FAILED;
    }

    spin_lock(&tmpfs_lock);
    tmpfs_entry_t *entry = (tmpfs_entry_t *)current;
    tmpfs_entry_t *conflict = tmpfs_find_child(entry->parent, new_name);
    if (conflict != NULL && conflict != entry) {
        spin_unlock(&tmpfs_lock);
        free(new_name);
        return -EEXIST;
    }
    free(entry->name);
    entry->name = new_name;
    if (entry->node != NULL) {
        free(entry->node->name);
        entry->node->name = strdup(new_name);
    }
    spin_unlock(&tmpfs_lock);
    return VFS_STATUS_SUCCESS;
}

static int tmpfs_dummy()
{
    return -ENOSYS;
}

static void tmpfs_close(void *current)
{
    (void)current;
}

static errno_t tmpfs_poll(void *file, size_t events)
{
    (void)file;
    (void)events;
    return 0;
}

static vfs_node_t tmpfs_dup(vfs_node_t node)
{
    return node;
}

static struct vfs_callback tmpfs_callbacks = {
    .mount    = tmpfs_mount,
    .unmount  = (vfs_unmount_t)tmpfs_close,
    .open     = tmpfs_open,
    .close    = tmpfs_close,
    .read     = tmpfs_read,
    .write    = tmpfs_write,
    .readlink = (vfs_readlink_t)tmpfs_dummy,
    .mkdir    = tmpfs_mkdir,
    .mkfile   = tmpfs_mkfile,
    .link     = (vfs_mk_t)tmpfs_dummy,
    .symlink  = (vfs_mk_t)tmpfs_dummy,
    .stat     = tmpfs_stat,
    .ioctl    = (vfs_ioctl_t)tmpfs_dummy,
    .dup      = tmpfs_dup,
    .poll     = tmpfs_poll,
    .map      = tmpfs_map,
    .resize   = tmpfs_resize,
    .del      = tmpfs_delete,
    .rename   = tmpfs_rename,
};

int tmpfs_setup(void)
{
    tmpfs_id = vfs_regist("tmpfs", &tmpfs_callbacks, TMPFS_REGISTER_ID, 0x01021994);
    if (tmpfs_id == VFS_STATUS_FAILED) return -EIO;

    vfs_mkdir("/tmp");
    vfs_node_t tmp = vfs_open("/tmp");
    if (tmp == NULL) return -ENOENT;
    if (vfs_mount((const char *)TMPFS_REGISTER_ID, tmp) == VFS_STATUS_FAILED) return -EIO;
    vfs_close(tmp);

    vfs_mkdir("/run");
    vfs_node_t run = vfs_open("/run");
    if (run == NULL) return -ENOENT;
    if (vfs_mount((const char *)TMPFS_REGISTER_ID, run) == VFS_STATUS_FAILED) return -EIO;
    vfs_close(run);

    vfs_mkdir("/dev/shm");
    vfs_node_t shm = vfs_open("/dev/shm");
    if (shm == NULL) return -ENOENT;
    if (vfs_mount((const char *)TMPFS_REGISTER_ID, shm) == VFS_STATUS_FAILED) return -EIO;
    write_serial_fmt("tmpfs: mounted /dev/shm\n");
    vfs_close(shm);
    return 0;
}
