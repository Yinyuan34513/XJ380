#include <ahci/ahci.h>
#include <pci/pci.h>
#include <proto.hpp>
#include <device.h>

void              *op_buffer;
static uint32_t    cdb_size[] = {SCSI_CDB12, SCSI_CDB16, 0, 0};
struct hba_device *hbaDevice[AHCI_MAX_DEVICES];

void achi_register_ops(struct hba_port *port) {
    if (!(port->device->flags & HBA_DEV_FATAPI)) {
        port->device->ops.submit = sata_submit;
    } else {
        port->device->ops.submit = scsi_submit;
    }
}

void ahci_parsestr(char *str, uint16_t *reg_start, int size_word) {
    int j = 0;
    for (int i = 0; i < size_word; i++, j += 2) {
        uint16_t reg = *(reg_start + i);
        str[j]       = (char)(reg >> 8);
        str[j + 1]   = (char)(reg & 0xff);
    }
    str[j - 1] = '\0';
}

void ahci_parse_dev_info(struct hba_device *dev_info, uint16_t *data) {
    dev_info->max_lba          = *((uint32_t *)(data + IDDEV_OFFMAXLBA));
    dev_info->block_size       = *((uint32_t *)(data + IDDEV_OFFLSECSIZE));
    dev_info->cbd_size         = cdb_size[(*data & 0x3)];
    dev_info->wwn              = *(uint64_t *)(data + IDDEV_OFFWWN);
    dev_info->block_per_sec    = 1 << (*(data + IDDEV_OFFLPP) & 0xf);
    dev_info->alignment_offset = *(data + IDDEV_OFFALIGN) & 0x3fff;
    dev_info->capabilities     = *((uint32_t *)(data + IDDEV_OFFCAPABILITIES));

    if (!dev_info->block_size) { dev_info->block_size = 512; }

    if ((*(data + IDDEV_OFFADDSUPPORT) & 0x8) && (*(data + IDDEV_OFFA48SUPPORT) & 0x400)) {
        dev_info->max_lba  = *((uint64_t *)(data + IDDEV_OFFMAXLBA_EXT));
        dev_info->flags   |= HBA_DEV_FEXTLBA;
    }

    ahci_parsestr(dev_info->serial_num, data + IDDEV_OFFSERIALNUM, 10);
    ahci_parsestr(dev_info->model, data + IDDEV_OFFMODELNUM, 20);
}

int __get_free_slot(struct hba_port *port) {
    hba_reg_t pxsact   = port->regs[HBA_RPxSACT];
    hba_reg_t pxci     = port->regs[HBA_RPxCI];
    hba_reg_t free_bmp = pxsact | pxci;
    uint32_t  i        = 0;
    for (; i <= port->hba->cmd_slots && (free_bmp & 0x1); i++, free_bmp >>= 1)
        ;
    return i | -(i > port->hba->cmd_slots);
}

int hba_prepare_cmd(struct hba_port *port, struct hba_cmdt **cmdt, struct hba_cmdh **cmdh) {
    int slot = __get_free_slot(port);

    // 构建命令头（Command Header）和命令表（Command Table）
    struct hba_cmdh *cmd_header = (struct hba_cmdh*)phys_to_virt((uint64_t)&port->cmdlst[slot]);
    memset(cmd_header, 0, sizeof(struct hba_cmdh));
    uint64_t         phys      = alloc_frames(1);
    struct hba_cmdt *cmd_table = (struct hba_cmdt *)driver_phys_to_virt(phys);
    page_map_to(get_current_directory(), (uint64_t)cmd_table, phys, KERNEL_PTE_FLAGS);

    memset(cmd_header, 0, sizeof(struct hba_cmdh));

    // 将命令表挂到命令头上
    cmd_header->cmd_table_base       = (uint32_t)(phys & 0xFFFFFFFF);
    cmd_header->cmd_table_base_upper = (uint32_t)(phys >> 32);
    cmd_header->options = HBA_CMDH_FIS_LEN(sizeof(struct sata_reg_fis)) | HBA_CMDH_CLR_BUSY;

    *cmdh = cmd_header;
    *cmdt = cmd_table;

    return slot;
}

void __hba_reset_port(hba_reg_t *port_reg) {
    // 根据：SATA-AHCI spec section 10.4.2 描述的端口重置流程
    port_reg[HBA_RPxCMD] &= ~HBA_PxCMD_ST;
    port_reg[HBA_RPxCMD] &= ~HBA_PxCMD_FRE;
    int cnt               = wait_until_expire(!(port_reg[HBA_RPxCMD] & HBA_PxCMD_CR), 500000);
    if (cnt) { return; }

    port_reg[HBA_RPxSCTL] = (port_reg[HBA_RPxSCTL] & ~0xf) | 1;
    wait_until_expire(true, 100000); // 等待至少一毫秒，差不多就行了
    port_reg[HBA_RPxSCTL] &= ~0xf;
}

int hba_bind_vbuf(struct hba_cmdh *cmdh, struct hba_cmdt *cmdt, struct vecbuf *vbuf) {
    size_t         i   = 0;
    struct vecbuf *pos = vbuf;

    do {
        cmdt->entries[i++] = (struct hba_prdte){
            .data_base  = (uint64_t)driver_virt_to_phys((uint64_t)pos->buf.buffer),
            .byte_count = pos->buf.size - 1};
        pos = list_entry(pos->components.next, struct vecbuf, components);
    } while (pos != vbuf);

    cmdh->prdt_len = i + 1;

    return 0;
}

int hba_bind_sbuf(struct hba_cmdh *cmdh, struct hba_cmdt *cmdt, void *buf, uint32_t len) {
    if (len > 0x400000UL) {
        write_serial_fmt("AHCI buffer too large\n");
        return -1;
    }

    uint64_t buf_phys = (uint64_t)driver_virt_to_phys((uint64_t)buf);
    if (buf_phys == 0) {
        write_serial_fmt("AHCI buffer not mapped\n");
        return -1;
    }

    cmdh->prdt_len                   = 1;
    cmdt->entries[0].data_base       = buf_phys;
    cmdt->entries[0].data_base_upper = (buf_phys >> 32);
    cmdt->entries[0].byte_count      = len - 1;

    return 0;
}

void sata_create_fis(struct sata_reg_fis *cmd_fis, uint8_t command, uint64_t lba,
                     uint16_t sector_count) {
    memset(cmd_fis, 0, sizeof(struct sata_reg_fis));

    cmd_fis->head.type       = SATA_REG_FIS_H2D;
    cmd_fis->head.options    = SATA_REG_FIS_COMMAND;
    cmd_fis->head.status_cmd = command;
    cmd_fis->dev             = 0;

    cmd_fis->lba0  = SATA_LBA_COMPONENT(lba, 0);
    cmd_fis->lba8  = SATA_LBA_COMPONENT(lba, 8);
    cmd_fis->lba16 = SATA_LBA_COMPONENT(lba, 16);
    cmd_fis->lba24 = SATA_LBA_COMPONENT(lba, 24);

    cmd_fis->lba32 = SATA_LBA_COMPONENT(lba, 32);
    cmd_fis->lba40 = SATA_LBA_COMPONENT(lba, 40);

    cmd_fis->count = sector_count;
}

int ahci_init_device(struct hba_port *port) {
    struct hba_cmdt *cmd_table;
    struct hba_cmdh *cmd_header;

    // 分配并映射IDENTIFY数据缓冲区
    uint16_t *data_in = (uint16_t *)alloc_frames(1);
    if (!data_in) {
        write_serial_fmt("AHCI: Failed to allocate buffer for IDENTIFY\n");
        return 0;
    }
    uint16_t *data = (uint16_t*)driver_phys_to_virt((uint64_t)data_in);
    page_map_to(get_current_directory(), (uint64_t)data, (uint64_t)data_in, KERNEL_PTE_FLAGS);
    memset(data, 0, 512); // 清零缓冲区

    // 准备命令结构
    int slot = hba_prepare_cmd(port, &cmd_table, &cmd_header);
    if (slot < 0) {
        write_serial_fmt("AHCI: No free command slot\n");
        free_frame((uint64_t)data_in);
        return 0;
    }
    
    if (hba_bind_sbuf(cmd_header, cmd_table, data, 512) != 0) {
        write_serial_fmt("AHCI: Failed to bind buffer\n");
        free_frame((uint64_t)data_in);
        free_frame((uint64_t)virt_to_phys((uint64_t)cmd_table));
        return 0;
    }

    // 分配并初始化设备结构
    struct sata_reg_fis *cmd_fis = NULL;
    uint32_t sig=0;
    port->device = (struct hba_device*)malloc(sizeof(struct hba_device));
    if (!port->device) {
        write_serial_fmt("AHCI: Out of memory for device struct\n");
        goto fail_cleanup;
    }
    memset(port->device, 0, sizeof(struct hba_device));
    port->device->port = port;
    port->device->hba = port->hba;

    cmd_fis = (struct sata_reg_fis *)(&cmd_table->command_fis);

    // 根据设备签名选择IDENTIFY命令
    sig = port->regs[HBA_RPxSIG];
    write_serial_fmt("SIG!!: %d\n",sig);
    if (sig == HBA_DEV_SIG_ATA) {
        // ATA 设备
        sata_create_fis(cmd_fis, ATA_IDENTIFY_DEVICE, 0, 0);
        write_serial_fmt("AHCI: ATA device detected, sending IDENTIFY\n");
    } else if (sig == HBA_DEV_SIG_ATAPI) {
        // ATAPI 设备
        port->device->flags |= HBA_DEV_FATAPI;
        sata_create_fis(cmd_fis, ATA_IDENTIFY_PAKCET_DEVICE, 0, 0);
        write_serial_fmt("AHCI: ATAPI device detected, sending IDENTIFY PACKET\n");
    } else {
        write_serial_fmt("AHCI: Unknown device signature: 0x%x\n", sig);
        goto fail_cleanup;
    }

    // 发送IDENTIFY命令
    if (!ahci_try_send(port, slot)) {
        write_serial_fmt("AHCI: IDENTIFY command failed\n");
        goto fail_cleanup;
    }

    // 解析IDENTIFY数据
    ahci_parse_dev_info(port->device, data);

    // 如果是ATAPI设备，需要获取容量信息
    if (port->device->flags & HBA_DEV_FATAPI) {
        write_serial_fmt("AHCI: Initializing ATAPI device\n");
        
        // 准备READ CAPACITY命令
        memset(cmd_table, 0, sizeof(struct hba_cmdt)); // 清空命令表
        sata_create_fis(cmd_fis, ATA_PACKET, 0, 0); // 设置PACKET FIS
        
        // 根据CDB大小创建SCSI命令
        if (port->device->cbd_size == SCSI_CDB12) {
            struct scsi_cdb12 *cdb12 = (struct scsi_cdb12 *)cmd_table->atapi_cmd;
            scsi_create_packet12(cdb12, SCSI_READ_CAPACITY_10, 0, 0);
        } else {
            struct scsi_cdb16 *cdb16 = (struct scsi_cdb16 *)cmd_table->atapi_cmd;
            scsi_create_packet16(cdb16, SCSI_READ_CAPACITY_16, 0, 0);
            cdb16->misc1 = 0x10; // service action for READ CAPACITY(16)
        }

        // 重新绑定数据缓冲区
        memset(data, 0, 512);
        if (hba_bind_sbuf(cmd_header, cmd_table, data, 512) != 0) {
            write_serial_fmt("AHCI: Failed to bind buffer for READ CAPACITY\n");
            goto fail_cleanup;
        }

        cmd_header->options |= HBA_CMDH_ATAPI;
        cmd_header->transferred_size = 0;

        // 发送READ CAPACITY命令
        if (!ahci_try_send(port, slot)) {
            write_serial_fmt("AHCI: READ CAPACITY command failed\n");
            goto fail_cleanup;
        }

        // 解析容量信息
        scsi_parse_capacity(port->device, (uint32_t *)data);
    }

    // 注册设备操作
    achi_register_ops(port);

    write_serial_fmt("AHCI: Device initialized: %s, blocks: %d, block_size: %d\n",
          port->device->model, port->device->max_lba, port->device->block_size);

    // 清理资源
    free_frame((uint64_t)data_in);
    free_frame((uint64_t)virt_to_phys((uint64_t)cmd_table));
    return 1;

fail_cleanup:
    if (port->device) {
        free(port->device);
        port->device = NULL;
    }
    free_frame((uint64_t)data_in);
    free_frame((uint64_t)virt_to_phys((uint64_t)cmd_table));
    return 0;
}

spin_t ahci_operate_lock = SPIN_INIT;

size_t ahci_read(int drive, uint8_t *buffer, size_t size, size_t lba) {
    spin_lock(&ahci_operate_lock);
    struct hba_device *dev = (struct hba_device *)hbaDevice[drive];
    struct blkio_req   req = {
          .buf = (uint64_t)buffer, .lba = lba, .len = size * dev->block_size, .flags = 0};
    dev->ops.submit(dev, &req);
    spin_unlock(&ahci_operate_lock);
    return size;
}

size_t ahci_write(int drive, uint8_t *buffer, size_t size, size_t lba) {
    spin_lock(&ahci_operate_lock);
    struct hba_device *dev = (struct hba_device *)hbaDevice[drive];
    struct blkio_req   req = {
          .buf = (uint64_t)buffer, .lba = lba, .len = size * dev->block_size, .flags = BLKIO_WRITE};
    dev->ops.submit(dev, &req);
    spin_unlock(&ahci_operate_lock);
    return size;
}

// 在 utils.cpp 中添加
void __hba_reset_port_13(hba_reg_t *port_reg, uint32_t major, uint32_t minor) {
    // 停止命令处理
    port_reg[HBA_RPxCMD] &= ~HBA_PxCMD_ST;
    
    // 停止FIS接收
    port_reg[HBA_RPxCMD] &= ~HBA_PxCMD_FRE;
    
    // 等待命令处理停止
    int timeout = 1000000;
    while ((port_reg[HBA_RPxCMD] & HBA_PxCMD_CR) && timeout--) {
        asm volatile("pause");
    }
    
    if (timeout <= 0) {
        write_serial_fmt("AHCI: Port reset timeout waiting for CR to clear\n");
        return;
    }
    
    // AHCI 1.3 推荐的复位流程
    if (major >= 1 && minor >= 3) {
        // 清除错误状态
        hba_clear_reg(port_reg[HBA_RPxSERR]);
        hba_clear_reg(port_reg[HBA_RPxSERR2]);
        
        // 发送COMRESET
        port_reg[HBA_RPxSCTL] = (port_reg[HBA_RPxSCTL] & ~0xF) | 1;
        wait_until_expire(true, 100000); // 等待1ms
        
        // 清除SControl
        port_reg[HBA_RPxSCTL] &= ~0xF;
        wait_until_expire(true, 1000000); // 等待10ms让设备恢复
    } else {
        // 旧版本复位流程
        port_reg[HBA_RPxSCTL] = (port_reg[HBA_RPxSCTL] & ~0xF) | 1;
        wait_until_expire(true, 100000);
        port_reg[HBA_RPxSCTL] &= ~0xF;
    }
}
// 修改 ahci_setup 函数，添加版本检测和1.3特定处理
void ahci_setup() {
    pci_device_t *device = pci_find_class(0x010601);
    if (device == NULL) return;
    
    // 读取PCI配置空间获取BAR5
    pci_bar_base_address bar5 = device->bars[5];
    
    if (bar5.address == 0 || bar5.size == 0) {
        write_serial_fmt("AHCI: BAR5 is not valid, skipping AHCI setup\n");
        return;
    }
    
    op_buffer = (void *)alloc_frames(0x400000UL / PAGE_SIZE);

    struct ahci_hba *hba = (struct ahci_hba*)malloc(sizeof(struct ahci_hba));
    memset(hba, 0, sizeof(struct ahci_hba));

    hba->base = (hba_reg_t *)phys_to_virt(bar5.address);
    
    // 检测AHCI版本
    hba->version = hba->base[HBA_RVER];
    uint32_t major = (hba->version >> 16) & 0xFF;
    uint32_t minor = (hba->version >> 8) & 0xFF;
    
    write_serial_fmt("AHCI Controller Version: %d.%d.%d\n", 
        major, minor, hba->version & 0xFF);

    // 启用AHCI模式
    hba->base[HBA_RGHC] |= HBA_RGHC_ACHI_ENABLE;
    
    // 等待AHCI就绪
    int timeout = 1000000;
    while ((hba->base[HBA_RGHC] & HBA_RGHC_ACHI_ENABLE) == 0 && timeout--) {
        asm volatile("pause");
    }
    
    if (timeout <= 0) {
        write_serial_fmt("AHCI: Failed to enable AHCI mode\n");
        free(hba);
        return;
    }

    hba_reg_t cap = hba->base[HBA_RCAP];
    hba_reg_t pmap = hba->base[HBA_RPI];

    // AHCI 1.3 使用不同的端口数计算方式
    if (major >= 1 && minor >= 3) {
        hba->ports_num = (cap & HBA_CAP_NP) ? ((cap & HBA_CAP_NP) >> 8) + 1 : 32;
    } else {
        hba->ports_num = (cap & 0x1f) + 1;  // 旧版本
    }
    
    hba->cmd_slots = ((cap >> 8) & 0x1f) + 1; // NCS + 1
    hba->ports_bmp = pmap;

    uint64_t clb_pa = 0, fis_pa = 0;

    for (size_t i = 0; i < 32; i++) {
        if (!(pmap & (1 << i))) continue;

        struct hba_port *port = (struct hba_port *)malloc(sizeof(struct hba_port));
        memset(port, 0, sizeof(struct hba_port));

        hba_reg_t *port_regs = (hba_reg_t *)(&hba->base[HBA_RPBASE + i * HBA_RPSIZE]);

        // 使用兼容1.3的端口重置
        __hba_reset_port_13(port_regs, major, minor);

        // 分配命令列表和FIS接收区域
        if (clb_pa == 0) {
            clb_pa = alloc_frames(1);
            memset((void *)phys_to_virt(clb_pa), 0, PAGE_SIZE);
        }
        if (fis_pa == 0) {
            fis_pa = alloc_frames(1);
            memset((void *)phys_to_virt(fis_pa), 0, PAGE_SIZE);
        }

        uint64_t addr = clb_pa + i * HBA_CLB_SIZE;
        port_regs[HBA_RPxCLB] = (uint32_t)(addr & 0xFFFFFFFF);
        port_regs[HBA_RPxCLB + 1] = (uint32_t)(addr >> 32);
        
        addr = fis_pa + i * HBA_FIS_SIZE;
        port_regs[HBA_RPxFB] = (uint32_t)(addr & 0xFFFFFFFF);
        port_regs[HBA_RPxFB + 1] = (uint32_t)(addr >> 32);

        port->regs = port_regs;
        port->ssts = port_regs[HBA_RPxSSTS];
        port->cmdlst = (struct hba_cmdh *)(clb_pa + i * HBA_CLB_SIZE);
        port->fis = (void *)(fis_pa + i * HBA_FIS_SIZE);
        port->hba = hba;

        port_regs[HBA_RPxCI] = 0;

        // AHCI 1.3 需要清除更多的错误寄存器
        hba_clear_reg(port_regs[HBA_RPxSERR]);
        if (major >= 1 && minor >= 3) {
            hba_clear_reg(port_regs[HBA_RPxSERR2]);
        }

        // 检查端口是否有设备连接
        if (!HBA_RPxSSTS_IF(port->ssts)) {
            free(port);
            continue;
        }

        hba->ports[i] = port;

        // 等待命令列表就绪
        wait_until(!(port_regs[HBA_RPxCMD] & HBA_PxCMD_CR));
        
        // 启用FIS接收和命令处理
        port_regs[HBA_RPxCMD] |= HBA_PxCMD_FRE;
        port_regs[HBA_RPxCMD] |= HBA_PxCMD_ST;

        if (!ahci_init_device(port)) {
            write_serial_fmt("AHCI device init failed for port %d\n", i);
            continue;
        }

        // 设备注册代码保持不变...
        struct hba_device *hbadev = port->device;
        char name_buf[20];
        sprintf(name_buf, "sata%d", i);
        device_t sata;
        sata.size = hbadev->max_lba * hbadev->block_size;
        sata.sector_size = hbadev->block_size;
        sata.type = DEVICE_BLOCK;
        sata.read = ahci_read;
        sata.write = ahci_write;
        sata.flag = 1;
        sata.ioctl = (ioctlf)empty;
        sata.poll = (pollf)empty;
        sata.map = (mapf)empty;
        sata.read_vbuf = NULL;
        sata.write_vbuf = NULL;
        sata.max_size   = __UINT64_MAX__;//随便读!
        strcpy(sata.drive_name, name_buf);
        int id = regist_device(NULL, sata);
        hbaDevice[id] = hbadev;
        write_serial_fmt("sata%d: blk_size=%d, blk=0..%d, %s\n", i, hbadev->block_size, hbadev->max_lba, hbadev->model);
    }
    
    write_serial_fmt("AHCI initialized with %d ports, version %d.%d.%d\n", 
        hba->ports_num, major, minor, hba->version & 0xFF);
}