#include "fs/partition.h"
#include "device.h"
#include "errno.h"
#include "fs/vfs/vfs.h"
#include "krlibc.h"
#include <ioctl.h>

extern device_t device_ctl[256];
partition_t     partitions[MAX_PARTITIONS_NUM];
size_t          partition_num = 0;
partition_t    *device_lists[MAX_PARTITIONS_NUM];

static void partition_dump_prefix(const char *name, const uint8_t *buf, size_t bytes)
{
    size_t dump_len = bytes > 64 ? 64 : bytes;
    write_serial_fmt("PART: first %zu bytes of %s:", dump_len, name);
    for (size_t i = 0; i < dump_len; ++i)
    {
        if ((i % 16) == 0) write_serial_string("\nPART:   ");
        write_serial_fmt("%02x ", buf[i]);
    }
    write_serial_string("\n");
}

static bool partition_read_exact(vfs_node_t device, device_t disk, void *buf, size_t offset, size_t bytes,
                                 const char *tag)
{
    size_t got      = vfs_read(device, buf, offset, bytes);
    if (got != bytes)
    {
        write_serial_fmt("PART: %s short read offset=0x%zx bytes=0x%zx got=0x%zx sector_size=%zu\n", tag, offset,
                         bytes, got, disk.sector_size);
        return false;
    }
    return true;
}

size_t partition_read(int part, uint8_t *buf, size_t number, size_t lba)
{
    partition_t *partition = device_lists[part];
    // write_serial_fmt("Y K %s %d\n",device_ctl[partition->vdisk_id].drive_name,partition->vdisk_id);
    return device_ctl[partition->vdisk_id].read(partition->vdisk_id, buf, number, partition->starting_lba + lba);
}

size_t partition_write(int part, uint8_t *buf, size_t number, size_t lba)
{
    partition_t *partition = device_lists[part];
    return device_ctl[partition->vdisk_id].write(partition->vdisk_id, buf, number, partition->starting_lba + lba);
}

void format_guid(const uint8_t guid[16], char out[37])
{
    snprintf(out, 37, "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X", guid[3], guid[2], guid[1],
             guid[0],                                                   // time_low
             guid[5], guid[4],                                          // time_mid
             guid[7], guid[6],                                          // time_hi_and_version
             guid[8], guid[9],                                          // clock_seq
             guid[10], guid[11], guid[12], guid[13], guid[14], guid[15] // node
    );
}

bool is_partition_used(struct GPT_DPTE *entry)
{
    if (entry->starting_lba == 0 || entry->ending_lba == 0) return false;

    const uint8_t *partition_type_guid = entry->partition_type_guid;
    for (int i = 0; i < 16; ++i)
    {
        if (partition_type_guid[i] != 0) return true;
    }
    return false;
}
static bool parse_gpt_partitions(vfs_node_t device, struct GPT_DPT *gpt, device_t disk, size_t vdisk_id)
{
    if (memcmp(gpt->signature, "EFI PART", 8) != 0 || gpt->num_partition_entries == 0 ||
        gpt->partition_entry_lba == 0 || gpt->size_of_partition_entry < sizeof(struct GPT_DPTE) ||
        gpt->size_of_partition_entry > 512)
    {
        return false;
    }

    size_t           dptes_size = (size_t)gpt->num_partition_entries * gpt->size_of_partition_entry;
    struct GPT_DPTE *dptes      = (struct GPT_DPTE *)malloc(dptes_size);
    memset(dptes,0,dptes_size);
    if (!dptes) return false;

    if (!partition_read_exact(device, disk, (uint8_t *)dptes, gpt->partition_entry_lba * disk.sector_size, dptes_size,
                              "GPT entries"))
    {
        free(dptes);
        return false;
    }

    for (size_t j = 0; j < gpt->num_partition_entries; j++)
    {
        struct GPT_DPTE *entry = (struct GPT_DPTE *)((uint8_t *)dptes + j * gpt->size_of_partition_entry);
        if (is_partition_used(entry))
        {
            partition_t *partition  = &partitions[partition_num];
            partition->vdisk_id     = vdisk_id;
            partition->starting_lba = entry->starting_lba;
            partition->ending_lba   = entry->ending_lba;
            partition->type         = partition->GPT;
            partition->sector_size  = disk.sector_size;
            partition->is_used      = true;
            memcpy(partition->disk_guid, gpt->disk_guid, 16);
            memcpy(partition->partition_type_guid, entry->partition_type_guid, 16);
            memcpy(partition->unique_partition_guid, entry->unique_partition_guid, 16);
            memcpy(partition->partition_name, entry->partition_name, 36 * 2);
            partition_num++;
            char out[37];
            format_guid(entry->unique_partition_guid, out);
            write_serial_fmt("GPT Partition(%s) %zu GUID: %s\n", disk.drive_name, partition_num, out);
        }
    }

    free(dptes);
    return true;
}
// Try GPT header directly at LBA1.
static bool try_gpt_at_lba1(vfs_node_t device, device_t disk, size_t vdisk_id)
{
    struct GPT_DPT *gpt = (struct GPT_DPT *)malloc(disk.sector_size);
    memset(gpt,0,disk.sector_size);
    if (!gpt) return false;

    if (!partition_read_exact(device, disk, (uint8_t *)gpt, disk.sector_size, disk.sector_size, "GPT header"))
    {
        free(gpt);
        return false;
    }

    bool is_valid = (memcmp(gpt->signature, "EFI PART", 8) == 0);
    if (is_valid)
    {
        write_serial_fmt("Detected GPT at LBA1 (e.g., hybrid ISO)");
        bool ok = parse_gpt_partitions(device, gpt, disk, vdisk_id);
        free(gpt);
        return ok;
    }

    free(gpt);
    return false;
}
bool parser_block_device(vfs_node_t device, device_t disk, size_t vdisk_id)
{
    uint8_t *mbr = (uint8_t *)malloc(disk.sector_size * 4);
    memset(mbr,0,disk.sector_size*4);
    write_serial_fmt("PART: read %s sector_size=%zu size=0x%zx\n", disk.drive_name, disk.sector_size, disk.size);
    bool read_ok = false;
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        memset(mbr, 0, disk.sector_size * 4);
        if (partition_read_exact(device, disk, mbr, 0, disk.sector_size * 4, "MBR"))
        {
            read_ok = true;
            write_serial_fmt("PART: MBR read attempt=%d signature=%02x%02x type0=%02x\n", attempt + 1, mbr[0x1FE],
                             mbr[0x1FF], mbr[0x1BE + 4]);
            if (mbr[0x1FE] == 0x55 && mbr[0x1FF] == 0xAA) break;
        }
        else
        {
            write_serial_fmt("PART: retrying %s after failed read attempt=%d\n", disk.drive_name, attempt + 1);
        }
        delay_ms_hp(10);
    }
    if (!read_ok) return false;
    // for(int i=0;i<disk.sector_size*4;i++)
    // {
    //     if(i%32==0 && i!=0)
    //     {
    //         write_serial_fmt("\n");
    //         delay_ms_hp(500);
    //     }
    //     write_serial_fmt("0x%x ", mbr[i]);
    // }
    // write_serial_fmt("\n");
    write_serial_fmt("PART: MBR signature=%02x%02x type0=%02x\n", mbr[0x1FE], mbr[0x1FF], mbr[0x1BE + 4]);
    if (mbr[0x1FE] != 0x55 || mbr[0x1FF] != 0xAA) {
        partition_dump_prefix(disk.drive_name, mbr, disk.sector_size);
    }
    if (mbr[0x1FE] == 0x55 && mbr[0x1FF] == 0xAA)
    {
        uint8_t part_type = mbr[0x1BE + 4];
        if (part_type == 0xEE)
        {
            struct GPT_DPT *gpt = (struct GPT_DPT *)malloc(disk.sector_size);
            memset(gpt,0,disk.sector_size);
            if (!partition_read_exact(device, disk, gpt, 1 * disk.sector_size, disk.sector_size, "GPT header"))
            {
                free(gpt);
                free(mbr);
                return false;
            }
            write_serial_fmt("PART: GPT sig=%.8s rev=0x%x hdr=%u pe_lba=%llu entries=%u entry_size=%u\n",
                             gpt->signature, gpt->revision, gpt->header_size, gpt->partition_entry_lba,
                             gpt->num_partition_entries, gpt->size_of_partition_entry);
            if (memcmp(gpt->signature, GPT_HEADER_SIGNATURE, 8) || gpt->num_partition_entries == 0 ||
                gpt->partition_entry_lba == 0)
            {
                free(gpt);
                return false;
            }
            char disk_uid[37];
            format_guid(gpt->disk_guid, disk_uid);

            size_t           dptes_size = gpt->num_partition_entries * gpt->size_of_partition_entry;
            struct GPT_DPTE *dptes      = (struct GPT_DPTE *)malloc(dptes_size);
            memset(dptes,0,dptes_size);
            if (!partition_read_exact(device, disk, dptes, gpt->partition_entry_lba * disk.sector_size, dptes_size,
                                      "GPT entries"))
            {
                free(dptes);
                free(gpt);
                free(mbr);
                return false;
            }
            for (size_t j = 0; j < gpt->num_partition_entries; j++)
            {
                struct GPT_DPTE *entry = (struct GPT_DPTE *)((uint8_t *)dptes + j * gpt->size_of_partition_entry);
                if (is_partition_used(entry))
                {
                    partition_t *partition  = &partitions[partition_num];
                    partition->vdisk_id     = vdisk_id;
                    partition->starting_lba = entry->starting_lba;
                    partition->ending_lba   = entry->ending_lba;
                    partition->type         = partition->GPT;
                    partition->sector_size  = disk.sector_size;
                    partition->is_used      = true;
                    memcpy(partition->disk_guid, gpt->disk_guid, 16);
                    memcpy(partition->partition_type_guid, entry->partition_type_guid, 16);
                    memcpy(partition->unique_partition_guid, entry->unique_partition_guid, 16);
                    memcpy(partition->partition_name, entry->partition_name, 36 * 2);
                    partition_num++;
                    char out[37];
                    format_guid(entry->unique_partition_guid, out);
                    write_serial_fmt("GPT Partition(%s) %zu GUID: %s\n", disk.drive_name, partition_num, out);
                }
            }
            free(dptes);
            free(gpt);
        }
        else
        {
            struct MBR_DPT *boot_sector = (struct MBR_DPT *)mbr;
            if (boot_sector->bs_trail_sig != 0xAA55) { goto end; }
            for (int j = 0; j < MBR_MAX_PARTITION_NUM; j++)
            {
                if (boot_sector->dpte[j].start_lba == 0 || boot_sector->dpte[j].sectors_limit == 0) continue;
                size_t       starting_lba = boot_sector->dpte[j].start_lba;
                size_t       ending_lba   = boot_sector->dpte[j].sectors_limit;
                partition_t *partition    = &partitions[partition_num];
                partition->vdisk_id       = vdisk_id;
                partition->starting_lba   = starting_lba;
                partition->ending_lba     = ending_lba;
                partition->type           = partition->MBR;
                partition->sector_size    = disk.sector_size;
                partition->is_used        = true;

                write_serial_fmt("MBR Partition(%s) %d lba=%llu..%llu %s\n", disk.drive_name, j, starting_lba,
                                 ending_lba, (boot_sector->dpte[j].flags & 0x80) != 0 ? "bootable" : "");
                partition_num++;
            }
        }
    }
    else
    {
        if (try_gpt_at_lba1(device, disk, vdisk_id))
        {
            free(mbr);
            return true;
        }
    }
end:
    free(mbr);
    return true;
}

int extract_partition_index(const char *name)
{
    const char *prefix     = "part";
    size_t      prefix_len = strlen(prefix);
    if (strncmp(name, prefix, prefix_len) != 0) { return -1; }
    const char *suffix = name + prefix_len;
    for (const char *p = suffix; *p; p++)
    {
        if (!isdigit((unsigned char)*p)) { return -1; }
    }
    return atoi(suffix);
}

int partition_ioctl(device_t *device, size_t req, void *arg)
{
    int device_id = extract_partition_index(device->drive_name);
    if (device_id == -1) return -ENODEV;
    partition_t partition = partitions[device_id];
    switch (req)
    {
    case IOGPTYPE: {
        if (arg == NULL) return -EINVAL;
        uint16_t *type = (uint16_t *)arg;
        *type          = partition.type == partition.GPT   ? PARTITION_TYPE_GPT
                         : partition.type == partition.MBR ? PARTITION_TYPE_MBR
                                                           : PARTITION_TYPE_UNKNOWN;
        break;
    }
    case IOGPINFO: {
        if (arg == NULL) return -EINVAL;
        if (partition.type == partition.GPT)
        {
            struct ioctl_gpt_partition *info = (struct ioctl_gpt_partition *)arg;
            memcpy(info->disk_guid, partition.disk_guid, 16);
            memcpy(info->partition_type_guid, partition.partition_type_guid, 16);
            memcpy(info->unique_partition_guid, partition.unique_partition_guid, 16);
            memcpy(info->name, partition.partition_name, 36);
            info->size  = partition.sector_size;
            info->start = partition.starting_lba;
            info->end   = partition.ending_lba;
        }
    }
    break;
    }
    return EOK;
}

void partition_init()
{
    for (size_t i = 0; i < 256; i++)
    {
        if (device_ctl[i].flag >= 1 && device_ctl[i].type == DEVICE_BLOCK)
        {
            char buf[50];
            sprintf(buf, "/dev/%s", device_ctl[i].drive_name);
            write_serial_fmt("PART: %s\n", buf);
            vfs_node_t device = vfs_open(buf);
            if (device == NULL)
            {
                write_serial_fmt("part: Partition init failed, device %s not found.\n", device_ctl[i].drive_name);
                continue;
            }
            else
            {
                write_serial_fmt("part: Partition init success, device %s found.\n", device_ctl[i].drive_name);
            }
            if (parser_block_device(device, device_ctl[i], i)) {}
        }
    }

    write_serial_fmt("Loading part device...\n");

    for (size_t j = 0; j < partition_num; j++)
    {
        partition_t partition = partitions[j];
        device_t    part;
        memset(&part, 0, sizeof(part));
        part.sector_size  = partition.sector_size;
        part.flag         = 1;
        part.type         = DEVICE_BLOCK;
        part.read         = partition_read;
        part.write        = partition_write;
        part.ioctl        = partition_ioctl;
        part.poll         = (pollf)empty;
        part.map          = (mapf)empty;
        part.read_vbuf    = NULL;
        part.write_vbuf   = NULL;
        part.size         = (partition.ending_lba - partition.starting_lba + 1) * partition.sector_size;
        device_t *src_dev = get_device(partition.vdisk_id);
        part.max_size     = src_dev ? src_dev->max_size : __UINT64_MAX__;
        char      buf[20];
        sprintf(buf, "%sp%zu", src_dev->drive_name, j);
        strcpy(part.drive_name, buf);
        int id           = regist_device(NULL, part);
        device_lists[id] = &partitions[j];
    }
}
void mount_root()
{
    bool err = true;

    for (uint64_t i = 0; i < partition_num; i++)
    {
        partition_t p         = partitions[i];
        device_t   *src_dev   = get_device(p.vdisk_id);
        char        buf[1024] = {};
        sprintf(buf, "/dev/%sp%zu", src_dev->drive_name, i);
        write_serial_string(buf);
        write_serial_string("\n");

        if (!vfs_mount((const char *)buf, rootdir))
        {
            err = false;
            write_serial_fmt("Mount root OK!\n");
            break;
        }
    }

    if (err)
    {
        write_serial_fmt("Mount root failed\n");
        while (1)
        {
            __asm__ volatile("pause\n\t");
        }
    }
}
