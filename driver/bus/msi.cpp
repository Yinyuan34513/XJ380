#include "../../include/include.h"

/**
 * @brief 生成架构相关的msi的message address
 *
 */
#define ia64_pci_get_arch_msi_message_address(processor) (0xfee00000UL | (processor << 12))

/**
 * @brief 生成架构相关的message data
 *
 */
#define ia64_pci_get_arch_msi_message_data(vector, processor, edge_trigger, assert) ((uint32_t)((vector & 0xff) | (edge_trigger == 1 ? 0 : (1 << 15)) | ((assert == 0) ? 0 : (1 << 14))))

/**
 * @brief 生成msi消息
 *
 * @param msi_desc msi描述符
 * @return struct msi_msg_t* msi消息指针（在描述符内）
 */
struct msi_msg_t *msi_arch_get_msg(struct msi_desc_t *msi_desc)
{
    msi_desc->msg.address_hi = 0;
    msi_desc->msg.address_lo = ia64_pci_get_arch_msi_message_address(msi_desc->processor);
    msi_desc->msg.data = ia64_pci_get_arch_msi_message_data(msi_desc->irq_num, msi_desc->processor, msi_desc->edge_trigger, msi_desc->assert);
    msi_desc->msg.vector_control = 0;
    return &(msi_desc->msg);
}

static inline struct pci_msi_cap_t __msi_read_cap_list(struct msi_desc_t *msi_desc, uint32_t cap_off)
{
    struct pci_msi_cap_t cap_list = {0};
    uint32_t dw0;
    uint32_t pci_address = segment_bus_device_functon_to_pci_address(msi_desc->pci_dev->segment, msi_desc->pci_dev->bus, msi_desc->pci_dev->slot, msi_desc->pci_dev->func);
    dw0 = pci_read(pci_address, cap_off);
    cap_list.cap_id = dw0 & 0xff;
    cap_list.next_off = (dw0 >> 8) & 0xff;
    cap_list.msg_ctrl = (dw0 >> 16) & 0xffff;

    cap_list.msg_addr_lo =
        pci_read(pci_address, cap_off + 0x4);
    uint16_t msg_data_off = 0xc;
    if (cap_list.msg_ctrl & (1 << 7)) // 64位
    {
        cap_list.msg_addr_hi =
            pci_read(pci_address, cap_off + 0x8);
    }
    else
    {
        cap_list.msg_addr_hi = 0;
        msg_data_off = 0x8;
    }

    cap_list.msg_data = pci_read(pci_address,
                                 cap_off + msg_data_off) &
                        0xffff;

    cap_list.mask =
        pci_read(pci_address, cap_off + 0x10);
    cap_list.pending =
        pci_read(pci_address, cap_off + 0x14);

    return cap_list;
}

/**
 * @brief 读取msix的capability list
 *
 * @param msi_desc msi描述符
 * @param cap_off capability list的offset
 * @return struct pci_msix_cap_t 对应的capability list
 */
static inline struct pci_msix_cap_t __msi_read_msix_cap_list(struct msi_desc_t *msi_desc, uint32_t cap_off)
{
    struct pci_msix_cap_t cap_list = {0};
    uint32_t pci_address = segment_bus_device_functon_to_pci_address(msi_desc->pci_dev->segment, msi_desc->pci_dev->bus, msi_desc->pci_dev->slot, msi_desc->pci_dev->func);
    uint32_t dw0;
    dw0 = pci_read(pci_address, cap_off);
    cap_list.cap_id = dw0 & 0xff;
    cap_list.next_off = (dw0 >> 8) & 0xff;
    cap_list.msg_ctrl = (dw0 >> 16) & 0xffff;

    cap_list.dword1 =
        pci_read(pci_address, cap_off + 0x4);
    cap_list.dword2 =
        pci_read(pci_address, cap_off + 0x8);
    return cap_list;
}

/**
 * @brief 映射设备的msix表
 *
 * @param pci_dev pci设备信息结构体
 * @param msix_cap msix capability list的结构体
 * @return int 错误码
 */
static inline int __msix_map_table(pci_device_t *pci_dev,
                                   struct pci_msix_cap_t *msix_cap)
{
    uint32_t pci_address = segment_bus_device_functon_to_pci_address(pci_dev->segment, pci_dev->bus, pci_dev->slot, pci_dev->func);
    // 计算bar寄存器的offset
    uint32_t bar_off = 0x10 + 4 * (msix_cap->dword1 & 0x7);

    // msix table相对于bar寄存器中存储的地址的offset
    pci_dev->msix_offset = msix_cap->dword1 & (~0x7);
    pci_dev->msix_table_size = (msix_cap->msg_ctrl & 0x7ff) + 1;
    pci_dev->msix_mmio_size = pci_dev->msix_table_size * 16 + pci_dev->msix_offset;

    uint64_t physical_address = convert_virtual_to_physical(pci_dev->msix_mmio_vaddr);
    KERNEL_MAPPER mapper((uint64_t *)convert_physical_to_virtual((uint64_t)current_task->mm->pgd));
    mapper.map_range(pci_dev->msix_mmio_vaddr, physical_address, pci_dev->msix_mmio_size, 0x3);
    flush_tlb();

    return 0;
}

/**
 * @brief 将msi_desc中的数据填写到msix表的指定表项处
 *
 * @param pci_dev pci设备结构体
 * @param msi_desc msi描述符
 */
static inline void __msix_set_entry(struct msi_desc_t *msi_desc)
{
    uint64_t *ptr =
        (uint64_t *)(msi_desc->pci_dev->msix_mmio_vaddr + msi_desc->pci_dev->msix_offset + msi_desc->msi_index * 16);
    *ptr = ((uint64_t)(msi_desc->msg.address_hi) << 32) | (msi_desc->msg.address_lo);
    ++ptr;
    *ptr = ((uint64_t)(msi_desc->msg.data) << 32) | (msi_desc->msg.vector_control);
}

/**
 * @brief 清空设备的msix table的指定表项
 *
 * @param pci_dev pci设备
 * @param msi_index 表项号
 */
static inline void __msix_clear_entry(pci_device_t *pci_dev, uint16_t msi_index)
{
    uint64_t *ptr = (uint64_t *)(pci_dev->msix_mmio_vaddr + pci_dev->msix_offset + msi_index * 16);
    *ptr = 0;
    ++ptr;
    *ptr = 0;
}

/**
 * @brief 启用 Message Signaled Interrupts
 *
 * @param header 设备header
 * @param vector 中断向量号
 * @param processor 要投递到的处理器
 * @param edge_trigger 是否边缘触发
 * @param assert 是否高电平触发
 *
 * @return 返回码
 */
int pci_enable_msi(struct msi_desc_t *msi_desc)
{
    pci_device_t *ptr = msi_desc->pci_dev;
    uint32_t pci_address = segment_bus_device_functon_to_pci_address(ptr->segment, ptr->bus, ptr->slot, ptr->func);
    uint32_t cap_ptr;
    uint32_t tmp;
    uint16_t message_control;
    uint64_t message_addr;

    if (msi_desc->pci.msi_attribute.is_msix)
    {
        cap_ptr = pci_enumerate_capability_list(ptr, 0x11);
        if (cap_ptr == 0)
        {
            cap_ptr = pci_enumerate_capability_list(ptr, 0x05);
            if (cap_ptr == 0)
                return -ENOSYS;
            msi_desc->pci.msi_attribute.is_msix = 0;
        }
    }
    else
    {
        cap_ptr = pci_enumerate_capability_list(ptr, 0x05);
        if (cap_ptr == 0)
            return -ENOSYS;
        msi_desc->pci.msi_attribute.is_msix = 0;
    }
    // 获取msi消息
    msi_arch_get_msg(msi_desc);

    // disable intx
    tmp = pci_read(pci_address, 0x04); // 读取cap+0x0处的值
    tmp &= ~(1U << 10);
    pci_write(pci_address, 0x04, tmp);

    if (msi_desc->pci.msi_attribute.is_msix) // MSI-X
    {
        write_serial_string("is msix\n");

        uint64_t table_offset = pci_read(pci_address, cap_ptr + 0x04);
        uint64_t bir = table_offset & 0x7;
        if (bir > 5)
        {
            return -EINVAL;
        }
        uint32_t table_offset_in_bar = table_offset & 0xFFFFFFF8;

        uint64_t msix_table_physical_address = ptr->bars[bir].address + table_offset_in_bar;
        uint64_t msix_mmio_vaddr = convert_physical_to_virtual(msix_table_physical_address);

        ptr->msix_mmio_vaddr = msix_mmio_vaddr;

        // 读取msix的信息
        struct pci_msix_cap_t cap = __msi_read_msix_cap_list(msi_desc, cap_ptr);
        // 映射msix table
        __msix_map_table(ptr, &cap);
        // 设置msix的中断
        __msix_set_entry(msi_desc);

        // 使能msi-x
        tmp = pci_read(pci_address, cap_ptr + 0x2); // 读取cap+0x2处的值
        tmp |= (1U << 15);
        pci_write(pci_address, cap_ptr + 0x2, tmp);
    }
    else
    {
        write_serial_string("is msi\n");

        tmp = pci_read(pci_address, cap_ptr); // 读取cap+0x0处的值
        message_control = (tmp >> 16) & 0xffff;

        // 写入message address
        message_addr = ((((uint64_t)msi_desc->msg.address_hi) << 32) | msi_desc->msg.address_lo); // 获取message address
        pci_write(pci_address, cap_ptr + 0x4, (uint32_t)(message_addr & 0xffffffff));

        if (message_control & (1 << 7)) // 64位
            pci_write(pci_address, cap_ptr + 0x8,
                      (uint32_t)((message_addr >> 32) & 0xffffffff));

        // 写入message data

        tmp = msi_desc->msg.data;
        if (message_control & (1 << 7)) // 64位
            pci_write(pci_address, cap_ptr + 0xc, tmp);
        else
            pci_write(pci_address, cap_ptr + 0x8, tmp);

        // 使能msi
        tmp = pci_read(pci_address, cap_ptr + 0x2); // 读取cap+0x2处的值
        tmp |= 1;
        tmp &= ~(7 << 4);
        pci_write(pci_address, cap_ptr + 0x2, tmp);
    }

    return 0;
}

/**
 * @brief 禁用指定设备的msi
 *
 * @param header pci header
 * @return int
 */
int pci_disable_msi(void *header)
{
    pci_device_t *ptr = (pci_device_t *)header;
    uint32_t pci_address = segment_bus_device_functon_to_pci_address(ptr->segment, ptr->bus, ptr->slot, ptr->func);
    uint32_t cap_ptr;
    uint32_t tmp;

    switch (ptr->header_type)
    {
    case 0x00: // general device
        cap_ptr = ptr->capability_point;

        tmp = pci_read(pci_address, cap_ptr); // 读取cap+0x0处的值

        if (tmp & 0xff != 0x5)
            return -ENOSYS;

        // 禁用msi
        tmp = pci_read(pci_address, cap_ptr); // 读取cap+0x0处的值
        tmp &= (~(1 << 16));
        pci_write(pci_address, cap_ptr, tmp);

        break;

    case 0x01: // pci to pci bridge
        cap_ptr = ptr->capability_point;

        tmp = pci_read(pci_address, cap_ptr); // 读取cap+0x0处的值

        if (tmp & 0xff != 0x5)
            return -ENOSYS;

        // 禁用msi
        tmp = pci_read(pci_address, cap_ptr); // 读取cap+0x0处的值
        tmp &= (~(1 << 16));
        pci_write(pci_address, cap_ptr, tmp);

        break;
    case 0x02: // pci to card bus bridge
        return -ENOSYS;
        break;

    default: // 不应该到达这里
        return -EINVAL;
        break;
    }

    return 0;
}