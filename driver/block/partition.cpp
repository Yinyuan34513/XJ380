#include "../../include/include.h"

int partition_ioctl(void *dev, int cmd, void *args, int flags)
{
    partition_dev_t *part_dev = (partition_dev_t *)dev;

    return 0;
}

int partition_read(void *dev, void *buf, size_t count, int idx, int flags)
{
    partition_dev_t *part_dev = (partition_dev_t *)dev;
    uint64_t new_idx = part_dev->entry->starting_lba + idx;
    uint64_t max_count = part_dev->entry->ending_lba - part_dev->entry->starting_lba + 1;

    if (count > max_count)
    {
        return -1;
    }

    return device_read(part_dev->parent_dev, buf, count, new_idx, flags);
}

int partition_write(void *dev, void *buf, size_t count, int idx, int flags)
{
    partition_dev_t *part_dev = (partition_dev_t *)dev;
    uint64_t new_idx = part_dev->entry->starting_lba + idx;
    uint64_t max_count = part_dev->entry->ending_lba - part_dev->entry->starting_lba + 1;

    if (count > max_count)
    {
        return -1;
    }

    return device_write(part_dev->parent_dev, buf, count, new_idx, flags);
}

void parse_disk(int dev)
{
    uint8_t *buffer = (uint8_t *)malloc(512);
    xmemset(buffer, 0, 512);
    device_read(dev, buffer, 1, 1, 0);

    GptPartitionTableHeader *header = (GptPartitionTableHeader *)buffer;
    if (!part_strcmp(header->signature, GPT_HEADER_SIGNATURE, 8))
    {
        write_serial_string("Found GPT partition table header at device ");
        write_serial_dec(dev);
        write_serial_string("\n");

        uint64_t partition_entry_lba = header->partition_entry_lba;
        uint64_t num_partition_entries = header->num_partition_entries / header->size_of_partition_entry;
        write_serial_string("partition_entry_lba = ");
        write_serial_dec(partition_entry_lba);
        write_serial_string(" num_partition_entries = ");
        write_serial_dec(num_partition_entries);
        write_serial_string("\n");
        xmemset(buffer, 0, 512);

        device_read(dev, buffer, 1, partition_entry_lba, 0);

        GPTPartitionEntry *entry = (GPTPartitionEntry *)buffer;

        // 目前只支持在同一个扇区的结构体
        for (int i = 0; i < num_partition_entries; i++)
        {
            GPTPartitionEntry *ent = entry + i;
            write_serial_string("Partition ");
            write_serial_dec(i);
            write_serial_string(" starting_lba = ");
            write_serial_dec(ent->starting_lba);
            write_serial_string("\n");

            partition_dev_t *part_dev = (partition_dev_t *)malloc(sizeof(partition_dev_t));
            xmemset(part_dev, 0, sizeof(partition_dev_t));
            part_dev->parent_dev = dev;
            part_dev->entry = (GPTPartitionEntry *)malloc(sizeof(GPTPartitionEntry));
            xmemcpy(part_dev->entry, ent, sizeof(GPTPartitionEntry));

            device_install(DEV_BLOCK, DEV_PART, part_dev, "NO NAME", dev, (void *)partition_ioctl, (void *)partition_read, (void *)partition_write);
        }
    }

    free(buffer);
}

void init_partition()
{
    for (int i = 0; i < MAX_SUPPORTED_BLOCK_DEV_NUM; i++)
    {
        device_t *device = device_find(DEV_DISK, i);
        if (!device)
            continue;

        parse_disk(device->dev);
    }
}
