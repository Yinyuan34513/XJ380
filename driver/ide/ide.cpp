#include <ide/ide.h>
#include <proto.hpp>
#include <pci/pci.h>

static ide_device_t ide_devices[4]; // 最多 4 个 IDE 设备

// 等待 IDE 就绪
static void ide_wait_ready(uint16_t base) {
    while (inb(base + 7) & IDE_STATUS_BSY);
}

// 等待数据就绪
static int ide_wait_drq(uint16_t base) {
    int timeout = 100000;
    while (timeout--) {
        uint8_t status = inb(base + 7);
        if (status & IDE_STATUS_DRQ) return 0;
        if (status & (IDE_STATUS_DF | IDE_STATUS_ERR)) return -1;
    }
    return -1;
}

// 检测 IDE 设备
int ide_detect_device(uint8_t port, uint8_t device) {
    uint16_t base = (port == 0) ? IDE_PRIMARY_DATA_PORT : IDE_SECONDARY_DATA_PORT;
    uint8_t drive_select = (device == 0) ? IDE_DEVICE_MASTER : IDE_DEVICE_SLAVE;
    
    // 选择设备
    outb(base + 6, drive_select);
    
    // 等待
    for (int i = 0; i < 4; i++) inb(base + 7);
    
    // 检查是否存在
    uint8_t status = inb(base + 7);
    if (status == 0xFF) return -1;
    
    // 发送 IDENTIFY 命令
    outb(base + 6, drive_select);
    for (int i = 0; i < 4; i++) inb(base + 7);
    
    outb(base + 2, 0);  // 扇区计数
    outb(base + 3, 0);  // LBA 低
    outb(base + 4, 0);  // LBA 中
    outb(base + 5, 0);  // LBA 高
    outb(base + 7, IDE_CMD_IDENTIFY);
    
    // 读取状态
    status = inb(base + 7);
    if (status == 0) return -1;
    
    // 等待
    ide_wait_ready(base);
    
    // 检查错误
    if (inb(base + 7) & IDE_STATUS_ERR) return -1;
    
    // 等待 DRQ
    if (ide_wait_drq(base) != 0) return -1;
    
    // 读取 IDENTIFY 数据
    uint16_t data[256];
    for (int i = 0; i < 256; i++) {
        data[i] = inw(base);
    }
    
    // 解析设备信息
    int idx = port * 2 + device;
    ide_devices[idx].exists = 1;
    ide_devices[idx].is_master = (device == 0);
    ide_devices[idx].is_primary = (port == 0);
    ide_devices[idx].device_type = (data[0] & IDE_DEVICE_ATAPI) ? IDE_DEVICE_ATAPI : IDE_DEVICE_ATA;
    
    // 获取容量
    ide_devices[idx].sectors = (uint32_t)data[60] | ((uint32_t)data[61] << 16);
    ide_devices[idx].sector_size = 512;
    
    // 获取型号
    for (int i = 0; i < 40; i += 2) {
        ide_devices[idx].model[i] = (char)(data[27 + i/2] >> 8);
        ide_devices[idx].model[i+1] = (char)(data[27 + i/2] & 0xFF);
    }
    ide_devices[idx].model[40] = '\0';
    
    // 获取序列号
    for (int i = 0; i < 20; i += 2) {
        ide_devices[idx].serial[i] = (char)(data[10 + i/2] >> 8);
        ide_devices[idx].serial[i+1] = (char)(data[10 + i/2] & 0xFF);
    }
    ide_devices[idx].serial[20] = '\0';
    
    write_serial_fmt("IDE: Found device %d: %s, %d sectors\n", idx, ide_devices[idx].model, ide_devices[idx].sectors);
    
    return 0;
}

// IDE 读操作
int ide_read(int drive, uint8_t *buffer, size_t size, size_t lba) {
    if (drive < 0 || drive > 3 || !ide_devices[drive].exists) return -1;
    
    uint16_t base = ide_devices[drive].is_primary ? IDE_PRIMARY_DATA_PORT : IDE_SECONDARY_DATA_PORT;
    uint8_t drive_select = ide_devices[drive].is_master ? IDE_DEVICE_MASTER : IDE_DEVICE_SLAVE;
    
    // 等待就绪
    ide_wait_ready(base);
    
    // 选择设备和 LBA 模式
    outb(base + 6, drive_select | 0x40 | ((lba >> 24) & 0x0F));
    
    // 设置扇区数和 LBA 地址
    outb(base + 2, 1);  // 读取 1 个扇区
    outb(base + 3, lba & 0xFF);
    outb(base + 4, (lba >> 8) & 0xFF);
    outb(base + 5, (lba >> 16) & 0xFF);
    
    // 发送读命令
    outb(base + 7, IDE_CMD_READ_SECTORS);
    
    // 等待数据就绪
    if (ide_wait_drq(base) != 0) return -1;
    
    // 读取数据
    uint16_t *buf = (uint16_t *)buffer;
    for (int i = 0; i < 256; i++) {
        buf[i] = inw(base);
    }
    
    return 0;
}

// IDE 写操作
int ide_write(int drive, uint8_t *buffer, size_t size, size_t lba) {
    if (drive < 0 || drive > 3 || !ide_devices[drive].exists) return -1;
    
    uint16_t base = ide_devices[drive].is_primary ? IDE_PRIMARY_DATA_PORT : IDE_SECONDARY_DATA_PORT;
    uint8_t drive_select = ide_devices[drive].is_master ? IDE_DEVICE_MASTER : IDE_DEVICE_SLAVE;
    
    // 等待就绪
    ide_wait_ready(base);
    
    // 选择设备和 LBA 模式
    outb(base + 6, drive_select | 0x40 | ((lba >> 24) & 0x0F));
    
    // 设置扇区数和 LBA 地址
    outb(base + 2, 1);  // 写入 1 个扇区
    outb(base + 3, lba & 0xFF);
    outb(base + 4, (lba >> 8) & 0xFF);
    outb(base + 5, (lba >> 16) & 0xFF);
    
    // 发送写命令
    outb(base + 7, IDE_CMD_WRITE_SECTORS);
    
    // 等待就绪
    ide_wait_ready(base);
    
    // 写入数据
    uint16_t *buf = (uint16_t *)buffer;
    for (int i = 0; i < 256; i++) {
        outw(base, buf[i]);
    }
    
    // 等待完成
    ide_wait_ready(base);
    
    return 0;
}

// IDE 设备读取回调
static size_t ide_read_callback(int drive, uint8_t *buffer, size_t size, size_t lba) {
    if (ide_read(drive, buffer, size, lba) != 0) return 0;
    return size;
}

// IDE 设备写入回调
static size_t ide_write_callback(int drive, uint8_t *buffer, size_t size, size_t lba) {
    if (ide_write(drive, buffer, size, lba) != 0) return 0;
    return size;
}

// 等待 devfs 就绪
static void wait_devfs_ready(void) {
    // 自旋等待 devfs_root 被设置
    extern vfs_node_t devfs_root;
    while (devfs_root == NULL) {
        // 可以添加 pause 指令减少功耗
        __asm__ __volatile__("pause" ::: "memory");
    }
}

// IDE 初始化
void ide_setup(void) {
    write_serial_string("Initializing IDE controller...\n");
    
    // 查找 IDE 控制器
    pci_device_t *ide_controller = pci_find_class(0x01018a);
    if (ide_controller == NULL) {
        write_serial_string("IDE: No IDE controller found\n");
        return;
    }
    
    write_serial_string("IDE: Found IDE controller\n");
    
    // 初始化设备数组
    memset(ide_devices, 0, sizeof(ide_devices));
    
    // 初始化 drive_id 字段
    for (int i = 0; i < 4; i++) {
        ide_devices[i].drive_id = -1;
    }
    
    // 等待 devfs 就绪，而不是用延迟
    wait_devfs_ready();
    
    // 检测主通道主从设备
    write_serial_string("IDE: Detecting Primary Master...\n");
    if (ide_detect_device(0, 0) == 0) {
        write_serial_string("IDE: Primary Master detected\n");
    }
    
    write_serial_string("IDE: Detecting Primary Slave...\n");
    if (ide_detect_device(0, 1) == 0) {
        write_serial_string("IDE: Primary Slave detected\n");
    }
    
    // 检测从通道主从设备
    write_serial_string("IDE: Detecting Secondary Master...\n");
    if (ide_detect_device(1, 0) == 0) {
        write_serial_string("IDE: Secondary Master detected\n");
    }
    
    write_serial_string("IDE: Detecting Secondary Slave...\n");
    if (ide_detect_device(1, 1) == 0) {
        write_serial_string("IDE: Secondary Slave detected\n");
    }
    
    write_serial_string("IDE: Registering devices...\n");
    
    // 注册找到的设备
    for (int i = 0; i < 4; i++) {
        if (!ide_devices[i].exists) continue;
        
        char name_buf[20];
        sprintf(name_buf, "ide%d", i);
        
        device_t ide_dev;
        ide_dev.size = ide_devices[i].sectors * ide_devices[i].sector_size;
        ide_dev.sector_size = ide_devices[i].sector_size;
        ide_dev.type = DEVICE_BLOCK;
        ide_dev.read = ide_read_callback;
        ide_dev.write = ide_write_callback;
        ide_dev.flag = 1;
        ide_dev.ioctl = (ioctlf)empty;
        ide_dev.poll = (pollf)empty;
        ide_dev.map = (mapf)empty;
        ide_dev.read_vbuf = NULL;
        ide_dev.write_vbuf = NULL;
        ide_dev.max_size = __UINT64_MAX__;
        strcpy(ide_dev.drive_name, name_buf);
        ide_dev.path = NULL;  // 让 regist_device 分配路径
        
        write_serial_fmt("IDE: Registering %s with %d sectors\n", name_buf, ide_devices[i].sectors);
        
        int id = regist_device(name_buf, ide_dev);
        if (id >= 0) {
            ide_devices[i].drive_id = id;  // 保存设备 ID
            write_serial_fmt("ide%d registered with id %d\n", i, id);
        }
        
        write_serial_fmt("ide%d: blk_size=%d, blk=0..%d, %s\n", 
                        i, ide_devices[i].sector_size, 
                        ide_devices[i].sectors, 
                        ide_devices[i].model);
    }
    
    write_serial_string("IDE controller initialized.\n");
}
