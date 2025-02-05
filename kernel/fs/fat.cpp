#include "../../include/include.h"

extern VNodeOps fat32_ops;

uint32_t fat32_read_fat_entry(FAT32_NodeInfo *fsbi, uint32_t fat_entry)
{
    uint32_t buf[128];
    xmemset(buf, 0, 512);
    device_read(fsbi->dev, (uint8_t *)buf, 1, fsbi->FAT1_firstsector + (fat_entry >> 7), 0);
    return buf[fat_entry & 0x7f] & 0x0fffffff;
}

uint64_t fat32_write_fat_entry(FAT32_NodeInfo *fsbi, uint32_t fat_entry, uint32_t value)
{
    uint32_t buf[128];
    int i;

    xmemset(buf, 0, 512);
    device_read(fsbi->dev, (uint8_t *)buf, 1, fsbi->FAT1_firstsector + (fat_entry >> 7), 0);
    buf[fat_entry & 0x7f] = (buf[fat_entry & 0x7f] & 0xf0000000) | (value & 0x0fffffff);

    for (i = 0; i < fsbi->NumFATs; i++)
        device_write(fsbi->dev, buf, 1, fsbi->FAT1_firstsector + fsbi->sector_per_FAT * i + (fat_entry >> 7), 0);
    return 1;
}

uint64_t fat32_find_available_cluster(FAT32_NodeInfo *fsbi)
{
    int i, j;
    int fat_entry;
    uint64_t sector_per_fat = fsbi->sector_per_FAT;
    uint32_t buf[128];

    //	fsbi->fat_fsinfo->FSI_Free_Count & fsbi->fat_fsinfo->FSI_Nxt_Free not exactly,so unuse

    for (i = 0; i < sector_per_fat; i++)
    {
        xmemset(buf, 0, 512);
        device_read(fsbi->dev, buf, 1, fsbi->FAT1_firstsector + i, 0);

        for (j = 0; j < 128; j++)
        {
            if ((buf[j] & 0x0fffffff) == 0)
                return (i << 7) + j;
        }
    }
    return 0;
}

int fat_write_node(VNode *node)
{
    struct FAT32_Directory *fdentry = NULL;
    struct FAT32_Directory *buf = NULL;
    FAT32_NodeInfo *info = (FAT32_NodeInfo *)node->desc;
    unsigned long sector = 0;

    if (info->dentry_location == 0)
    {
        write_serial_string("FS ERROR:write root inode!\n");
        return -1;
    }

    sector = info->Data_firstsector + (info->dentry_location - 2) * info->sector_per_cluster;
    buf = (struct FAT32_Directory *)malloc(info->bytes_per_cluster);
    xmemset(buf, 0, info->bytes_per_cluster);
    device_read(info->dev, buf, info->sector_per_cluster, sector, 0);
    fdentry = buf + info->dentry_position;

    ////alert fat32 dentry data
    fdentry->DIR_FileSize = node->file_size;
    fdentry->DIR_FstClusLO = info->first_cluster & 0xffff;
    fdentry->DIR_FstClusHI = (fdentry->DIR_FstClusHI & 0xf000) | (info->first_cluster >> 16);

    device_write(info->dev, buf, info->sector_per_cluster, sector, 0);

    free(buf);
}

int fat_super(int dev, VNode *result)
{
    VNode *node = result;

    uint8_t *super_buf = (uint8_t *)malloc(512);
    device_read(dev, super_buf, 1, 0, 0);

    struct FAT32_BootSector *bootsector = (struct FAT32_BootSector *)super_buf;

    FAT32_NodeInfo *desc = (FAT32_NodeInfo *)malloc(sizeof(FAT32_NodeInfo));
    desc->sector_per_cluster = bootsector->BPB_SecPerClus;
    desc->bytes_per_cluster = bootsector->BPB_SecPerClus * bootsector->BPB_BytesPerSec;
    desc->bytes_per_sector = bootsector->BPB_BytesPerSec;
    desc->Data_firstsector = bootsector->BPB_RsvdSecCnt + bootsector->BPB_FATSz32 * bootsector->BPB_NumFATs;
    desc->FAT1_firstsector = bootsector->BPB_RsvdSecCnt;
    desc->sector_per_FAT = bootsector->BPB_FATSz32;
    desc->NumFATs = bootsector->BPB_NumFATs;
    desc->fsinfo_sector_infat = bootsector->BPB_FSInfo;
    desc->bootsector_bk_infat = bootsector->BPB_BkBootSec;

    char *lab = (char *)malloc(11);
    strncpy(lab, (const char *)bootsector->BS_VolLab, 11);
    write_serial_string("FAT32 label: ");
    write_serial_string(lab);
    write_serial_string("\n");

    if (part_strcmp((char *)bootsector->BS_FilSysType, "FAT32", 5))
    {
        return -1;
    }

    desc->fs_info = (struct FAT32_FSInfo *)malloc(sizeof(struct FAT32_FSInfo));
    xmemset(desc->fs_info, 0, sizeof(struct FAT32_FSInfo));
    device_read(dev, desc->fs_info, 1, (int)desc->fsinfo_sector_infat, 0);

    write_serial_string("FAT32 FSInfo ");
    write_serial_string("FSI_Free_Count: ");
    write_serial_hex(desc->fs_info->FSI_Free_Count);
    write_serial_string("\n");

    desc->first_cluster = bootsector->BPB_RootClus;
    desc->dentry_location = 0;
    desc->dentry_position = 0;
    desc->create_date = 0;
    desc->create_time = 0;
    desc->write_date = 0;
    desc->write_time = 0;

    desc->dev = dev;

    node->desc = desc;

    return 0;
}

int fat_lookup(VNode *node, VNode *search_node)
{
    FAT32_NodeInfo *info = (FAT32_NodeInfo *)node->desc;

    uint32_t cluster = 0;
    uint64_t sector = 0;
    uint8_t *buf = nullptr;
    int i = 0, j = 0, x = 0;

    struct FAT32_Directory *tmpdentry = nullptr;
    struct FAT32_LongDirectory *tmpldentry = nullptr;

    buf = (uint8_t *)malloc(info->bytes_per_cluster);

    cluster = info->first_cluster;

next_cluster:
    sector = info->Data_firstsector + (cluster - 2) * info->sector_per_cluster;
    if (device_read(info->dev, buf, info->sector_per_cluster, sector, 0) != 0)
    {
        write_serial_string("FAT32 FS(lookup) read disk error!!!!!!!!!!");
        free(buf);
        return 0;
    }

    tmpdentry = (struct FAT32_Directory *)buf;

    for (i = 0; i < info->bytes_per_cluster; i += 32, tmpdentry++)
    {
        if (tmpdentry->DIR_Attr == ATTR_LONG_NAME)
            continue;
        if (tmpdentry->DIR_Name[0] == 0xe5 || tmpdentry->DIR_Name[0] == 0x00 || tmpdentry->DIR_Name[0] == 0x05)
            continue;

        tmpldentry = (struct FAT32_LongDirectory *)tmpdentry - 1;
        j = 0;

        while (tmpldentry->LDIR_Attr == ATTR_LONG_NAME && tmpldentry->LDIR_Ord != 0xe5)
        {
            for (x = 0; x < 5; x++)
            {
                if (j > search_node->name_len && tmpldentry->LDIR_Name1[x] == 0xffff)
                    continue;
                else if (j > search_node->name_len || tmpldentry->LDIR_Name1[x] != (unsigned short)(search_node->name[j++]))
                    goto continue_cmp_fail;
            }
            for (x = 0; x < 6; x++)
            {
                if (j > search_node->name_len && tmpldentry->LDIR_Name2[x] == 0xffff)
                    continue;
                else if (j > search_node->name_len || tmpldentry->LDIR_Name2[x] != (unsigned short)(search_node->name[j++]))
                    goto continue_cmp_fail;
            }
            for (x = 0; x < 2; x++)
            {
                if (j > search_node->name_len && tmpldentry->LDIR_Name3[x] == 0xffff)
                    continue;
                else if (j > search_node->name_len || tmpldentry->LDIR_Name3[x] != (unsigned short)(search_node->name[j++]))
                    goto continue_cmp_fail;
            }

            if (j >= search_node->name_len)
            {
                goto find_lookup_success;
            }

            tmpldentry--;
        }

        j = 0;

        // long file/dir name compare
        while (tmpldentry->LDIR_Attr == ATTR_LONG_NAME && tmpldentry->LDIR_Ord != 0xe5)
        {
            for (x = 0; x < 5; x++)
            {
                if (j > search_node->name_len && tmpldentry->LDIR_Name1[x] == 0xffff)
                    continue;
                else if (j > search_node->name_len || tmpldentry->LDIR_Name1[x] != (unsigned short)(search_node->name[j++]))
                    goto continue_cmp_fail;
            }
            for (x = 0; x < 6; x++)
            {
                if (j > search_node->name_len && tmpldentry->LDIR_Name2[x] == 0xffff)
                    continue;
                else if (j > search_node->name_len || tmpldentry->LDIR_Name2[x] != (unsigned short)(search_node->name[j++]))
                    goto continue_cmp_fail;
            }
            for (x = 0; x < 2; x++)
            {
                if (j > search_node->name_len && tmpldentry->LDIR_Name3[x] == 0xffff)
                    continue;
                else if (j > search_node->name_len || tmpldentry->LDIR_Name3[x] != (unsigned short)(search_node->name[j++]))
                    goto continue_cmp_fail;
            }

            if (j >= search_node->name_len)
            {
                goto find_lookup_success;
            }

            tmpldentry--;
        }

        // short file/dir base name compare
        j = 0;
        for (x = 0; x < 8; x++)
        {
            switch (tmpdentry->DIR_Name[x])
            {
            case ' ':
                if (!(tmpdentry->DIR_Attr & ATTR_DIRECTORY))
                {
                    if (search_node->name[j] == '.')
                        continue;
                    else if (tmpdentry->DIR_Name[x] == search_node->name[j])
                    {
                        j++;
                        break;
                    }
                    else
                        goto continue_cmp_fail;
                }
                else
                {
                    if (j < search_node->name_len && tmpdentry->DIR_Name[x] == search_node->name[j])
                    {
                        j++;
                        break;
                    }
                    else if (j == search_node->name_len)
                        continue;
                    else
                        goto continue_cmp_fail;
                }

            case 'A' ... 'Z':
            case 'a' ... 'z':
                if (tmpdentry->DIR_NTRes & LOWERCASE_BASE)
                    if (j < search_node->name_len && tmpdentry->DIR_Name[x] + 32 == search_node->name[j])
                    {
                        j++;
                        break;
                    }
                    else
                        goto continue_cmp_fail;
                else
                {
                    if (j < search_node->name_len && tmpdentry->DIR_Name[x] == search_node->name[j])
                    {
                        j++;
                        break;
                    }
                    else
                        goto continue_cmp_fail;
                }

            case '0' ... '9':
                if (j < search_node->name_len && tmpdentry->DIR_Name[x] == search_node->name[j])
                {
                    j++;
                    break;
                }
                else
                    goto continue_cmp_fail;

            default:
                j++;
                break;
            }
        }
        // short file ext name compare
        if (!(tmpdentry->DIR_Attr & ATTR_DIRECTORY))
        {
            j++;
            for (x = 8; x < 11; x++)
            {
                switch (tmpdentry->DIR_Name[x])
                {
                case 'A' ... 'Z':
                case 'a' ... 'z':
                    if (tmpdentry->DIR_NTRes & LOWERCASE_EXT)
                        if (tmpdentry->DIR_Name[x] + 32 == search_node->name[j])
                        {
                            j++;
                            break;
                        }
                        else
                            goto continue_cmp_fail;
                    else
                    {
                        if (tmpdentry->DIR_Name[x] == search_node->name[j])
                        {
                            j++;
                            break;
                        }
                        else
                            goto continue_cmp_fail;
                    }

                case '0' ... '9':
                    if (tmpdentry->DIR_Name[x] == search_node->name[j])
                    {
                        j++;
                        break;
                    }
                    else
                        goto continue_cmp_fail;

                case ' ':
                    if (tmpdentry->DIR_Name[x] == search_node->name[j])
                    {
                        j++;
                        break;
                    }
                    else
                        goto continue_cmp_fail;

                default:
                    goto continue_cmp_fail;
                }
            }
        }
        goto find_lookup_success;
    continue_cmp_fail:;
    }

    cluster = fat32_read_fat_entry(info, cluster);
    if (cluster < 0x0ffffff7)
        goto next_cluster;

    free(buf);
    return 0;

find_lookup_success:
    FAT32_NodeInfo *desc = (FAT32_NodeInfo *)malloc(sizeof(FAT32_NodeInfo));

    xmemcpy(desc, info, sizeof(FAT32_NodeInfo));
    desc->first_cluster = (tmpdentry->DIR_FstClusHI << 16 | tmpdentry->DIR_FstClusLO) & 0x0fffffff;
    desc->dentry_location = cluster;
    desc->dentry_position = tmpdentry - (struct FAT32_Directory *)buf;
    desc->create_date = tmpdentry->DIR_CrtDate;
    desc->create_time = tmpdentry->DIR_CrtTime;
    desc->write_date = tmpdentry->DIR_WrtDate;
    desc->write_time = tmpdentry->DIR_WrtTime;

    search_node->ops = &fat32_ops;
    search_node->desc = desc;
    search_node->type = (tmpdentry->DIR_Attr & ATTR_DIRECTORY) ? NODE_TYPE_DIR : NODE_TYPE_FILE;
    search_node->file_size = tmpdentry->DIR_FileSize;

    free(buf);
    return 1;
}

int fat_open(VNode *parent, const char *name, VNode *node)
{
    return 0;
}

void fat_close(VNode *current)
{
}

int fat_resize(VNode *current, uint64_t size)
{
    return 0;
}

int fat_read(VNode *node, uint8_t *buf, size_t position, size_t count)
{
    FAT32_NodeInfo *info = (FAT32_NodeInfo *)node->desc;

    uint64_t cluster = info->first_cluster;
    uint64_t sector = 0;
    int i, length = 0;
    int64_t retval = 0;
    int index = position / info->bytes_per_cluster;
    long offset = position % info->bytes_per_cluster;
    char *buffer = (char *)malloc(info->bytes_per_cluster);

    if (!cluster)
    {
        free(buffer);
        return -1;
    }

    for (i = 0; i < index; i++)
    {
        cluster = fat32_read_fat_entry(info, cluster);
    }

    if (position + count > node->file_size)
    {
        index = count = node->file_size - position;
    }
    else
    {
        index = count;
    }

    do
    {
        xmemset(buffer, 0, info->bytes_per_cluster);
        sector = info->Data_firstsector + (cluster - 2) * info->sector_per_cluster;
        if (device_read(info->dev, buffer, info->sector_per_cluster, sector, 0) != 0)
        {
            write_serial_string("FAT32 FS(read) read disk ERROR!!!!!!!!!!\n");
            retval = -1;
            break;
        }

        length = index <= info->bytes_per_cluster - offset ? index : info->bytes_per_cluster - offset;

        xmemcpy(buf, buffer + offset, length);

        index -= length;
        buf += length;
        offset -= offset;
        position += length;
    } while (index && (cluster = fat32_read_fat_entry(info, cluster)));

    free(buffer);
    if (!index)
        retval = count;

    return retval;
}

int fat_write(VNode *node, uint8_t *buf, size_t position, size_t count)
{
    FAT32_NodeInfo *info = (FAT32_NodeInfo *)node->desc;

    uint64_t cluster = info->first_cluster;
    uint64_t next_cluster = 0;
    uint64_t sector = 0;
    int i, length = 0;
    int retval = 0;
    long flags = 0;
    int index = position / info->bytes_per_cluster;
    long offset = position % info->bytes_per_cluster;
    char *buffer = (char *)malloc(info->bytes_per_cluster);

    if (!cluster)
    {
        cluster = fat32_find_available_cluster(info);
        flags = 1;
    }
    else
        for (i = 0; i < index; i++)
            cluster = fat32_read_fat_entry(info, cluster);

    if (!cluster)
    {
        free(buffer);
        return -1;
    }

    if (flags)
    {
        info->first_cluster = cluster;
        node->ops->write_node(node);
        fat32_write_fat_entry(info, cluster, 0x0ffffff8);
    }

    index = count;

    do
    {
        if (!flags)
        {
            xmemset(buffer, 0, info->bytes_per_cluster);
            sector = info->Data_firstsector + (cluster - 2) * info->sector_per_cluster;
            if (device_read(info->dev, buffer, info->sector_per_cluster, sector, 0) != 0)
            {
                write_serial_string("FAT32 FS(write) read disk ERROR!!!!!!!!!!\n");
                retval = -1;
                break;
            }
        }

        length = index <= info->bytes_per_cluster - offset ? index : info->bytes_per_cluster - offset;

        xmemcpy(buffer + offset, buf, length);

        if (device_write(info->dev, buffer, info->sector_per_cluster, sector, 0) != 0)
        {
            write_serial_string("FAT32 FS(write) write disk ERROR!!!!!!!!!!\n");
            retval = -1;
            break;
        }

        index -= length;
        buf += length;
        offset -= offset;
        position += length;

        if (index)
            next_cluster = fat32_read_fat_entry(info, cluster);
        else
            break;

        if (next_cluster >= 0x0ffffff8)
        {
            next_cluster = fat32_find_available_cluster(info);
            if (!next_cluster)
            {
                free(buffer);
                return -1;
            }

            fat32_write_fat_entry(info, cluster, next_cluster);
            fat32_write_fat_entry(info, next_cluster, 0x0ffffff8);
            cluster = next_cluster;
            flags = 1;
        }

    } while (index);

    if (position > node->file_size)
    {
        node->file_size = position;
        node->ops->write_node(node);
    }

    free(buffer);
    if (!index)
        retval = count;
    return retval;
}

int fat_lseek(VNode *node, uint64_t offset, uint64_t origin)
{
    long pos = 0;

    switch (origin)
    {
    case SEEK_SET:
        pos = offset;
        break;

    case SEEK_CUR:
        pos = node->position + offset;
        break;

    case SEEK_END:
        pos = node->file_size - offset;
        break;

    default:
        return -1;
        break;
    }

    if (pos < 0 || pos > node->file_size)
        return -1;

    node->position = pos;

    return pos;
}

VNodeOps fat32_ops = {
    .lookup = fat_lookup,
    .super = fat_super,
    .write_node = fat_write_node,

    .open = fat_open,
    .close = fat_close,
    .resize = fat_resize,
    .read = fat_read,
    .write = fat_write,
    .lseek = fat_lseek,
};

void init_fat()
{
    register_fs("FAT32", &fat32_ops);
}
