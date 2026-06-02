#include <device.h>
#include <efi/boot.h>
#include <errno.h>
#include <fs/fatfs/fatfs.h>
#include <fs/partition.h>
#include <fs/vfs/vfs.h>
#include <installer_mode.h>
#include <installer_protocol.h>
#include <ioctl.h>
#include <krlibc.h>
#include <mm/uaccess.h>
#include <proto.hpp>
#include <syscall/pxapi.h>
#include <syscall/xapi_user.h>
#include <task/pcb.h>
#include <task/scheduler.h>

extern device_t device_ctl[256];

static const uint8_t *installer_payload_virt(const BOOT_CONFIG *config, size_t *out_size);

typedef struct {
    uint8_t status;
    uint8_t first_chs[3];
    uint8_t type;
    uint8_t last_chs[3];
    uint32_t first_lba;
    uint32_t sector_count;
} __attribute__((packed)) installer_mbr_entry_t;

typedef struct {
    uint8_t reserved[446];
    installer_mbr_entry_t entry[4];
    uint16_t signature;
} __attribute__((packed)) installer_mbr_t;

static volatile int g_installer_running = 0;
static size_t       g_installer_target_disk = 0;
static uint32_t     g_installer_mode = XJ380_INSTALLER_MODE_FRESH;
static uint64_t     g_installer_components = XJ380_INSTALLER_COMPONENT_DEFAULT;
static xj380_installer_log g_installer_log;
static xj380_installer_progress g_installer_progress = {
    XJ380_INSTALLER_IDLE,
    0,
    0,
    "等待安装",
    "请选择目标硬盘，然后开始安装。",
};

typedef struct {
    size_t   disk_id;
    uint32_t mode;
    uint64_t components;
} installer_worker_args;

static void installer_log_text(const char *line)
{
    if (line == NULL) return;

    if (g_installer_log.count < XJ380_INSTALLER_LOG_LINES)
    {
        strncpy(g_installer_log.lines[g_installer_log.count], line, XJ380_INSTALLER_LOG_LEN - 1);
        g_installer_log.count++;
    }
    else
    {
        memmove(g_installer_log.lines[0], g_installer_log.lines[1],
                (XJ380_INSTALLER_LOG_LINES - 1) * XJ380_INSTALLER_LOG_LEN);
        memset(g_installer_log.lines[XJ380_INSTALLER_LOG_LINES - 1], 0, XJ380_INSTALLER_LOG_LEN);
        strncpy(g_installer_log.lines[XJ380_INSTALLER_LOG_LINES - 1], line, XJ380_INSTALLER_LOG_LEN - 1);
    }

    write_serial_string("installer-log: ");
    write_serial_string(line);
    write_serial_string("\n");
}

static void installer_log_fmt(const char *fmt, uint64_t a, uint64_t b, uint64_t c)
{
    char line[XJ380_INSTALLER_LOG_LEN];
    memset(line, 0, sizeof(line));
    snprintf(line, sizeof(line), fmt, a, b, c);
    installer_log_text(line);
}

static bool installer_device_is_slice_name(const char *name)
{
    if (name == NULL) return false;
    size_t len = strlen(name);
    if (len < 3) return false;
    size_t pos = len;
    while (pos > 0 && name[pos - 1] >= '0' && name[pos - 1] <= '9') pos--;
    return pos != len && pos > 0 && name[pos - 1] == 'p';
}

static bool installer_is_boot_media_name(const char *name)
{
    if (name == NULL) return false;
    return strncmp(name, "cd", 2) == 0 || strstr(name, "iso") != NULL || strstr(name, "sr") != NULL;
}

static bool installer_device_is_install_target(device_t *dev)
{
    if (dev == NULL || dev->flag < 1 || dev->type != DEVICE_BLOCK || dev->write == NULL) return false;
    if (dev->sector_size != 512 || dev->size == 0) return false;
    if (installer_device_is_slice_name(dev->drive_name) || installer_is_boot_media_name(dev->drive_name)) return false;
    return true;
}

static uint64_t installer_disk_flags(device_t *dev)
{
    uint64_t flags = 0;
    if (dev == NULL) return flags;
    if (dev->write != NULL) flags |= XJ380_INSTALLER_DISK_FLAG_WRITABLE;
    if (installer_is_boot_media_name(dev->drive_name)) flags |= XJ380_INSTALLER_DISK_FLAG_BOOT_MEDIA;
    if (installer_device_is_slice_name(dev->drive_name)) flags |= XJ380_INSTALLER_DISK_FLAG_SLICE;
    if (dev->sector_size == 512) flags |= XJ380_INSTALLER_DISK_FLAG_SECTOR_512;
    if (installer_device_is_install_target(dev)) flags |= XJ380_INSTALLER_DISK_FLAG_INSTALLABLE;
    return flags;
}

static const char *installer_mode_name(uint32_t mode)
{
    switch (mode)
    {
    case XJ380_INSTALLER_MODE_REPAIR_BOOT: return "仅修复引导";
    case XJ380_INSTALLER_MODE_KEEP_USERS: return "重装系统保留用户";
    case XJ380_INSTALLER_MODE_DEVELOPER: return "开发者安装";
    case XJ380_INSTALLER_MODE_FRESH:
    default: return "全新安装";
    }
}

static uint64_t installer_normalize_components(uint64_t components)
{
    components &= XJ380_INSTALLER_COMPONENT_ALL;
    if ((components & XJ380_INSTALLER_COMPONENT_LINUX_COMPAT) == 0)
        components = 0;
    return components;
}

static void installer_set_progress(uint32_t state, uint32_t percent, int64_t result,
                                    const char *stage, const char *detail)
{
    if (percent > 100) percent = 100;
    memset(&g_installer_progress, 0, sizeof(g_installer_progress));
    g_installer_progress.state = state;
    g_installer_progress.percent = percent;
    g_installer_progress.total_percent = percent;
    g_installer_progress.result = result;
    g_installer_progress.mode = g_installer_mode;
    if (stage != NULL) strncpy(g_installer_progress.stage, stage, sizeof(g_installer_progress.stage) - 1);
    if (detail != NULL) strncpy(g_installer_progress.detail, detail, sizeof(g_installer_progress.detail) - 1);
}

static int installer_fail(int ret, uint32_t percent, const char *stage, const char *detail)
{
    installer_set_progress(XJ380_INSTALLER_FAILED, percent, ret, stage, detail);
    write_serial_fmt("installer: failed: ret=%d stage=%s detail=%s\n",
                     ret,
                     stage != NULL ? stage : "",
                     detail != NULL ? detail : "");
    return ret;
}

static bool installer_write_sectors(size_t disk_id, uint64_t lba, const void *data, size_t sectors)
{
    if (data == NULL && sectors != 0) return false;
    size_t got = device_write((size_t)lba, sectors, data, (int)disk_id);
    if (got != sectors)
    {
        device_t *disk = get_device(disk_id);
        write_serial_fmt("installer: disk write short disk=%s id=%zu lba=%llu sectors=%zu got=%zu\n",
                         disk != NULL ? disk->drive_name : "<missing>", disk_id, lba, sectors, got);
        return false;
    }
    return true;
}

static bool installer_zero_sectors(size_t disk_id, uint64_t lba, size_t sectors)
{
    device_t *disk = get_device(disk_id);
    if (disk == NULL || disk->sector_size == 0) return false;

    size_t chunk_sectors = 1024;
    size_t bytes = chunk_sectors * disk->sector_size;
    uint8_t *zero = (uint8_t *)calloc(bytes, 1);
    if (zero == NULL) return false;

    size_t done = 0;
    while (done < sectors)
    {
        size_t chunk = MIN(chunk_sectors, sectors - done);
        if (!installer_write_sectors(disk_id, lba + done, zero, chunk))
        {
            free(zero);
            return false;
        }
        done += chunk;
    }

    free(zero);
    return true;
}

static uint32_t installer_crc32(const uint8_t *data, size_t size)
{
    uint32_t crc = 0xffffffffU;
    for (size_t i = 0; i < size; i++)
    {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++)
        {
            uint32_t mask = (uint32_t)-(int32_t)(crc & 1U);
            crc = (crc >> 1) ^ (0xedb88320U & mask);
        }
    }
    return ~crc;
}

static void installer_fill_guid(uint8_t guid[16], uint64_t seed)
{
    uint64_t a = 0x584a333830554546ULL ^ seed ^ nanoTime();
    uint64_t b = 0x49534f5041594c44ULL ^ (seed << 7) ^ ((uint64_t)(uintptr_t)guid >> 3);
    for (int i = 0; i < 8; i++) guid[i] = (uint8_t)(a >> (i * 8));
    for (int i = 0; i < 8; i++) guid[i + 8] = (uint8_t)(b >> (i * 8));
    guid[7] = (uint8_t)((guid[7] & 0x0f) | 0x40);
    guid[8] = (uint8_t)((guid[8] & 0x3f) | 0x80);
}

static bool installer_write_gpt(size_t disk_id, uint64_t first_lba, uint64_t last_lba)
{
    device_t *disk = get_device(disk_id);
    if (disk == NULL || disk->sector_size < 512 || disk->size == 0) return false;

    uint64_t total_lba = disk->size / disk->sector_size;
    if (total_lba < 4096 || first_lba >= last_lba || last_lba >= total_lba - 33) return false;

    uint8_t *sector = (uint8_t *)calloc(disk->sector_size, 1);
    if (sector == NULL) return false;

    installer_mbr_t *mbr = (installer_mbr_t *)sector;
    mbr->entry[0].type = 0xee;
    mbr->entry[0].first_chs[0] = 0x00;
    mbr->entry[0].first_chs[1] = 0x02;
    mbr->entry[0].first_chs[2] = 0x00;
    mbr->entry[0].last_chs[0] = 0xff;
    mbr->entry[0].last_chs[1] = 0xff;
    mbr->entry[0].last_chs[2] = 0xff;
    mbr->entry[0].first_lba = 1;
    mbr->entry[0].sector_count = total_lba > 0xffffffffULL ? 0xffffffffU : (uint32_t)(total_lba - 1);
    mbr->signature = 0xaa55;
    bool ok = installer_write_sectors(disk_id, 0, sector, 1);
    free(sector);
    if (!ok) return false;

    const uint32_t entry_count = 128;
    const uint32_t entry_size = sizeof(GPT_DPTE);
    const size_t entries_bytes = entry_count * entry_size;
    const size_t entries_sectors = PADDING_UP(entries_bytes, disk->sector_size) / disk->sector_size;
    GPT_DPTE *entries = (GPT_DPTE *)calloc(entries_sectors * disk->sector_size, 1);
    if (entries == NULL) return false;

    static const uint8_t esp_guid[16] = {
        0x28, 0x73, 0x2a, 0xc1, 0x1f, 0xf8, 0xd2, 0x11,
        0xba, 0x4b, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b
    };
    memcpy(entries[0].partition_type_guid, esp_guid, sizeof(esp_guid));
    installer_fill_guid(entries[0].unique_partition_guid, disk_id ^ first_lba ^ last_lba);
    entries[0].starting_lba = first_lba;
    entries[0].ending_lba = last_lba;
    const char label[] = "XJ380";
    for (size_t i = 0; i < sizeof(label) - 1 && i < 36; i++) entries[0].partition_name[i] = label[i];

    uint32_t entries_crc = installer_crc32((const uint8_t *)entries, entries_bytes);
    ok = installer_write_sectors(disk_id, 2, entries, entries_sectors);
    if (ok) ok = installer_write_sectors(disk_id, total_lba - 1 - entries_sectors, entries, entries_sectors);
    free(entries);
    if (!ok) return false;

    sector = (uint8_t *)calloc(disk->sector_size, 1);
    if (sector == NULL) return false;
    GPT_DPT *primary = (GPT_DPT *)sector;
    memcpy(primary->signature, GPT_HEADER_SIGNATURE, 8);
    primary->revision = 0x00010000;
    primary->header_size = sizeof(GPT_DPT);
    primary->my_lba = 1;
    primary->alternate_lba = total_lba - 1;
    primary->first_usable_lba = first_lba;
    primary->last_usable_lba = last_lba;
    installer_fill_guid(primary->disk_guid, disk_id ^ total_lba);
    primary->partition_entry_lba = 2;
    primary->num_partition_entries = entry_count;
    primary->size_of_partition_entry = entry_size;
    primary->partition_entry_array_crc32 = entries_crc;
    primary->header_crc32 = 0;
    primary->header_crc32 = installer_crc32((const uint8_t *)primary, primary->header_size);
    ok = installer_write_sectors(disk_id, 1, sector, 1);

    memset(sector, 0, disk->sector_size);
    GPT_DPT *backup = (GPT_DPT *)sector;
    *backup = *primary;
    backup->my_lba = total_lba - 1;
    backup->alternate_lba = 1;
    backup->partition_entry_lba = total_lba - 1 - entries_sectors;
    backup->header_crc32 = 0;
    backup->header_crc32 = installer_crc32((const uint8_t *)backup, backup->header_size);
    if (ok) ok = installer_write_sectors(disk_id, total_lba - 1, sector, 1);
    free(sector);
    return ok;
}

typedef struct {
    char  **items;
    size_t  capacity;
    size_t  count;
} installer_dir_cache;

typedef struct {
    char    **items;
    uint64_t  count;
} installer_file_list;

typedef struct {
    installer_dir_cache dirs;
    installer_file_list  files;
    uint64_t            total_file_bytes;
    uint64_t            total_file_count;
    uint64_t            small_file_count;
    uint64_t            large_file_count;
    uint64_t            copied_small_file_count;
    uint64_t            copied_large_file_count;
    uint64_t            copied_file_bytes;
    uint64_t            current_file_size;
    uint64_t            current_file_written;
    uint64_t            current_file_index;
    uint64_t            copy_start_ns;
    uint32_t            last_percent;
    uint32_t            chunk_counter;
    char                last_parent[640];
} installer_copy_context;

static uint64_t installer_path_hash(const char *path)
{
    uint64_t hash = 1469598103934665603ULL;
    if (path == NULL) return hash;
    for (const unsigned char *p = (const unsigned char *)path; *p != '\0'; p++)
    {
        hash ^= *p;
        hash *= 1099511628211ULL;
    }
    return hash;
}

static size_t installer_dir_cache_capacity(uint64_t entry_count)
{
    size_t cap = 64;
    uint64_t want = entry_count < 131072ULL ? entry_count * 2ULL + 32ULL : 262144ULL;
    while ((uint64_t)cap < want && cap < 262144) cap <<= 1;
    return cap;
}

static void installer_dir_cache_init(installer_dir_cache *cache, uint64_t entry_count)
{
    if (cache == NULL) return;
    memset(cache, 0, sizeof(*cache));
    size_t cap = installer_dir_cache_capacity(entry_count);
    cache->items = (char **)calloc(cap, sizeof(char *));
    if (cache->items == NULL)
    {
        cache->capacity = 0;
        return;
    }
    cache->capacity = cap;
}

static void installer_dir_cache_free(installer_dir_cache *cache)
{
    if (cache == NULL || cache->items == NULL) return;
    for (size_t i = 0; i < cache->capacity; i++)
    {
        if (cache->items[i] != NULL) free(cache->items[i]);
    }
    free(cache->items);
    memset(cache, 0, sizeof(*cache));
}

static bool installer_dir_cache_contains(installer_dir_cache *cache, const char *path)
{
    if (cache == NULL || cache->items == NULL || cache->capacity == 0 || path == NULL) return false;
    size_t mask = cache->capacity - 1;
    size_t pos = (size_t)installer_path_hash(path) & mask;
    for (size_t probe = 0; probe < cache->capacity; probe++)
    {
        char *item = cache->items[pos];
        if (item == NULL) return false;
        if (strcmp(item, path) == 0) return true;
        pos = (pos + 1) & mask;
    }
    return false;
}

static void installer_dir_cache_add(installer_dir_cache *cache, const char *path)
{
    if (cache == NULL || cache->items == NULL || cache->capacity == 0 || path == NULL) return;
    if (cache->count * 2 >= cache->capacity) return;

    size_t mask = cache->capacity - 1;
    size_t pos = (size_t)installer_path_hash(path) & mask;
    for (size_t probe = 0; probe < cache->capacity; probe++)
    {
        char *item = cache->items[pos];
        if (item == NULL)
        {
            char *copy = strdup(path);
            if (copy == NULL) return;
            cache->items[pos] = copy;
            cache->count++;
            return;
        }
        if (strcmp(item, path) == 0) return;
        pos = (pos + 1) & mask;
    }
}

static void installer_file_list_free(installer_file_list *list)
{
    if (list == NULL || list->items == NULL) return;
    for (uint64_t i = 0; i < list->count; i++)
    {
        free(list->items[i]);
    }
    free(list->items);
    memset(list, 0, sizeof(*list));
}

static bool installer_file_list_init(installer_file_list *list, uint64_t count)
{
    if (list == NULL) return false;
    memset(list, 0, sizeof(*list));
    if (count == 0) return true;
    list->items = (char **)calloc((size_t)count, sizeof(char *));
    if (list->items == NULL) return false;
    list->count = count;
    return true;
}

static bool installer_should_skip_payload_path(const char *rel, uint32_t mode, uint64_t components);

static int installer_collect_file_list(const uint8_t *pak, size_t pak_size, installer_file_list *list,
                                       uint32_t mode, uint64_t components)
{
    if (pak == NULL || pak_size < sizeof(xj380_pak_header) || list == NULL) return -EINVAL;
    const xj380_pak_header *header = (const xj380_pak_header *)pak;
    if (memcmp(header->magic, XJ380_INSTALLER_PAK_MAGIC, XJ380_INSTALLER_PAK_MAGIC_SIZE) != 0) return -EINVAL;
    if (list->count == 0) return EOK;
    if (list->items == NULL) return -ENOMEM;

    size_t   off  = sizeof(*header);
    uint64_t file = 0;
    for (uint64_t i = 0; i < header->entry_count; i++)
    {
        if (off > pak_size || pak_size - off < sizeof(xj380_pak_entry_header)) return -EINVAL;
        const xj380_pak_entry_header *entry = (const xj380_pak_entry_header *)(pak + off);
        off += sizeof(*entry);
        if (entry->path_len == 0 || entry->path_len >= 512 || off > pak_size || pak_size - off < entry->path_len)
            return -EINVAL;

        char rel[512];
        memcpy(rel, pak + off, entry->path_len);
        rel[entry->path_len] = '\0';
        off += entry->path_len;
        if (rel[0] != '/') return -EINVAL;
        bool skip = installer_should_skip_payload_path(rel, mode, components);

        if (entry->type == XJ380_PAK_ENTRY_FILE)
        {
            if (entry->size > pak_size - off) return -EINVAL;
            if (!skip)
            {
                if (file >= list->count) return -EINVAL;
                list->items[file] = strdup(rel);
                if (list->items[file] == NULL) return -ENOMEM;
                file++;
            }
            off += (size_t)entry->size;
        }
        else if (entry->type == XJ380_PAK_ENTRY_DIR)
        {
            continue;
        }
        else
        {
            return -EINVAL;
        }
    }

    if (file != list->count) return -EINVAL;
    return EOK;
}

static int installer_mkdir_cached(installer_copy_context *ctx, const char *path)
{
    if (path == NULL || path[0] != '/') return -EINVAL;
    if (path[1] == '\0') return EOK;
    if (ctx != NULL && installer_dir_cache_contains(&ctx->dirs, path)) return EOK;

    char temp[640];
    size_t len = strlen(path);
    if (len >= sizeof(temp)) return -ENAMETOOLONG;
    strcpy(temp, path);

    for (size_t i = 1; temp[i] != '\0'; i++)
    {
        if (temp[i] != '/') continue;
        temp[i] = '\0';
        if (temp[0] != '\0' && (ctx == NULL || !installer_dir_cache_contains(&ctx->dirs, temp)))
        {
            vfs_mkdir(temp);
            if (ctx != NULL) installer_dir_cache_add(&ctx->dirs, temp);
        }
        temp[i] = '/';
    }

    if (ctx == NULL || !installer_dir_cache_contains(&ctx->dirs, temp))
    {
        vfs_mkdir(temp);
        if (ctx != NULL) installer_dir_cache_add(&ctx->dirs, temp);
    }
    return EOK;
}

static int installer_prepare_payload_parent_cached(installer_copy_context *ctx, const char *path)
{
    if (path == NULL || path[0] != '/') return -EINVAL;
    char parent[640];
    size_t len = strlen(path);
    if (len >= sizeof(parent)) return -ENAMETOOLONG;
    strcpy(parent, path);

    char *last = strrchr(parent, '/');
    if (last == NULL) return -EINVAL;
    if (last == parent) return EOK;
    *last = '\0';

    if (ctx != NULL && strcmp(ctx->last_parent, parent) == 0) return EOK;
    int ret = installer_mkdir_cached(ctx, parent);
    if (ret == EOK && ctx != NULL)
    {
        strncpy(ctx->last_parent, parent, sizeof(ctx->last_parent) - 1);
        ctx->last_parent[sizeof(ctx->last_parent) - 1] = '\0';
    }
    return ret;
}

static void installer_progress_sync_queue(installer_copy_context *ctx)
{
    if (ctx == NULL)
    {
        g_installer_progress.queue_index = 0;
        g_installer_progress.queue_total = 0;
        g_installer_progress.queue_count = 0;
        g_installer_progress.stage_percent = 0;
        g_installer_progress.bytes_per_second = 0;
        g_installer_progress.eta_seconds = 0;
        g_installer_progress.copied_bytes = 0;
        g_installer_progress.total_bytes = 0;
        g_installer_progress.current_file_bytes = 0;
        g_installer_progress.current_file_size = 0;
        memset(g_installer_progress.queue_items, 0, sizeof(g_installer_progress.queue_items));
        return;
    }

    memset(g_installer_progress.queue_items, 0, sizeof(g_installer_progress.queue_items));
    g_installer_progress.queue_count = 0;
    g_installer_progress.queue_total = (xj380_inst_u32)MIN(ctx->total_file_count, 0xffffffffULL);
    g_installer_progress.queue_index = (xj380_inst_u32)MIN(ctx->current_file_index + 1, ctx->total_file_count);
    g_installer_progress.small_file_count = (xj380_inst_u32)MIN(ctx->small_file_count, 0xffffffffULL);
    g_installer_progress.large_file_count = (xj380_inst_u32)MIN(ctx->large_file_count, 0xffffffffULL);
    g_installer_progress.copied_small_file_count = (xj380_inst_u32)MIN(ctx->copied_small_file_count, 0xffffffffULL);
    g_installer_progress.copied_large_file_count = (xj380_inst_u32)MIN(ctx->copied_large_file_count, 0xffffffffULL);
    g_installer_progress.copied_bytes = ctx->copied_file_bytes;
    g_installer_progress.total_bytes = ctx->total_file_bytes;
    g_installer_progress.current_file_bytes = ctx->current_file_written;
    g_installer_progress.current_file_size = ctx->current_file_size;
    if (ctx->total_file_bytes > 0)
        g_installer_progress.stage_percent = (xj380_inst_u32)MIN((ctx->copied_file_bytes * 100ULL) / ctx->total_file_bytes, 100ULL);
    else
        g_installer_progress.stage_percent = 0;

    uint64_t now = nanoTime();
    if (ctx->copy_start_ns != 0 && now > ctx->copy_start_ns)
    {
        uint64_t elapsed_ns = now - ctx->copy_start_ns;
        if (elapsed_ns > 0)
        {
            g_installer_progress.bytes_per_second = (ctx->copied_file_bytes * 1000000000ULL) / elapsed_ns;
            if (g_installer_progress.bytes_per_second > 0 && ctx->total_file_bytes > ctx->copied_file_bytes)
                g_installer_progress.eta_seconds = (ctx->total_file_bytes - ctx->copied_file_bytes) / g_installer_progress.bytes_per_second;
            else
                g_installer_progress.eta_seconds = 0;
        }
    }
    if (ctx->files.items == NULL || ctx->files.count == 0) return;
    if (ctx->current_file_index >= ctx->files.count) return;

    uint32_t slot = 0;
    for (uint64_t i = ctx->current_file_index; i < ctx->files.count && slot < XJ380_INSTALLER_QUEUE_ITEMS; i++)
    {
        const char *item = ctx->files.items[i];
        if (item == NULL || item[0] == '\0') continue;
        strncpy(g_installer_progress.queue_items[slot], item, XJ380_INSTALLER_QUEUE_LEN - 1);
        g_installer_progress.queue_items[slot][XJ380_INSTALLER_QUEUE_LEN - 1] = '\0';
        slot++;
    }
    g_installer_progress.queue_count = slot;
}

static uint32_t installer_copy_percent(installer_copy_context *ctx)
{
    if (ctx == NULL || ctx->total_file_bytes == 0) return 55;
    uint32_t copy_percent = (uint32_t)((ctx->copied_file_bytes * 40ULL) / ctx->total_file_bytes);
    if (copy_percent > 40) copy_percent = 40;
    return 55 + copy_percent;
}

static void installer_copy_progress(installer_copy_context *ctx, const char *detail, bool force)
{
    if (ctx == NULL) return;
    installer_progress_sync_queue(ctx);
    uint32_t percent = installer_copy_percent(ctx);
    if (!force && percent == ctx->last_percent) return;
    ctx->last_percent = percent;
    installer_set_progress(XJ380_INSTALLER_RUNNING, percent, 0, "正在复制系统文件", detail);
    g_installer_progress.stage_percent = ctx->total_file_bytes == 0 ? 0 :
        (xj380_inst_u32)MIN((ctx->copied_file_bytes * 100ULL) / ctx->total_file_bytes, 100ULL);
    installer_progress_sync_queue(ctx);
}

static int installer_write_payload_file(installer_copy_context *ctx, const char *path,
                                        const uint8_t *data, size_t size, const char *detail)
{
    static const size_t INSTALLER_COPY_CHUNK = 0x400000;

    int ret = installer_prepare_payload_parent_cached(ctx, path);
    if (ret < 0) return ret;
    ret = vfs_mkfile(path);
    if (ret != EOK)
    {
        write_serial_fmt("installer: mkfile failed path=%s ret=%d\n", path, ret);
        return ret < 0 ? ret : -EIO;
    }
    vfs_node_t node = vfs_open(path);
    if (node == NULL)
    {
        write_serial_fmt("installer: open payload file failed path=%s\n", path);
        return -ENOENT;
    }
    if (size > INSTALLER_COPY_CHUNK) vfs_resize_fast(node, size);
    size_t written = 0;
    while (written < size)
    {
        size_t chunk = MIN(INSTALLER_COPY_CHUNK, size - written);
        size_t got = vfs_write_fast(node, (void *)(data + written), written, chunk);
        if (got != chunk)
        {
            write_serial_fmt("installer: write payload file failed path=%s offset=%zu chunk=%zu got=%zu\n",
                             path, written, chunk, got);
            vfs_close(node);
            return -EIO;
        }
        written += got;
        if (ctx != NULL)
        {
            ctx->copied_file_bytes += got;
            ctx->current_file_written = written;
            installer_copy_progress(ctx, detail, false);
            ctx->chunk_counter++;
            if ((ctx->chunk_counter & 7U) == 0) scheduler_yield();
        }
    }
    vfs_close(node);
    return EOK;
}

static uint64_t installer_align_up_u64(uint64_t value, uint64_t align)
{
    if (align == 0) return value;
    uint64_t rem = value % align;
    if (rem == 0) return value;
    uint64_t add = align - rem;
    if (value > (~0ULL) - add) return ~0ULL;
    return value + add;
}

static int installer_payload_required_space_ex(const uint8_t *pak, size_t pak_size, uint32_t mode,
                                               uint64_t components, uint64_t *out_file_bytes,
                                               uint64_t *out_required_bytes, uint64_t *out_file_count,
                                               uint64_t *out_small_file_count, uint64_t *out_large_file_count)
{
    if (out_file_bytes == NULL || out_required_bytes == NULL) return -EINVAL;
    *out_file_bytes = 0;
    *out_required_bytes = 0;
    if (out_file_count != NULL) *out_file_count = 0;
    if (out_small_file_count != NULL) *out_small_file_count = 0;
    if (out_large_file_count != NULL) *out_large_file_count = 0;
    if (pak == NULL || pak_size < sizeof(xj380_pak_header)) return -EINVAL;
    const xj380_pak_header *header = (const xj380_pak_header *)pak;
    if (memcmp(header->magic, XJ380_INSTALLER_PAK_MAGIC, XJ380_INSTALLER_PAK_MAGIC_SIZE) != 0) return -EINVAL;

    static const uint64_t FAT32_CLUSTER_ESTIMATE = 32768;
    static const uint64_t FAT32_METADATA_RESERVE = 64ULL * 1024ULL * 1024ULL;
    size_t off = sizeof(*header);
    uint64_t required = FAT32_METADATA_RESERVE;
    components = installer_normalize_components(components);
    for (uint64_t i = 0; i < header->entry_count; i++)
    {
        if (off > pak_size || pak_size - off < sizeof(xj380_pak_entry_header)) return -EINVAL;
        const xj380_pak_entry_header *entry = (const xj380_pak_entry_header *)(pak + off);
        off += sizeof(*entry);
        if (entry->path_len == 0 || entry->path_len >= 512 || off > pak_size || pak_size - off < entry->path_len)
            return -EINVAL;
        char rel[512];
        memcpy(rel, pak + off, entry->path_len);
        rel[entry->path_len] = '\0';
        off += entry->path_len;
        bool skip = installer_should_skip_payload_path(rel, mode, components);
        if (entry->type == XJ380_PAK_ENTRY_FILE)
        {
            if (entry->size > pak_size - off) return -EINVAL;
            if (!skip)
            {
                uint64_t aligned = installer_align_up_u64(entry->size, FAT32_CLUSTER_ESTIMATE);
                if (*out_file_bytes > (~0ULL) - entry->size || required > (~0ULL) - aligned) return -EOVERFLOW;
                *out_file_bytes += entry->size;
                required += aligned;
                if (out_file_count != NULL) (*out_file_count)++;
                if (entry->size >= 1024ULL * 1024ULL)
                {
                    if (out_large_file_count != NULL) (*out_large_file_count)++;
                }
                else
                {
                    if (out_small_file_count != NULL) (*out_small_file_count)++;
                }
            }
            off += (size_t)entry->size;
        }
        else if (entry->type == XJ380_PAK_ENTRY_DIR)
        {
            if (!skip)
            {
                if (required > (~0ULL) - FAT32_CLUSTER_ESTIMATE) return -EOVERFLOW;
                required += FAT32_CLUSTER_ESTIMATE;
            }
        }
        else
        {
            return -EINVAL;
        }
    }
    if (off != pak_size) return -EINVAL;
    *out_required_bytes = required;
    return EOK;
}

static int installer_payload_required_space(const uint8_t *pak, size_t pak_size,
                                            uint64_t *out_file_bytes, uint64_t *out_required_bytes)
{
    return installer_payload_required_space_ex(pak, pak_size, XJ380_INSTALLER_MODE_FRESH,
                                               XJ380_INSTALLER_COMPONENT_DEFAULT,
                                               out_file_bytes, out_required_bytes, NULL, NULL, NULL);
}

static void installer_precheck_add(xj380_installer_precheck *check, uint32_t status, uint32_t code,
                                   const char *title, const char *detail)
{
    if (check == NULL || check->item_count >= XJ380_INSTALLER_CHECK_ITEMS) return;
    xj380_installer_check_item *item = &check->items[check->item_count++];
    memset(item, 0, sizeof(*item));
    item->status = status;
    item->code = code;
    if (title != NULL) strncpy(item->title, title, sizeof(item->title) - 1);
    if (detail != NULL) strncpy(item->detail, detail, sizeof(item->detail) - 1);
    if (status == XJ380_INSTALLER_CHECK_ERROR) check->can_continue = 0;
}

static int installer_build_precheck(size_t disk_id, uint32_t mode, uint64_t components,
                                    xj380_installer_precheck *check)
{
    if (check == NULL) return -EINVAL;
    components = installer_normalize_components(components);
    memset(check, 0, sizeof(*check));
    check->disk_id = (xj380_inst_u32)disk_id;
    check->mode = mode;
    check->components = components;
    check->can_continue = 1;

    device_t *disk = get_device(disk_id);
    if (disk == NULL || disk->type != DEVICE_BLOCK || disk->flag < 1)
    {
        installer_precheck_add(check, XJ380_INSTALLER_CHECK_ERROR, 1, "目标硬盘不可用", "内核找不到指定的块设备。");
        return -ENODEV;
    }

    char detail[XJ380_INSTALLER_DETAIL_LEN];
    snprintf(detail, sizeof(detail), "设备 %s，容量 %llu MiB。", disk->drive_name, disk->size / 1024ULL / 1024ULL);
    installer_precheck_add(check, XJ380_INSTALLER_CHECK_OK, 0, "发现目标硬盘", detail);

    if (disk->write == NULL)
        installer_precheck_add(check, XJ380_INSTALLER_CHECK_ERROR, 2, "目标硬盘不可写", "该设备没有写入接口，可能是 ISO 光驱或只读设备。");
    else
        installer_precheck_add(check, XJ380_INSTALLER_CHECK_OK, 0, "硬盘可写", "内核检测到该块设备支持写入。");

    if (installer_device_is_slice_name(disk->drive_name) || installer_is_boot_media_name(disk->drive_name))
        installer_precheck_add(check, XJ380_INSTALLER_CHECK_ERROR, 3, "设备类型不允许", "不能安装到分区、ISO 或启动介质。");
    else
        installer_precheck_add(check, XJ380_INSTALLER_CHECK_OK, 0, "设备类型正常", "该设备看起来是整块硬盘。");

    if (disk->sector_size != 512)
    {
        snprintf(detail, sizeof(detail), "当前扇区大小是 %zu 字节，安装器目前只支持 512 字节。", disk->sector_size);
        installer_precheck_add(check, XJ380_INSTALLER_CHECK_ERROR, 4, "扇区大小不支持", detail);
    }
    else
    {
        installer_precheck_add(check, XJ380_INSTALLER_CHECK_OK, 0, "扇区大小正确", "目标硬盘扇区大小为 512 字节。");
    }

    if (disk->size == 0)
        installer_precheck_add(check, XJ380_INSTALLER_CHECK_ERROR, 5, "容量读取失败", "内核无法读取目标硬盘容量。");
    if (disk->sector_size != 0 && (disk->size / disk->sector_size) > 0xffffffffULL)
        installer_precheck_add(check, XJ380_INSTALLER_CHECK_ERROR, 6, "目标硬盘过大", "当前 FAT32 后端暂不支持该硬盘大小。");

    size_t payload_size = 0;
    const uint8_t *payload = installer_payload_virt(installer_current_boot_config(), &payload_size);
    if (payload == NULL || payload_size < sizeof(xj380_pak_header))
    {
        installer_precheck_add(check, XJ380_INSTALLER_CHECK_ERROR, 7, "系统包不可用", "安装介质中的 system-payload.pak 无法读取。");
        return -EINVAL;
    }

    uint64_t payload_file_bytes = 0;
    uint64_t payload_required_bytes = 0;
    int ret = installer_payload_required_space_ex(payload, payload_size, mode, components,
                                                  &payload_file_bytes, &payload_required_bytes,
                                                  NULL, NULL, NULL);
    if (ret < 0)
    {
        installer_precheck_add(check, XJ380_INSTALLER_CHECK_ERROR, 8, "系统包格式错误", "安装介质中的 system-payload.pak 无法解析。");
        return ret;
    }
    check->payload_bytes = payload_file_bytes;
    check->required_bytes = payload_required_bytes;
    snprintf(detail, sizeof(detail), "系统文件约 %llu MiB，需要约 %llu MiB 可用空间。",
             payload_file_bytes / 1024ULL / 1024ULL, payload_required_bytes / 1024ULL / 1024ULL);
    installer_precheck_add(check, XJ380_INSTALLER_CHECK_OK, 0, "系统包检查完成", detail);

    if (disk->sector_size != 0)
    {
        uint64_t total_lba = disk->size / disk->sector_size;
        uint64_t first_lba = 2048;
        uint64_t last_lba = total_lba > 34 ? total_lba - 34 : 0;
        check->efi_first_lba = first_lba;
        check->efi_last_lba = last_lba;
        if (total_lba < 4096 || last_lba <= first_lba)
        {
            installer_precheck_add(check, XJ380_INSTALLER_CHECK_ERROR, 9, "无法创建 EFI 分区", "目标硬盘太小，无法放下 GPT 和 EFI 系统分区。");
        }
        else
        {
            check->target_bytes = (last_lba - first_lba + 1) * disk->sector_size;
            snprintf(detail, sizeof(detail), "将创建 1 个 FAT32 EFI 系统分区，可用约 %llu MiB。",
                     check->target_bytes / 1024ULL / 1024ULL);
            installer_precheck_add(check, XJ380_INSTALLER_CHECK_OK, 0, "EFI 分区可创建", detail);
            if (mode != XJ380_INSTALLER_MODE_REPAIR_BOOT && check->target_bytes < payload_required_bytes)
            {
                snprintf(detail, sizeof(detail), "目标分区约 %llu MiB，系统需要约 %llu MiB。",
                         check->target_bytes / 1024ULL / 1024ULL, payload_required_bytes / 1024ULL / 1024ULL);
                installer_precheck_add(check, XJ380_INSTALLER_CHECK_ERROR, 10, "目标硬盘空间不足", detail);
            }
        }
    }

    if (mode == XJ380_INSTALLER_MODE_REPAIR_BOOT)
        installer_precheck_add(check, XJ380_INSTALLER_CHECK_WARN, 0, "仅修复引导", "该模式需要已有 EFI 分区，只会重写 EFI/BOOT/BOOTX64.efi 和 /system/kernel.krl。");
    else if (mode == XJ380_INSTALLER_MODE_KEEP_USERS)
        installer_precheck_add(check, XJ380_INSTALLER_CHECK_WARN, 0, "保留用户目录", "该模式需要已有 XJ380 分区，会覆盖系统文件并跳过 /users，不会格式化硬盘。");
    else if (mode == XJ380_INSTALLER_MODE_DEVELOPER)
        installer_precheck_add(check, XJ380_INSTALLER_CHECK_WARN, 0, "开发者安装", "安装过程会显示更详细的复制和日志信息。");
    else
        installer_precheck_add(check, XJ380_INSTALLER_CHECK_WARN, 0, "全新安装", "该模式会清空目标硬盘上的所有数据。");

    if ((components & XJ380_INSTALLER_COMPONENT_LINUX_COMPAT) == 0)
        installer_precheck_add(check, XJ380_INSTALLER_CHECK_WARN, 0, "未选择 Linux 兼容层", "将跳过 /usr、Linux 动态库和 Linux 工具程序。");
    else if (components != XJ380_INSTALLER_COMPONENT_DEFAULT)
        installer_precheck_add(check, XJ380_INSTALLER_CHECK_WARN, 0, "自定义组件", "将只安装已勾选的可选包。");

    return check->can_continue ? EOK : -EINVAL;
}

static bool installer_path_is_or_under(const char *rel, const char *prefix)
{
    size_t len = strlen(prefix);
    return strcmp(rel, prefix) == 0 || (strncmp(rel, prefix, len) == 0 && rel[len] == '/');
}

static bool installer_path_base_has_prefix(const char *rel, const char *prefix)
{
    if (rel == NULL || prefix == NULL) return false;
    const char *base = strrchr(rel, '/');
    base = base == NULL ? rel : base + 1;
    return strncmp(base, prefix, strlen(prefix)) == 0;
}

static bool installer_path_has_segment_prefix(const char *rel, const char *prefix)
{
    if (rel == NULL || prefix == NULL) return false;
    size_t prefix_len = strlen(prefix);
    const char *p = rel;
    while (*p != '\0')
    {
        while (*p == '/') p++;
        if (*p == '\0') break;
        if (strncmp(p, prefix, prefix_len) == 0) return true;
        while (*p != '\0' && *p != '/') p++;
    }
    return false;
}

static bool installer_should_skip_linux_compat_path(const char *rel)
{
    if (installer_path_is_or_under(rel, "/usr")) return true;
    if (installer_path_is_or_under(rel, "/bin")) return true;
    if (installer_path_is_or_under(rel, "/sbin")) return true;
    if (installer_path_is_or_under(rel, "/lib64")) return true;
    if (installer_path_is_or_under(rel, "/etc")) return true;
    if (installer_path_is_or_under(rel, "/var")) return true;
    if (installer_path_is_or_under(rel, "/opt")) return true;
    if (installer_path_is_or_under(rel, "/root")) return true;
    if (installer_path_is_or_under(rel, "/apps/busybox")) return true;
    if (installer_path_is_or_under(rel, "/apps/fastfetch")) return true;
    if (installer_path_is_or_under(rel, "/apps/dyn-hello")) return true;
    if (installer_path_is_or_under(rel, "/apps/lolcat.deb")) return true;
    if (installer_path_is_or_under(rel, "/apps/1.c")) return true;
    if (installer_path_is_or_under(rel, "/lib/ld-linux-x86-64.so.2")) return true;
    if (installer_path_is_or_under(rel, "/lib/ld-musl-x86_64.so.1")) return true;
    if (installer_path_is_or_under(rel, "/lib/libc.so")) return true;
    if (installer_path_is_or_under(rel, "/lib/libc.musl-x86_64.so.1")) return true;
    if (installer_path_is_or_under(rel, "/lib/libgcc_s.so.1")) return true;
    return false;
}

static bool installer_should_skip_python_path(const char *rel)
{
    return installer_path_base_has_prefix(rel, "python") ||
           installer_path_has_segment_prefix(rel, "python") ||
           installer_path_is_or_under(rel, "/usr/lib/python") ||
           installer_path_is_or_under(rel, "/usr/libexec/python") ||
           installer_path_is_or_under(rel, "/usr/include/python") ||
           installer_path_is_or_under(rel, "/usr/share/python") ||
           installer_path_is_or_under(rel, "/usr/share/licenses/python");
}

static bool installer_should_skip_llvm_clang_path(const char *rel)
{
    return installer_path_base_has_prefix(rel, "clang") ||
           installer_path_base_has_prefix(rel, "llvm") ||
           installer_path_base_has_prefix(rel, "llc") ||
           installer_path_base_has_prefix(rel, "lld") ||
           installer_path_has_segment_prefix(rel, "clang") ||
           installer_path_has_segment_prefix(rel, "llvm") ||
           installer_path_is_or_under(rel, "/usr/lib/clang") ||
           installer_path_is_or_under(rel, "/usr/lib/llvm") ||
           installer_path_is_or_under(rel, "/usr/include/clang") ||
           installer_path_is_or_under(rel, "/usr/include/llvm") ||
           installer_path_is_or_under(rel, "/usr/share/clang") ||
           installer_path_is_or_under(rel, "/usr/share/llvm") ||
           installer_path_is_or_under(rel, "/usr/share/licenses/clang") ||
           installer_path_is_or_under(rel, "/usr/share/licenses/llvm");
}

static bool installer_should_skip_gcc_path(const char *rel)
{
    return installer_path_is_or_under(rel, "/usr/lib/gcc") ||
           installer_path_is_or_under(rel, "/usr/lib/cpp") ||
           installer_path_is_or_under(rel, "/usr/share/gcc") ||
           installer_path_is_or_under(rel, "/usr/include/c++") ||
           installer_path_is_or_under(rel, "/usr/share/licenses/gcc") ||
           installer_path_has_segment_prefix(rel, "gcc") ||
           installer_path_has_segment_prefix(rel, "g++") ||
           installer_path_base_has_prefix(rel, "libgcc") ||
           installer_path_base_has_prefix(rel, "libgomp") ||
           installer_path_base_has_prefix(rel, "libstdc++") ||
           strcmp(rel, "/usr/bin/gcc") == 0 ||
           strcmp(rel, "/usr/bin/g++") == 0 ||
           strcmp(rel, "/usr/bin/cpp") == 0 ||
           strcmp(rel, "/usr/bin/cc") == 0 ||
           strcmp(rel, "/usr/bin/c++") == 0 ||
           installer_path_base_has_prefix(rel, "gcc.") ||
           installer_path_base_has_prefix(rel, "g++.") ||
           installer_path_base_has_prefix(rel, "gccinstall.") ||
           installer_path_base_has_prefix(rel, "gccint.") ||
           installer_path_base_has_prefix(rel, "gcc-") ||
           installer_path_base_has_prefix(rel, "g++-") ||
           installer_path_base_has_prefix(rel, "cpp-");
}

static bool installer_should_skip_payload_path(const char *rel, uint32_t mode, uint64_t components)
{
    if (rel == NULL) return false;
    if (mode == XJ380_INSTALLER_MODE_KEEP_USERS)
    {
        if (strcmp(rel, "/users") == 0 || strncmp(rel, "/users/", 7) == 0)
            return true;
    }
    components = installer_normalize_components(components);
    if ((components & XJ380_INSTALLER_COMPONENT_LINUX_COMPAT) == 0)
        return installer_should_skip_linux_compat_path(rel);
    if ((components & XJ380_INSTALLER_COMPONENT_PYTHON) == 0 && installer_should_skip_python_path(rel))
        return true;
    if ((components & XJ380_INSTALLER_COMPONENT_LLVM_CLANG) == 0 && installer_should_skip_llvm_clang_path(rel))
        return true;
    if ((components & XJ380_INSTALLER_COMPONENT_GCC) == 0 && installer_should_skip_gcc_path(rel))
        return true;
    return false;
}

static int installer_unpack_pak_to_target(const uint8_t *pak, size_t pak_size, const char *target_prefix,
                                          uint32_t mode, uint64_t components)
{
    if (pak == NULL || pak_size < sizeof(xj380_pak_header) || target_prefix == NULL) return -EINVAL;
    const xj380_pak_header *header = (const xj380_pak_header *)pak;
    if (memcmp(header->magic, XJ380_INSTALLER_PAK_MAGIC, XJ380_INSTALLER_PAK_MAGIC_SIZE) != 0) return -EINVAL;
    components = installer_normalize_components(components);

    uint64_t total_file_bytes = 0;
    uint64_t required_bytes = 0;
    uint64_t total_file_count = 0;
    uint64_t small_file_count = 0;
    uint64_t large_file_count = 0;
    int ret = installer_payload_required_space_ex(pak, pak_size, mode, components,
                                                  &total_file_bytes, &required_bytes,
                                                  &total_file_count, &small_file_count,
                                                  &large_file_count);
    if (ret < 0) return ret;

    installer_copy_context ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.total_file_bytes = total_file_bytes;
    ctx.total_file_count = total_file_count;
    ctx.small_file_count = small_file_count;
    ctx.large_file_count = large_file_count;
    ctx.last_percent = 54;
    ctx.copy_start_ns = nanoTime();
    installer_dir_cache_init(&ctx.dirs, header->entry_count);
    installer_dir_cache_add(&ctx.dirs, "/target");
    memset(g_installer_progress.queue_items, 0, sizeof(g_installer_progress.queue_items));
    g_installer_progress.queue_count = 0;
    g_installer_progress.queue_index = 0;
    g_installer_progress.queue_total = 0;

    if (!installer_file_list_init(&ctx.files, ctx.total_file_count))
    {
        installer_dir_cache_free(&ctx.dirs);
        return -ENOMEM;
    }
    ret = installer_collect_file_list(pak, pak_size, &ctx.files, mode, components);
    if (ret < 0)
    {
        installer_file_list_free(&ctx.files);
        installer_dir_cache_free(&ctx.dirs);
        return ret;
    }
    installer_progress_sync_queue(&ctx);
    installer_set_progress(XJ380_INSTALLER_RUNNING, 55, 0, "正在复制系统文件", "准备复制系统文件。");
    installer_progress_sync_queue(&ctx);

    size_t off = sizeof(*header);
    uint64_t file_index = 0;
    for (uint64_t i = 0; i < header->entry_count; i++)
    {
        if (off > pak_size || pak_size - off < sizeof(xj380_pak_entry_header))
        {
            installer_dir_cache_free(&ctx.dirs);
            installer_file_list_free(&ctx.files);
            return -EINVAL;
        }
        const xj380_pak_entry_header *entry = (const xj380_pak_entry_header *)(pak + off);
        off += sizeof(*entry);
        if (entry->path_len == 0 || entry->path_len >= 512 || off > pak_size || pak_size - off < entry->path_len)
        {
            installer_dir_cache_free(&ctx.dirs);
            installer_file_list_free(&ctx.files);
            return -EINVAL;
        }

        char rel[512];
        memcpy(rel, pak + off, entry->path_len);
        rel[entry->path_len] = '\0';
        off += entry->path_len;
        if (rel[0] != '/')
        {
            installer_dir_cache_free(&ctx.dirs);
            installer_file_list_free(&ctx.files);
            return -EINVAL;
        }
        if (installer_should_skip_payload_path(rel, mode, components))
        {
            if (entry->type == XJ380_PAK_ENTRY_FILE)
            {
                if (off > pak_size || entry->size > pak_size - off)
                {
                    installer_dir_cache_free(&ctx.dirs);
                    installer_file_list_free(&ctx.files);
                    return -EINVAL;
                }
                off += (size_t)entry->size;
            }
            continue;
        }

        char out_path[640];
        snprintf(out_path, sizeof(out_path), "%s%s", target_prefix, rel);
        if (entry->type == XJ380_PAK_ENTRY_DIR)
        {
            ret = installer_mkdir_cached(&ctx, out_path);
            if (ret < 0)
            {
                installer_dir_cache_free(&ctx.dirs);
                installer_file_list_free(&ctx.files);
                return ret;
            }
        }
        else if (entry->type == XJ380_PAK_ENTRY_FILE)
        {
            if (off > pak_size || entry->size > pak_size - off)
            {
                installer_dir_cache_free(&ctx.dirs);
                installer_file_list_free(&ctx.files);
                return -EINVAL;
            }
            ctx.current_file_index = file_index;
            ctx.current_file_size = entry->size;
            ctx.current_file_written = 0;
            installer_progress_sync_queue(&ctx);
            installer_copy_progress(&ctx, rel, true);
            ret = installer_write_payload_file(&ctx, out_path, pak + off, (size_t)entry->size, rel);
            if (ret < 0)
            {
                uint32_t percent = installer_copy_percent(&ctx);
                installer_set_progress(XJ380_INSTALLER_RUNNING, percent, ret, "正在复制系统文件", rel);
                write_serial_fmt("installer: payload file failed ret=%d rel=%s out=%s size=%llu\n",
                                 ret, rel, out_path, entry->size);
                installer_dir_cache_free(&ctx.dirs);
                installer_file_list_free(&ctx.files);
                return ret;
            }
            off += (size_t)entry->size;
            if (entry->size >= 1024ULL * 1024ULL)
                ctx.copied_large_file_count++;
            else
                ctx.copied_small_file_count++;
            ctx.current_file_written = entry->size;
            file_index++;
        }
        else
        {
            installer_dir_cache_free(&ctx.dirs);
            installer_file_list_free(&ctx.files);
            return -EINVAL;
        }

        if ((i & 63) == 0)
        {
            installer_copy_progress(&ctx, rel, false);
            scheduler_yield();
        }
    }
    installer_copy_progress(&ctx, "系统文件复制完成。", true);
    installer_dir_cache_free(&ctx.dirs);
    installer_file_list_free(&ctx.files);
    return EOK;
}

static int installer_copy_payload_file_by_rel(const uint8_t *pak, size_t pak_size, const char *rel, const char *target_prefix)
{
    if (pak == NULL || rel == NULL || target_prefix == NULL || pak_size < sizeof(xj380_pak_header)) return -EINVAL;
    const xj380_pak_header *header = (const xj380_pak_header *)pak;
    if (memcmp(header->magic, XJ380_INSTALLER_PAK_MAGIC, XJ380_INSTALLER_PAK_MAGIC_SIZE) != 0) return -EINVAL;

    installer_copy_context ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.total_file_count = 1;
    ctx.last_percent = 54;
    ctx.copy_start_ns = nanoTime();
    installer_dir_cache_init(&ctx.dirs, header->entry_count);
    installer_dir_cache_add(&ctx.dirs, "/target");

    size_t off = sizeof(*header);
    for (uint64_t i = 0; i < header->entry_count; i++)
    {
        if (off > pak_size || pak_size - off < sizeof(xj380_pak_entry_header))
        {
            installer_dir_cache_free(&ctx.dirs);
            return -EINVAL;
        }
        const xj380_pak_entry_header *entry = (const xj380_pak_entry_header *)(pak + off);
        off += sizeof(*entry);
        if (entry->path_len == 0 || entry->path_len >= 512 || off > pak_size || pak_size - off < entry->path_len)
        {
            installer_dir_cache_free(&ctx.dirs);
            return -EINVAL;
        }

        char entry_rel[512];
        memcpy(entry_rel, pak + off, entry->path_len);
        entry_rel[entry->path_len] = '\0';
        off += entry->path_len;

        if (entry->type == XJ380_PAK_ENTRY_FILE)
        {
            if (entry->size > pak_size - off)
            {
                installer_dir_cache_free(&ctx.dirs);
                return -EINVAL;
            }
            if (strcmp(entry_rel, rel) == 0)
            {
                char out_path[640];
                snprintf(out_path, sizeof(out_path), "%s%s", target_prefix, entry_rel);
                ctx.total_file_bytes = entry->size;
                ctx.current_file_size = entry->size;
                int ret = installer_write_payload_file(&ctx, out_path, pak + off, (size_t)entry->size, entry_rel);
                installer_dir_cache_free(&ctx.dirs);
                return ret;
            }
            off += (size_t)entry->size;
        }
        else if (entry->type != XJ380_PAK_ENTRY_DIR)
        {
            installer_dir_cache_free(&ctx.dirs);
            return -EINVAL;
        }
    }

    installer_dir_cache_free(&ctx.dirs);
    return -ENOENT;
}

static const uint8_t *installer_payload_virt(const BOOT_CONFIG *config, size_t *out_size)
{
    if (out_size == NULL || config == NULL || config->system_payload_pak == 0 || config->system_payload_pak_size == 0)
        return NULL;
    *out_size = (size_t)config->system_payload_pak_size;
    return (const uint8_t *)(config->system_payload_pak + 0xffff800000000000ULL);
}

static int installer_mount_first_partition(size_t disk_id, const char *stage, uint32_t percent, char *out_part_path, size_t out_part_path_size)
{
    device_t *disk = get_device(disk_id);
    if (disk == NULL) return installer_fail(-ENODEV, percent, stage, "内核找不到指定的块设备。");

    partition_rescan_device(disk_id, false);
    int part_index = partition_find_first_for_disk(disk_id);
    if (part_index < 0) return installer_fail(-ENODEV, percent, stage, "内核没有识别到目标硬盘上的 EFI 分区。");

    char part_path[64];
    snprintf(part_path, sizeof(part_path), "/dev/%sp%d", disk->drive_name, part_index);
    if (out_part_path != NULL && out_part_path_size > 0)
    {
        strncpy(out_part_path, part_path, out_part_path_size - 1);
        out_part_path[out_part_path_size - 1] = '\0';
    }

    vfs_mkdir("/target");
    vfs_node_t target = vfs_open("/target");
    if (target == NULL) return installer_fail(-ENOENT, percent, "挂载失败", "无法创建 /target 挂载点。");
    int ret = vfs_mount(part_path, target);
    if (ret != EOK) return installer_fail(-EIO, percent, "挂载失败", "无法挂载 FAT32 目标分区。");
    return EOK;
}

static int installer_repair_boot_on_mounted_target(const uint8_t *payload, size_t payload_size)
{
    installer_set_progress(XJ380_INSTALLER_RUNNING, 55, 0, "正在修复引导", "正在写入 UEFI bootloader。");
    int ret = installer_copy_payload_file_by_rel(payload, payload_size, "/EFI/BOOT/BOOTX64.efi", "/target");
    if (ret < 0) return ret;
    installer_set_progress(XJ380_INSTALLER_RUNNING, 75, 0, "正在修复引导", "正在写入系统内核。");
    ret = installer_copy_payload_file_by_rel(payload, payload_size, "/system/kernel.krl", "/target");
    if (ret < 0) return ret;
    installer_set_progress(XJ380_INSTALLER_RUNNING, 92, 0, "正在修复引导", "引导文件已重建。");
    return EOK;
}

static int installer_install_to_disk(size_t disk_id, uint32_t mode, uint64_t components)
{
    g_installer_mode = mode;
    components = installer_normalize_components(components);
    g_installer_components = components;
    device_t *disk = get_device(disk_id);
    if (disk == NULL || disk->type != DEVICE_BLOCK || disk->flag < 1)
        return installer_fail(-ENODEV, 0, "目标硬盘不可用", "内核找不到指定的块设备。");
    if (disk->write == NULL)
    {
        char detail[XJ380_INSTALLER_DETAIL_LEN];
        snprintf(detail, sizeof(detail), "设备 %s 不是可写硬盘，可能是 ISO 光驱。", disk->drive_name);
        return installer_fail(-EROFS, 0, "目标硬盘不可写", detail);
    }
    if (installer_device_is_slice_name(disk->drive_name) || installer_is_boot_media_name(disk->drive_name))
        return installer_fail(-EINVAL, 0, "目标硬盘不可用", "不能安装到分区或 ISO 启动介质。");
    if (disk->sector_size != 512)
    {
        char detail[XJ380_INSTALLER_DETAIL_LEN];
        snprintf(detail, sizeof(detail), "设备 %s 的扇区大小是 %zu 字节，当前只支持 512 字节硬盘。",
                 disk->drive_name, disk->sector_size);
        return installer_fail(-EINVAL, 0, "目标硬盘不支持", detail);
    }
    if (disk->size == 0) return installer_fail(-ENODEV, 0, "目标硬盘不可用", "内核无法读取目标硬盘容量。");
    if ((disk->size / disk->sector_size) > 0xffffffffULL) return installer_fail(-ENOSPC, 0, "目标硬盘过大", "当前 FAT32 后端暂不支持该硬盘大小。");

    size_t payload_size = 0;
    const uint8_t *payload = installer_payload_virt(installer_current_boot_config(), &payload_size);
    if (payload == NULL || payload_size < sizeof(xj380_pak_header))
        return installer_fail(-EINVAL, 0, "系统包不可用", "安装介质中的 system-payload.pak 无法读取。");

    uint64_t total_lba = disk->size / disk->sector_size;
    if (total_lba < 4096) return installer_fail(-ENOSPC, 0, "目标硬盘过小", "目标硬盘空间不足。");
    uint64_t first_lba = 2048;
    uint64_t last_lba = total_lba - 34;
    if (last_lba <= first_lba) return installer_fail(-ENOSPC, 0, "目标硬盘过小", "无法创建 EFI 系统分区。");

    uint64_t payload_file_bytes = 0;
    uint64_t payload_required_bytes = 0;
    int payload_ret = installer_payload_required_space_ex(payload, payload_size, mode, components,
                                                          &payload_file_bytes, &payload_required_bytes,
                                                          NULL, NULL, NULL);
    if (payload_ret < 0)
        return installer_fail(payload_ret, 0, "系统包不可用", "安装介质中的 system-payload.pak 格式不正确。");
    uint64_t target_bytes = (last_lba - first_lba + 1) * disk->sector_size;
    if (mode != XJ380_INSTALLER_MODE_REPAIR_BOOT && target_bytes < payload_required_bytes)
    {
        char detail[XJ380_INSTALLER_DETAIL_LEN];
        snprintf(detail, sizeof(detail), "目标分区约 %llu MiB，系统需要约 %llu MiB。请使用更大的硬盘。",
                 target_bytes / 1024ULL / 1024ULL, payload_required_bytes / 1024ULL / 1024ULL);
        write_serial_fmt("installer: target too small target=%llu file_bytes=%llu required=%llu payload=%zu\n",
                         target_bytes, payload_file_bytes, payload_required_bytes, payload_size);
        return installer_fail(-ENOSPC, 0, "目标硬盘过小", detail);
    }

    write_serial_fmt("installer: start disk=%s id=%zu mode=%u components=0x%llx size=%llu sector=%zu payload=%zu file_bytes=%llu required=%llu target=%llu\n",
                     disk->drive_name, disk_id, mode, components, disk->size, disk->sector_size, payload_size,
                     payload_file_bytes, payload_required_bytes, target_bytes);
    installer_log_fmt("开始安装：disk=%llu mode=%llu size=%llu", disk_id, mode, disk->size);
    installer_log_fmt("可选组件：mask=0x%llx", components, 0, 0);

    if (mode == XJ380_INSTALLER_MODE_REPAIR_BOOT)
    {
        installer_set_progress(XJ380_INSTALLER_RUNNING, 20, 0, "正在查找现有系统", "准备挂载目标 EFI 分区。");
        char part_path[64];
        int ret = installer_mount_first_partition(disk_id, "查找分区失败", 30, part_path, sizeof(part_path));
        if (ret < 0) return ret;
        ret = installer_repair_boot_on_mounted_target(payload, payload_size);
        vfs_unmount("/target");
        if (disk->ioctl != NULL) disk->ioctl(disk, IOBLKSYNC, NULL);
        if (ret < 0) return installer_fail(ret, g_installer_progress.percent, "修复引导失败", g_installer_progress.detail);
        installer_set_progress(XJ380_INSTALLER_DONE, 100, 0, "修复完成", "EFI 引导文件已重新写入。");
        return EOK;
    }

    if (mode == XJ380_INSTALLER_MODE_KEEP_USERS)
    {
        installer_set_progress(XJ380_INSTALLER_RUNNING, 20, 0, "正在查找现有系统", "准备挂载目标系统分区。");
        char part_path[64];
        int ret = installer_mount_first_partition(disk_id, "查找分区失败", 30, part_path, sizeof(part_path));
        if (ret < 0) return ret;
        installer_set_progress(XJ380_INSTALLER_RUNNING, 52, 0, "正在挂载目标系统", "将覆盖系统文件并保留 /users。");
        ret = installer_unpack_pak_to_target(payload, payload_size, "/target", mode, components);
        if (ret < 0)
        {
            vfs_unmount("/target");
            return installer_fail(ret, g_installer_progress.percent, "复制系统失败", g_installer_progress.detail);
        }
        installer_set_progress(XJ380_INSTALLER_RUNNING, 97, 0, "正在完成安装", "正在同步并卸载目标分区。");
        vfs_unmount("/target");
        if (disk->ioctl != NULL) disk->ioctl(disk, IOBLKSYNC, NULL);
        installer_set_progress(XJ380_INSTALLER_DONE, 100, 0, "安装完成", "XJ380 已重装完成，/users 已保留。");
        return EOK;
    }

    installer_set_progress(XJ380_INSTALLER_RUNNING, 5, 0, "正在清理磁盘", "写入新的 GPT 分区表前会清除磁盘头尾区域。");
    if (!installer_zero_sectors(disk_id, 0, 2048)) return installer_fail(-EIO, 5, "清理磁盘失败", "写入磁盘头部区域失败。");
    if (!installer_zero_sectors(disk_id, total_lba - 2048, 2048)) return installer_fail(-EIO, 12, "清理磁盘失败", "写入磁盘尾部区域失败。");

    installer_set_progress(XJ380_INSTALLER_RUNNING, 18, 0, "正在创建分区表", "创建一个 UEFI 可启动的 FAT32 EFI 系统分区。");
    if (!installer_write_gpt(disk_id, first_lba, last_lba)) return installer_fail(-EIO, 18, "创建分区表失败", "写入 GPT 分区表失败。");

    installer_set_progress(XJ380_INSTALLER_RUNNING, 30, 0, "正在扫描新分区", "等待内核识别目标硬盘上的新分区。");
    partition_rescan_device(disk_id, false);
    int part_index = partition_find_first_for_disk(disk_id);
    if (part_index < 0) return installer_fail(-ENODEV, 30, "扫描分区失败", "内核没有识别到新创建的 EFI 分区。");

    char part_path[64];
    snprintf(part_path, sizeof(part_path), "/dev/%sp%d", disk->drive_name, part_index);
    vfs_node_t part = vfs_open(part_path);
    if (part == NULL) return installer_fail(-ENOENT, 35, "打开分区失败", part_path);

    installer_set_progress(XJ380_INSTALLER_RUNNING, 42, 0, "正在格式化硬盘", "正在将目标分区格式化为 FAT32。");
    int ret = fatfs_format_node(part);
    vfs_close(part);
    if (ret < 0) return installer_fail(ret, 42, "格式化失败", "FAT32 格式化目标分区失败。");

    installer_set_progress(XJ380_INSTALLER_RUNNING, 52, 0, "正在挂载目标系统", "准备把 XJ380 系统文件写入硬盘。");
    vfs_mkdir("/target");
    vfs_node_t target = vfs_open("/target");
    if (target == NULL) return installer_fail(-ENOENT, 52, "挂载失败", "无法创建 /target 挂载点。");
    ret = vfs_mount(part_path, target);
    if (ret != EOK) return installer_fail(-EIO, 52, "挂载失败", "无法挂载 FAT32 目标分区。");

    ret = installer_unpack_pak_to_target(payload, payload_size, "/target", mode, components);
    if (ret < 0) return installer_fail(ret, g_installer_progress.percent, "复制系统失败", g_installer_progress.detail);

    installer_set_progress(XJ380_INSTALLER_RUNNING, 97, 0, "正在完成安装", "正在同步并卸载目标分区。");
    vfs_unmount("/target");
    if (disk->ioctl != NULL) disk->ioctl(disk, IOBLKSYNC, NULL);
    installer_set_progress(XJ380_INSTALLER_DONE, 100, 0, "安装完成", "XJ380 已安装到硬盘。请关机后移除 ISO 安装介质。");
    return EOK;
}

static void installer_worker(void *arg)
{
    installer_worker_args *args = (installer_worker_args *)arg;
    size_t disk_id = args != NULL ? args->disk_id : 0;
    uint32_t mode = args != NULL ? args->mode : XJ380_INSTALLER_MODE_FRESH;
    uint64_t components = args != NULL ? args->components : XJ380_INSTALLER_COMPONENT_DEFAULT;
    if (args != NULL) free(args);
    int ret = installer_install_to_disk(disk_id, mode, components);
    if (ret < 0)
    {
        if (g_installer_progress.state != XJ380_INSTALLER_FAILED)
            installer_fail(ret, g_installer_progress.percent, "安装失败", "安装过程中发生错误。");
    }
    g_installer_running = 0;
}

uint64_t do_xapi_InstallerEnumDisks(uint64_t list)
{
    if (list == 0) return (uint64_t)-EINVAL;
    page_directory_t *pagedir = xapi_current_pagedir();
    if (pagedir == NULL) return (uint64_t)-EFAULT;

    xj380_installer_disk_list local;
    memset(&local, 0, sizeof(local));
    for (uint32_t i = 0; i < 256 && local.count < XJ380_INSTALLER_MAX_DISKS; i++)
    {
        device_t *dev = get_device(i);
        if (!installer_device_is_install_target(dev)) continue;

        xj380_installer_disk *out = &local.disks[local.count++];
        out->id = i;
        out->sector_size = (xj380_inst_u32)dev->sector_size;
        out->size_bytes = dev->size;
        out->flags = installer_disk_flags(dev);
        strncpy(out->name, dev->drive_name, sizeof(out->name) - 1);
        write_serial_fmt("installer: target disk id=%u name=%s size=%llu sector=%zu\n",
                         i, dev->drive_name, dev->size, dev->sector_size);
    }

    write_serial_fmt("installer: enum target disks count=%u\n", local.count);
    if (!copy_to_user_pagedir(pagedir, (void *)list, &local, sizeof(local))) return (uint64_t)-EFAULT;
    return local.count;
}

uint64_t do_xapi_InstallerStart(uint64_t disk_id)
{
    return do_xapi_InstallerStartEx(disk_id, XJ380_INSTALLER_MODE_FRESH);
}

static uint64_t installer_start_internal(uint64_t disk_id, uint64_t mode, uint64_t components)
{
    const BOOT_CONFIG *config = installer_current_boot_config();
    if (config == NULL || !installer_boot_active(*config)) return (uint64_t)-ENOSYS;
    if (disk_id >= 256) return (uint64_t)-EINVAL;
    if (mode > XJ380_INSTALLER_MODE_DEVELOPER) return (uint64_t)-EINVAL;
    components = installer_normalize_components(components);

    xj380_installer_precheck check;
    int precheck_ret = installer_build_precheck((size_t)disk_id, (uint32_t)mode, components, &check);
    if (precheck_ret < 0 || !check.can_continue)
    {
        installer_set_progress(XJ380_INSTALLER_FAILED, 0, precheck_ret < 0 ? precheck_ret : -EINVAL,
                               "安装前检查失败", "目标硬盘未通过安装前检查。");
        return (uint64_t)(precheck_ret < 0 ? precheck_ret : -EINVAL);
    }

    if (__sync_lock_test_and_set(&g_installer_running, 1) != 0) return (uint64_t)-EBUSY;

    g_installer_target_disk = (size_t)disk_id;
    g_installer_mode = (uint32_t)mode;
    g_installer_components = components;
    memset(&g_installer_log, 0, sizeof(g_installer_log));
    installer_log_text("安装任务启动。");
    installer_log_text(installer_mode_name((uint32_t)mode));
    installer_set_progress(XJ380_INSTALLER_RUNNING, 0, 0, "准备安装", "安装程序正在初始化。");
    installer_worker_args *args = (installer_worker_args *)malloc(sizeof(installer_worker_args));
    if (args == NULL)
    {
        g_installer_running = 0;
        installer_set_progress(XJ380_INSTALLER_FAILED, 0, -ENOMEM, "无法启动安装线程", "内核无法分配安装参数。");
        return (uint64_t)-ENOMEM;
    }
    args->disk_id = (size_t)disk_id;
    args->mode = (uint32_t)mode;
    args->components = components;
    size_t tid = create_kernel_thread((void *)installer_worker, args, (char *)"XJ380 Installer", NULL);
    if ((int64_t)tid < 0)
    {
        free(args);
        g_installer_running = 0;
        installer_set_progress(XJ380_INSTALLER_FAILED, 0, (int64_t)tid, "无法启动安装线程",
                               "内核无法创建安装工作线程。");
        return tid;
    }
    return 0;
}

uint64_t do_xapi_InstallerStartEx(uint64_t disk_id, uint64_t mode)
{
    return installer_start_internal(disk_id, mode, XJ380_INSTALLER_COMPONENT_DEFAULT);
}

uint64_t do_xapi_InstallerStartOptions(uint64_t options)
{
    if (options == 0) return (uint64_t)-EINVAL;
    page_directory_t *pagedir = xapi_current_pagedir();
    if (pagedir == NULL) return (uint64_t)-EFAULT;

    xj380_installer_start_options local;
    if (!copy_from_user_pagedir(pagedir, &local, (const void *)options, sizeof(local))) return (uint64_t)-EFAULT;
    return installer_start_internal(local.disk_id, local.mode, local.components);
}

uint64_t do_xapi_InstallerPrecheck(uint64_t disk_id, uint64_t mode, uint64_t out)
{
    if (out == 0) return (uint64_t)-EINVAL;
    if (disk_id >= 256 || mode > XJ380_INSTALLER_MODE_DEVELOPER) return (uint64_t)-EINVAL;
    page_directory_t *pagedir = xapi_current_pagedir();
    if (pagedir == NULL) return (uint64_t)-EFAULT;

    xj380_installer_precheck local;
    int ret = installer_build_precheck((size_t)disk_id, (uint32_t)mode,
                                       XJ380_INSTALLER_COMPONENT_DEFAULT, &local);
    if (!copy_to_user_pagedir(pagedir, (void *)out, &local, sizeof(local))) return (uint64_t)-EFAULT;
    return (uint64_t)ret;
}

uint64_t do_xapi_InstallerPrecheckOptions(uint64_t options, uint64_t out)
{
    if (options == 0 || out == 0) return (uint64_t)-EINVAL;
    page_directory_t *pagedir = xapi_current_pagedir();
    if (pagedir == NULL) return (uint64_t)-EFAULT;

    xj380_installer_start_options opts;
    if (!copy_from_user_pagedir(pagedir, &opts, (const void *)options, sizeof(opts))) return (uint64_t)-EFAULT;
    if (opts.disk_id >= 256 || opts.mode > XJ380_INSTALLER_MODE_DEVELOPER) return (uint64_t)-EINVAL;

    xj380_installer_precheck local;
    int ret = installer_build_precheck((size_t)opts.disk_id, opts.mode, opts.components, &local);
    if (!copy_to_user_pagedir(pagedir, (void *)out, &local, sizeof(local))) return (uint64_t)-EFAULT;
    return (uint64_t)ret;
}

uint64_t do_xapi_InstallerProgress(uint64_t progress)
{
    if (progress == 0) return (uint64_t)-EINVAL;
    page_directory_t *pagedir = xapi_current_pagedir();
    if (pagedir == NULL) return (uint64_t)-EFAULT;
    (void)g_installer_target_disk;
    if (!copy_to_user_pagedir(pagedir, (void *)progress, &g_installer_progress, sizeof(g_installer_progress)))
        return (uint64_t)-EFAULT;
    return 0;
}

static void installer_rescue_add(xj380_installer_rescue_result *result, uint32_t status, uint32_t code,
                                 const char *title, const char *detail)
{
    if (result == NULL || result->item_count >= XJ380_INSTALLER_RESCUE_ITEMS) return;
    xj380_installer_rescue_item *item = &result->items[result->item_count++];
    memset(item, 0, sizeof(*item));
    item->status = status;
    item->code = code;
    if (title != NULL) strncpy(item->title, title, sizeof(item->title) - 1);
    if (detail != NULL) strncpy(item->detail, detail, sizeof(item->detail) - 1);
}

uint64_t do_xapi_InstallerRescue(uint64_t action, uint64_t disk_id, uint64_t out)
{
    if (out == 0) return (uint64_t)-EINVAL;
    page_directory_t *pagedir = xapi_current_pagedir();
    if (pagedir == NULL) return (uint64_t)-EFAULT;

    xj380_installer_rescue_result result;
    memset(&result, 0, sizeof(result));
    result.result = 0;

    if (action == XJ380_INSTALLER_RESCUE_OPEN_TERM)
    {
        int pid = create_user_process_from_file((char *)"/apps/system/shell.elf", NULL, NULL);
        switch_page_directory(pagedir);
        result.result = pid < 0 ? pid : 0;
        if (pid < 0)
            installer_rescue_add(&result, XJ380_INSTALLER_CHECK_ERROR, (uint32_t)-pid, "终端启动失败", "无法启动 /apps/system/shell.elf。");
        else
            installer_rescue_add(&result, XJ380_INSTALLER_CHECK_OK, 0, "终端已打开", "已启动安装环境终端。");
        if (!copy_to_user_pagedir(pagedir, (void *)out, &result, sizeof(result))) return (uint64_t)-EFAULT;
        return (uint64_t)result.result;
    }

    if (action == XJ380_INSTALLER_RESCUE_VIEW_LOG)
    {
        installer_rescue_add(&result, XJ380_INSTALLER_CHECK_OK, 0, "安装日志", "日志面板会显示最近的安装器日志。");
        if (!copy_to_user_pagedir(pagedir, (void *)out, &result, sizeof(result))) return (uint64_t)-EFAULT;
        return 0;
    }

    if (disk_id >= 256) return (uint64_t)-EINVAL;
    device_t *disk = get_device((size_t)disk_id);
    if (disk == NULL || disk->type != DEVICE_BLOCK || disk->flag < 1)
    {
        result.result = -ENODEV;
        installer_rescue_add(&result, XJ380_INSTALLER_CHECK_ERROR, 1, "目标硬盘不可用", "内核找不到指定的块设备。");
        copy_to_user_pagedir(pagedir, (void *)out, &result, sizeof(result));
        return (uint64_t)result.result;
    }

    char detail[XJ380_INSTALLER_DETAIL_LEN];
    switch (action)
    {
    case XJ380_INSTALLER_RESCUE_CHECK_DISK:
    {
        xj380_installer_precheck check;
        int ret = installer_build_precheck((size_t)disk_id, XJ380_INSTALLER_MODE_FRESH,
                                           XJ380_INSTALLER_COMPONENT_DEFAULT, &check);
        result.result = ret;
        for (uint32_t i = 0; i < check.item_count && result.item_count < XJ380_INSTALLER_RESCUE_ITEMS; i++)
            installer_rescue_add(&result, check.items[i].status, check.items[i].code, check.items[i].title, check.items[i].detail);
        break;
    }
    case XJ380_INSTALLER_RESCUE_VIEW_DISK:
        snprintf(detail, sizeof(detail), "设备名 %s，容量 %llu MiB，扇区 %zu 字节，标志 0x%llx。",
                 disk->drive_name, disk->size / 1024ULL / 1024ULL, disk->sector_size,
                 (unsigned long long)installer_disk_flags(disk));
        installer_rescue_add(&result, XJ380_INSTALLER_CHECK_OK, 0, "硬盘信息", detail);
        partition_rescan_device((size_t)disk_id, false);
        snprintf(detail, sizeof(detail), "第一分区编号：%d。", partition_find_first_for_disk((size_t)disk_id));
        installer_rescue_add(&result, XJ380_INSTALLER_CHECK_OK, 0, "分区扫描", detail);
        break;
    case XJ380_INSTALLER_RESCUE_REBUILD_BOOT:
    {
        size_t payload_size = 0;
        const uint8_t *payload = installer_payload_virt(installer_current_boot_config(), &payload_size);
        int ret = installer_mount_first_partition((size_t)disk_id, "查找分区失败", 0, NULL, 0);
        if (ret >= 0)
        {
            ret = installer_repair_boot_on_mounted_target(payload, payload_size);
            vfs_unmount("/target");
        }
        result.result = ret;
        if (ret < 0)
            installer_rescue_add(&result, XJ380_INSTALLER_CHECK_ERROR, (uint32_t)-ret, "重建引导失败", "无法写入 EFI 引导文件。");
        else
            installer_rescue_add(&result, XJ380_INSTALLER_CHECK_OK, 0, "重建引导完成", "EFI/BOOT/BOOTX64.efi 和系统内核已写入。");
        break;
    }
    default:
        result.result = -EINVAL;
        installer_rescue_add(&result, XJ380_INSTALLER_CHECK_ERROR, 1, "未知救援操作", "安装器不支持该救援命令。");
        break;
    }

    if (!copy_to_user_pagedir(pagedir, (void *)out, &result, sizeof(result))) return (uint64_t)-EFAULT;
    return (uint64_t)result.result;
}

uint64_t do_xapi_InstallerLog(uint64_t out)
{
    if (out == 0) return (uint64_t)-EINVAL;
    page_directory_t *pagedir = xapi_current_pagedir();
    if (pagedir == NULL) return (uint64_t)-EFAULT;
    if (!copy_to_user_pagedir(pagedir, (void *)out, &g_installer_log, sizeof(g_installer_log)))
        return (uint64_t)-EFAULT;
    return 0;
}
