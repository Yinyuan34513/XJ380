#include <device.h>
#include <efi/boot.h>
#include <errno.h>
#include <fs/fatfs/fatfs.h>
#include <fs/partition.h>
#include <fs/vfs/vfs.h>
#include <installer_mode.h>
#include <installer_protocol.h>
#include <krlibc.h>
#include <mm/uaccess.h>
#include <proto.hpp>
#include <syscall/pxapi.h>
#include <syscall/xapi_user.h>
#include <task/pcb.h>
#include <task/scheduler.h>

extern device_t device_ctl[256];

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
static xj380_installer_progress g_installer_progress = {
    XJ380_INSTALLER_IDLE,
    0,
    0,
    "等待安装",
    "请选择目标硬盘，然后开始安装。",
};

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

static void installer_set_progress(uint32_t state, uint32_t percent, int64_t result,
                                    const char *stage, const char *detail)
{
    if (percent > 100) percent = 100;
    g_installer_progress.state = state;
    g_installer_progress.percent = percent;
    g_installer_progress.result = result;
    memset(g_installer_progress.stage, 0, sizeof(g_installer_progress.stage));
    memset(g_installer_progress.detail, 0, sizeof(g_installer_progress.detail));
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

    size_t chunk_sectors = 1;
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

static int installer_mkdir_parent(const char *path)
{
    if (path == NULL || path[0] != '/') return -EINVAL;
    char temp[512];
    size_t len = strlen(path);
    if (len >= sizeof(temp)) return -ENAMETOOLONG;
    strcpy(temp, path);
    for (size_t i = 1; temp[i] != '\0'; i++)
    {
        if (temp[i] != '/') continue;
        temp[i] = '\0';
        if (temp[0] != '\0') vfs_mkdir(temp);
        temp[i] = '/';
    }
    return EOK;
}

static int installer_write_payload_file(const char *path, const uint8_t *data, size_t size)
{
    int ret = installer_mkdir_parent(path);
    if (ret < 0) return ret;
    vfs_node_t exists = vfs_open_no_follow(path);
    if (exists != NULL)
    {
        vfs_delete(exists);
    }
    vfs_mkfile(path);
    vfs_node_t node = vfs_open(path);
    if (node == NULL) return -ENOENT;
    size_t written = 0;
    while (written < size)
    {
        size_t chunk = MIN((size_t)0x10000, size - written);
        size_t got = vfs_write(node, (void *)(data + written), written, chunk);
        if (got != chunk)
        {
            vfs_close(node);
            return -EIO;
        }
        written += got;
    }
    vfs_close(node);
    return EOK;
}

static int installer_unpack_pak_to_target(const uint8_t *pak, size_t pak_size, const char *target_prefix)
{
    if (pak == NULL || pak_size < sizeof(xj380_pak_header) || target_prefix == NULL) return -EINVAL;
    const xj380_pak_header *header = (const xj380_pak_header *)pak;
    if (memcmp(header->magic, XJ380_INSTALLER_PAK_MAGIC, XJ380_INSTALLER_PAK_MAGIC_SIZE) != 0) return -EINVAL;

    size_t off = sizeof(*header);
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

        char out_path[640];
        snprintf(out_path, sizeof(out_path), "%s%s", target_prefix, rel);
        if (entry->type == XJ380_PAK_ENTRY_DIR)
        {
            vfs_mkdir(out_path);
        }
        else if (entry->type == XJ380_PAK_ENTRY_FILE)
        {
            if (off > pak_size || entry->size > pak_size - off) return -EINVAL;
            int ret = installer_write_payload_file(out_path, pak + off, (size_t)entry->size);
            if (ret < 0) return ret;
            off += (size_t)entry->size;
        }
        else
        {
            return -EINVAL;
        }

        if ((i & 7) == 0)
        {
            uint64_t denom = header->entry_count == 0 ? 1 : header->entry_count;
            uint32_t percent = 55 + (uint32_t)((i * 40) / denom);
            installer_set_progress(XJ380_INSTALLER_RUNNING, percent, 0, "正在复制系统文件", rel);
            scheduler_yield();
        }
    }
    return EOK;
}

static const uint8_t *installer_payload_virt(const BOOT_CONFIG *config, size_t *out_size)
{
    if (out_size == NULL || config == NULL || config->system_payload_pak == 0 || config->system_payload_pak_size == 0)
        return NULL;
    *out_size = (size_t)config->system_payload_pak_size;
    return (const uint8_t *)(config->system_payload_pak + 0xffff800000000000ULL);
}

static int installer_install_to_disk(size_t disk_id)
{
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

    write_serial_fmt("installer: start disk=%s id=%zu size=%llu sector=%zu payload=%zu\n",
                     disk->drive_name, disk_id, disk->size, disk->sector_size, payload_size);

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

    ret = installer_unpack_pak_to_target(payload, payload_size, "/target");
    if (ret < 0) return installer_fail(ret, g_installer_progress.percent, "复制系统失败", g_installer_progress.detail);

    installer_set_progress(XJ380_INSTALLER_RUNNING, 97, 0, "正在完成安装", "正在同步并卸载目标分区。");
    vfs_unmount("/target");
    installer_set_progress(XJ380_INSTALLER_DONE, 100, 0, "安装完成", "XJ380 已安装到硬盘。请关机后移除 ISO 安装介质。");
    return EOK;
}

static void installer_worker(void *arg)
{
    size_t disk_id = (size_t)(uintptr_t)arg;
    int ret = installer_install_to_disk(disk_id);
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
        out->flags = 0;
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
    const BOOT_CONFIG *config = installer_current_boot_config();
    if (config == NULL || !installer_boot_active(*config)) return (uint64_t)-ENOSYS;
    if (disk_id >= 256) return (uint64_t)-EINVAL;
    if (__sync_lock_test_and_set(&g_installer_running, 1) != 0) return (uint64_t)-EBUSY;

    g_installer_target_disk = (size_t)disk_id;
    installer_set_progress(XJ380_INSTALLER_RUNNING, 0, 0, "准备安装", "安装程序正在初始化。");
    size_t tid = create_kernel_thread((void *)installer_worker, (void *)(uintptr_t)disk_id,
                                      (char *)"XJ380 Installer", NULL);
    if ((int64_t)tid < 0)
    {
        g_installer_running = 0;
        installer_set_progress(XJ380_INSTALLER_FAILED, 0, (int64_t)tid, "无法启动安装线程",
                               "内核无法创建安装工作线程。");
        return tid;
    }
    return 0;
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
