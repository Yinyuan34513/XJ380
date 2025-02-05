#pragma once

#include "../../../include.h"

#define MAX_SUPPORTED_BLOCK_DEV_NUM 8

#define GPT_HEADER_SIGNATURE "EFI PART"

typedef struct
{
    char signature[8];                    // 签名，必须是 "EFI PART"
    uint32_t revision;                    // 修订版本，通常为 0x00010000
    uint32_t header_size;                 // 头部大小，通常为 92 字节
    uint32_t header_crc32;                // 头部CRC32校验值
    uint32_t reserved;                    // 保留字段，必须为 0
    uint64_t my_lba;                      // 当前LBA（这个头部所在的LBA）
    uint64_t alternate_lba;               // 备份头部所在的LBA
    uint64_t first_usable_lba;            // 第一个可用的LBA（用于分区）
    uint64_t last_usable_lba;             // 最后一个可用的LBA（用于分区）
    uint8_t disk_guid[16];                // 磁盘的GUID
    uint64_t partition_entry_lba;         // 分区表项的起始LBA
    uint32_t num_partition_entries;       // 分区表项的数量
    uint32_t size_of_partition_entry;     // 每个分区表项的大小（通常为 128 字节）
    uint32_t partition_entry_array_crc32; // 分区表项的CRC32校验值
} __attribute__((packed)) GptPartitionTableHeader;

typedef struct
{
    uint8_t partition_type_guid[16];   // Partition type GUID
    uint8_t unique_partition_guid[16]; // Unique partition GUID
    uint64_t starting_lba;             // Starting LBA of the partition
    uint64_t ending_lba;               // Ending LBA of the partition
    uint64_t attributes;               // Partition attributes
    uint16_t partition_name[36];       // Partition name (UTF-16LE, null-terminated)
} GPTPartitionEntry;

typedef struct
{
    int parent_dev;
    GPTPartitionEntry *entry;
} partition_dev_t;

void init_partition();
