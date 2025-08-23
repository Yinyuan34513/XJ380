#pragma once

#include <proto.h>
#include <krlibc.h>

#define MAX_BLKDEV_NUM 8

typedef struct blkdev
{
    char *name;
    void *ptr;
    uint64_t block_size;
    uint64_t size;
    uint64_t (*read)(void *data, uint64_t lba, void *buffer, uint64_t size);
    uint64_t (*write)(void *data, uint64_t lba, void *buffer, uint64_t size);
} blkdev_t;
static inline void *alloc_frames_bytes(uint64_t bytes)
{
    return phys_to_virt((uint64_t)(void *)alloc_frames((bytes + PAGE_SIZE - 1) / PAGE_SIZE));
}

static inline void free_frames_bytes(void *ptr, uint64_t bytes)
{
    free_frames((uint64_t)virt_to_phys((uint64_t)ptr), (bytes + PAGE_SIZE - 1) / PAGE_SIZE);
}
extern blkdev_t blk_devs[MAX_BLKDEV_NUM];
extern uint64_t blk_devnum;

void regist_blkdev(char *name, void *ptr, uint64_t block_size, uint64_t size, uint64_t (*read)(void *data, uint64_t lba, void *buffer, uint64_t size), uint64_t (*write)(void *data, uint64_t lba, void *buffer, uint64_t size));

enum
{
    IOCTL_GETBLKSIZE,
    IOCTL_GETSIZE,
};

uint64_t blkdev_ioctl(uint64_t drive, uint64_t cmd, uint64_t arg);

uint64_t blkdev_read(uint64_t drive, uint64_t offset, void *buf, uint64_t len);
uint64_t blkdev_write(uint64_t drive, uint64_t offset, const void *buf, uint64_t len);