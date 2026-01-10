#include "device.h"
#include "errno.h"
#include "id_alloc.h"
#include "krlibc.h"
#include <proto.hpp>
#include <fs/partition.h>
#include <dlinker.h>

device_t        device_ctl[256];
id_allocator_t *dev_allocator;

int regist_device(const char *path, device_t vd) {
    int i = id_alloc(dev_allocator);
    if (i == -1) return -1;
    device_ctl[i]         = vd;
    device_ctl[i].vdiskid = i;
    // write_serial_fmt("path: %s id %d\n",path,i);
    errno_t ret;
    if ((ret = devfs_register(path, i)) != EOK) {
        write_serial_fmt("Registers (%s)device error: %d\n", vd.drive_name, ret);
    }else{
        write_serial_fmt("Registers (%s)device success\n", vd.drive_name);
    }
    return i;
}

EXPORT_SYMBOL(regist_device);

void delete_device(int vdiskid) {
    if (vdiskid >= 256) return;
    device_t dev = device_ctl[vdiskid];
    devfs_delete(dev.path);
    free(dev.path);
    id_free(dev_allocator, dev.vdiskid);
    dev.path    = NULL;
    dev.flag    = 0;
    dev.vdiskid = 0;
}

bool have_vdisk(int drive) {
    int indx = drive;
    if (indx >= 256) { return false; }
    return device_ctl[indx].flag > 0;
}

size_t disk_size(int drive) {
    uint8_t drive1 = drive;
    if (have_vdisk(drive1)) {
        int indx = drive1;
        return device_ctl[indx].size;
    }
    return 0;
}

void *device_mmap(int drive, void *addr, size_t len) {
    if (have_vdisk(drive)) {
        int indx = drive;
        if (device_ctl[indx].map) { return device_ctl[indx].map(drive, addr, len); }
        return NULL;
    }
    return NULL;
}

device_t *get_device(size_t id) {
    if (device_ctl[id].flag == 0) return NULL;
    return &device_ctl[id];
}
size_t rw_device(int drive, size_t lba, uint8_t *buffer, size_t number, int read) {
    int indx = drive;
    if (indx >= 256){ return 0;}
    if (device_ctl[indx].flag == 0){return 0;}

    size_t   sector_size = device_ctl[indx].sector_size;
    size_t   total_size  = number * sector_size;
    size_t   page_size   = (total_size / PAGE_SIZE) == 0 ? 1 : (total_size / PAGE_SIZE);
    uint64_t phys        = alloc_frames(page_size);
    page_map_range(get_current_directory(), (uint64_t)driver_phys_to_virt(phys), phys,
                   page_size * PAGE_SIZE, PTE_PRESENT | PTE_WRITEABLE);
    uint8_t *kbuf = (uint8_t*)driver_phys_to_virt(phys);
    size_t   ret  = 0;
    if (read) {
        // write_serial_fmt("DRIVE %d\n",drive);
        ret = device_ctl[indx].read(drive, kbuf, number, lba);
        memcpy(buffer, kbuf, total_size);
    } else {
        memcpy(kbuf, buffer, total_size);
        ret = device_ctl[indx].write(drive, kbuf, number, lba);
    }
    unmap_page_range(get_current_directory(), (uint64_t)kbuf, page_size * PAGE_SIZE);
    return ret;
}
size_t device_read(size_t lba, size_t number, void *buffer, int drive) {
    if (have_vdisk(drive)) {
        if (device_ctl[drive].type == DEVICE_STREAM) {
            return device_ctl[drive].read(drive, (uint8_t*)buffer, number, lba);
        }
        size_t ret_size = 0;
        for (size_t i = 0; i < number; i += SECTORS_ONCE) {
            int sectors  = ((number - i) >= SECTORS_ONCE) ? SECTORS_ONCE : (number - i);
            ret_size    += rw_device(drive, lba + i,
                                     (uint8_t *)((uint64_t)buffer + i * device_ctl[drive].sector_size),
                                     sectors, 1);
        }
        return ret_size;
    }
    return 0;
}
size_t device_write(size_t lba, size_t number, const void *buffer, int drive) {
    if (have_vdisk(drive)) {
        size_t ret_size = 0;
        for (size_t i = 0; i < number; i += SECTORS_ONCE) {
            int sectors  = ((number - i) >= SECTORS_ONCE) ? SECTORS_ONCE : (number - i);
            ret_size    += rw_device(drive, lba + i,
                                     (uint8_t *)((uint64_t)buffer + i * device_ctl[drive].sector_size),
                                     sectors, 0);
        }
        return ret_size;
    }
    return 0;
}

size_t blk_device_read(int drive, void *buffer, size_t offset, size_t length) {
    device_t device;
    if (have_vdisk(drive))
    {
        device = device_ctl[drive];
    }
    uint64_t start_sector    = offset / device.sector_size;
    uint64_t end_sector      = (offset + length - 1) / device.sector_size;
    uint64_t sector_count    = end_sector - start_sector + 1;
    uint64_t offset_in_block = offset % device.sector_size;

    size_t   total_size = sector_count * device.sector_size;
    size_t   page_size  = (total_size / PAGE_SIZE) == 0 ? 1 : (total_size / PAGE_SIZE);
    uint64_t phys       = alloc_frames(page_size);
    page_map_range(get_current_directory(), (uint64_t)driver_phys_to_virt(phys), phys,
                   page_size * PAGE_SIZE, KERNEL_PTE_FLAGS);
    uint8_t *kbuf = (uint8_t*)driver_phys_to_virt(phys);

    if ((offset_in_block == 0) && ((length % device.sector_size) == 0)) {
        uint64_t total_copied      = 0;
        uint64_t remaining_sectors = sector_count;
        while (remaining_sectors > 0) {
            uint64_t to_copy_sectors =
                MIN(remaining_sectors, device.max_size / device.sector_size);
            device.read(device.vdiskid, kbuf, to_copy_sectors,
                             start_sector + total_copied / device.sector_size);
            uint64_t to_copy_bytes = to_copy_sectors * device.sector_size;
            memcpy(((uint64_t*)buffer + total_copied), kbuf, to_copy_bytes);
            total_copied      += to_copy_bytes;
            remaining_sectors -= to_copy_sectors;
        }
        unmap_page_range(get_kernel_pagedir(), (uint64_t)kbuf, sector_count * device.sector_size);
        return total_copied;
    }

    uint64_t total_read = 0;
    uint64_t remaining  = length;
    uint8_t *dest       = (uint8_t *)buffer;

    while (remaining > 0) {
        // 计算本次操作的扇区数和长度
        uint64_t chunk_sectors = sector_count;
        uint64_t chunk_size    = remaining;

        // 限制单次I/O大小
        if (chunk_sectors * device.sector_size > device.max_size) {
            chunk_sectors = device.max_size / device.sector_size;
            chunk_size    = chunk_sectors * device.sector_size - offset_in_block;
            if (chunk_size > remaining) { chunk_size = remaining; }
        }

        // 执行块设备读取
        if (device.read(device.vdiskid, kbuf, chunk_sectors, start_sector) != chunk_sectors) {
            write_serial_fmt("Read block device failed!!!\n");
            unmap_page_range(get_kernel_pagedir(), (uint64_t)kbuf,
                             sector_count * device.sector_size);
            return (uint64_t)-1;
        }

        // 复制数据到目标缓冲区
        uint64_t copy_size = (chunk_size > remaining) ? remaining : chunk_size;

        memcpy(dest, kbuf + offset_in_block, copy_size);

        // 更新状态
        dest            += copy_size;
        remaining       -= copy_size;
        total_read      += copy_size;
        start_sector    += chunk_sectors;
        offset_in_block  = 0; // 第一次之后不需要再处理块内偏移
    }

    unmap_page_range(get_kernel_pagedir(), (uint64_t)kbuf, sector_count * device.sector_size);

    return total_read;
}

size_t blk_device_write(device_t device, const void *buffer, size_t offset, size_t length) {

    uint64_t start_sector    = offset / device.sector_size;
    uint64_t end_sector      = (offset + length - 1) / device.sector_size;
    uint64_t sector_count    = end_sector - start_sector + 1;
    uint64_t offset_in_block = offset % device.sector_size;

    size_t   total_size = sector_count * device.sector_size;
    size_t   page_size  = (total_size / PAGE_SIZE) == 0 ? 1 : (total_size / PAGE_SIZE);
    uint64_t phys       = alloc_frames(page_size);
    page_map_range(get_current_directory(), (uint64_t)driver_phys_to_virt(phys), phys,
                   page_size * PAGE_SIZE, KERNEL_PTE_FLAGS);
    uint8_t *tmp = (uint8_t*)driver_phys_to_virt(phys);

    if ((offset_in_block == 0) && ((length % device.sector_size) == 0)) {
        uint64_t total_copied      = 0;
        uint64_t remaining_sectors = sector_count;
        while (remaining_sectors > 0) {
            uint64_t to_copy_sectors =
                MIN(remaining_sectors, device.max_size / device.sector_size);
            uint64_t to_copy_bytes = to_copy_sectors * device.sector_size;
            memcpy(tmp, ((uint64_t*)buffer + total_copied), to_copy_bytes);
            uint64_t ret       = device.write(device.vdiskid, tmp,
                                                   start_sector + total_copied / device.sector_size,
                                                   to_copy_sectors);
            total_copied      += to_copy_bytes;
            remaining_sectors -= to_copy_sectors;
        }
        unmap_page_range(get_kernel_pagedir(), (uint64_t)tmp, sector_count * device.sector_size);
        return total_copied;
    }

    uint64_t       total_written = 0;
    uint64_t       remaining     = length;
    const uint8_t *src           = (const uint8_t *)buffer;

    while (remaining > 0) {
        // 计算本次操作的扇区数和长度
        uint64_t chunk_sectors = sector_count;
        uint64_t chunk_size    = remaining;

        // 限制单次I/O大小
        if (chunk_sectors * device.sector_size > device.max_size) {
            chunk_sectors = device.max_size / device.sector_size;
            chunk_size    = chunk_sectors * device.sector_size - offset_in_block;
            if (chunk_size > remaining) { chunk_size = remaining; }
        }

        // 对于部分块写入，需要先读取原始数据
        if (offset_in_block != 0 || chunk_size < chunk_sectors * device.sector_size) {
            if (device.read(device.vdiskid, tmp, chunk_sectors, start_sector) !=
                chunk_sectors) {
                write_serial_fmt("Read block device failed!!!\n");
                unmap_page_range(get_kernel_pagedir(), (uint64_t)tmp,
                                 sector_count * device.sector_size);
                return (uint64_t)-1;
            }
        }

        // 复制数据到临时缓冲区
        uint64_t copy_size = (chunk_size > remaining) ? remaining : chunk_size;

        memcpy(tmp + offset_in_block, src, copy_size);

        // 执行块设备写入
        if (device.write(device.vdiskid, tmp, chunk_sectors, start_sector) != chunk_sectors) {
            write_serial_fmt("Write block device failed!!!\n");
            unmap_page_range(get_kernel_pagedir(), (uint64_t)tmp,
                             sector_count * device.sector_size);
            return (uint64_t)-1;
        }

        // 更新状态
        src             += copy_size;
        remaining       -= copy_size;
        total_written   += copy_size;
        start_sector    += chunk_sectors;
        offset_in_block  = 0; // 第一次之后不需要再处理块内偏移
    }

    unmap_page_range(get_kernel_pagedir(), (uint64_t)tmp, sector_count * device.sector_size);

    return total_written;
}


int device_manager_init() {
    for (size_t i = 0; i < 256; i++) {
        device_ctl[i].flag = 0; // 设置为未使用
    }
    dev_allocator = id_allocator_create(256);
    return 0;
}
