#include <proto.hpp>
#include <hda/hda.h>
#include <pci/pci.h>

// HDA寄存器偏移
#define HDA_GCAP         0x00  // 全局能力
#define HDA_VMIN         0x02  // 版本号
#define HDA_GCTL         0x08  // 全局控制
#define HDA_WAKEEN       0x0C  // 唤醒使能
#define HDA_STATESTS     0x0E  // 状态状态
#define HDA_CORB_BASE    0x40  // CORB基地址
#define HDA_RIRB_BASE    0x50  // RIRB基地址
#define HDA_CORB_WP      0x4A  // CORB写指针
#define HDA_CORB_RP      0x4C  // CORB读指针
#define HDA_RIRB_WP      0x5A  // RIRB写指针
#define HDA_RIRB_RP      0x5C  // RIRB读指针

// HDA命令格式
#define HDA_CMD(codec, node, verb, payload) \
    (((codec) << 28) | ((node) << 20) | ((verb) << 8) | (payload))

// 常用verbs
#define HDA_VERB_PARAMETER   0xF00
#define HDA_VERB_GET_CONN_LIST 0xF02
#define HDA_VERB_SET_POWER    0x705

hda_controller_t* hda_controller = NULL;

static uint32_t hda_read32(hda_controller_t* ctrl, uint16_t offset) {
    return *(volatile uint32_t*)((uintptr_t)ctrl->regs_base + offset);
}

static void hda_write32(hda_controller_t* ctrl, uint16_t offset, uint32_t value) {
    *(volatile uint32_t*)((uintptr_t)ctrl->regs_base + offset) = value;
}

static uint16_t hda_read16(hda_controller_t* ctrl, uint16_t offset) {
    return *(volatile uint16_t*)((uintptr_t)ctrl->regs_base + offset);
}

static void hda_write16(hda_controller_t* ctrl, uint16_t offset, uint16_t value) {
    *(volatile uint16_t*)((uintptr_t)ctrl->regs_base + offset) = value;
}

static uint8_t hda_read8(hda_controller_t* ctrl, uint16_t offset) {
    return *(volatile uint8_t*)((uintptr_t)ctrl->regs_base + offset);
}

static void hda_write8(hda_controller_t* ctrl, uint16_t offset, uint8_t value) {
    *(volatile uint8_t*)((uintptr_t)ctrl->regs_base + offset) = value;
}

// 发送HDA命令
static int hda_send_command(hda_controller_t* ctrl, uint32_t command) {
    // 等待CORB有空间
    uint16_t wp = hda_read8(ctrl, HDA_CORB_WP);
    uint16_t rp = hda_read8(ctrl, HDA_CORB_RP);
    
    if (((wp + 1) % ctrl->corb_entries) == rp) {
        return -1; // CORB满了
    }
    
    // 写入命令
    ctrl->corb_buffer[wp] = command;
    wp = (wp + 1) % ctrl->corb_entries;
    hda_write8(ctrl, HDA_CORB_WP, wp);
    
    return 0;
}

// 初始化CORB (Command Outbound Ring Buffer)
static int hda_init_corb(hda_controller_t* ctrl) {
    uint16_t corb_size = hda_read8(ctrl, HDA_CORB_BASE) & 0x3;
    
    switch (corb_size) {
        case 0: ctrl->corb_entries = 2; break;
        case 1: ctrl->corb_entries = 16; break;
        case 2: ctrl->corb_entries = 256; break;
        default: return -1;
    }
    
    // 分配CORB缓冲区
    ctrl->corb_buffer = (uint32_t*)malloc(ctrl->corb_entries * sizeof(uint32_t));
    if (!ctrl->corb_buffer) return -1;
    
    // 设置CORB基地址
    uint64_t phys_addr = (uint64_t)virt_to_phys((uint64_t)ctrl->corb_buffer);
    hda_write32(ctrl, HDA_CORB_BASE + 4, (uint32_t)(phys_addr >> 32));
    hda_write32(ctrl, HDA_CORB_BASE, (uint32_t)phys_addr);
    
    // 启用CORB
    uint8_t corb_ctl = hda_read8(ctrl, HDA_CORB_BASE + 2);
    corb_ctl |= 0x2; // 启用CORB
    hda_write8(ctrl, HDA_CORB_BASE + 2, corb_ctl);
    
    return 0;
}

// 重置HDA控制器
static void hda_reset(hda_controller_t* ctrl) {
    // 设置复位位
    uint32_t gctl = hda_read32(ctrl, HDA_GCTL);
    gctl &= ~0x1; // 清除CRST
    hda_write32(ctrl, HDA_GCTL, gctl);
    
    // 等待复位完成
    for (int i = 0; i < 1000; i++) {
        if (!(hda_read32(ctrl, HDA_GCTL) & 0x1)) break;
    }
    
    // 解除复位
    gctl |= 0x1;
    hda_write32(ctrl, HDA_GCTL, gctl);
    
    // 等待控制器就绪
    for (int i = 0; i < 1000; i++) {
        if (hda_read32(ctrl, HDA_GCTL) & 0x1) break;
    }
}

// 探测编解码器
static void hda_probe_codecs(hda_controller_t* ctrl) {
    uint16_t statests = hda_read16(ctrl, HDA_STATESTS);
    
    for (int i = 0; i < 15; i++) {
        if (statests & (1 << i)) {
            write_serial_fmt("HDA: Found codec #%d\n", i);
        }
    }
}

// 初始化HDA控制器
int hda_init() {
    // 查找HDA控制器 (Class 0x04, Subclass 0x03)
    pci_device_t* hda_device = pci_find_class(0x040300);
    if (!hda_device) {
        write_serial_string("HDA: No HDA controller found\n");
        return -1;
    }
    
    write_serial_fmt("HDA: Found controller at %x:%x.%x\n", 
                    hda_device->bus, hda_device->slot, hda_device->func);
    
    // 分配控制器结构
    hda_controller = (hda_controller_t*)malloc(sizeof(hda_controller_t));
    if (!hda_controller) return -1;
    
    memset(hda_controller, 0, sizeof(hda_controller_t));
    
    // 找到BAR0 (内存映射寄存器)
    for (int i = 0; i < 6; i++) {
        if (hda_device->bars[i].mmio && hda_device->bars[i].size > 0) {
            hda_controller->regs_base = (void*)hda_device->bars[i].address;
            write_serial_fmt("HDA: BAR%d at 0x%lx, size 0x%lx\n", 
                           i, hda_device->bars[i].address, hda_device->bars[i].size);
            break;
        }
    }
    
    if (!hda_controller->regs_base) {
        write_serial_string("HDA: No valid BAR found\n");
        free(hda_controller);
        return -1;
    }
    
    // 启用总线主控和内存空间
    uint32_t pci_command = hda_device->op->read(hda_device->bus, hda_device->slot, 
                                               hda_device->func, hda_device->segment, 0x04);
    pci_command |= 0x6; // 总线主控 + 内存空间
    hda_device->op->write(hda_device->bus, hda_device->slot, hda_device->func,
                         hda_device->segment, 0x04, pci_command);
    
    // 重置控制器
    hda_reset(hda_controller);
    
    // 读取能力
    uint16_t gcap = hda_read16(hda_controller, HDA_GCAP);
    write_serial_fmt("HDA: GCAP = 0x%x (OSS: %d, ISS: %d, BSS: %d)\n",
                    gcap, (gcap >> 12) & 0xF, (gcap >> 8) & 0xF, (gcap >> 3) & 0x1F);
    
    // 初始化CORB
    if (hda_init_corb(hda_controller) < 0) {
        write_serial_string("HDA: Failed to initialize CORB\n");
        free(hda_controller->corb_buffer);
        free(hda_controller);
        return -1;
    }
    
    // 探测编解码器
    hda_probe_codecs(hda_controller);
    
    write_serial_string("HDA: Controller initialized successfully\n");
    return 0;
}