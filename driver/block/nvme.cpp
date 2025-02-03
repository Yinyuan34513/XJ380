#include "../../include/include.h"

NVME NVME_MANAGER;

NVME::NVME()
{
}

NVME::~NVME()
{
}

uint32_t NVME::get_regs32(uint64_t offset)
{
    return *(uint32_t *)(this->bar_addr + offset);
}

void NVME::set_regs32(uint64_t offset, uint32_t value)
{
    *(uint32_t *)(this->bar_addr + offset) = value;
}

uint64_t NVME::get_regs64(uint64_t offset)
{
    return *(uint64_t *)(this->bar_addr + offset);
}

void NVME::set_regs64(uint64_t offset, uint64_t value)
{
    *(uint64_t *)(this->bar_addr + offset) = value;
}

void NVME::init()
{
    pci_device_t *device = pci_find_class(0x010802);
    if (device == NULL)
    {
        write_serial_string("Cannot find NVME Controller.\n");
        return;
    }
    else
        write_serial_string("Find NVME Controller.\n");

    if (!device->bars[0].mmio)
    {
        write_serial_string("NVME bar 0 is not mmio.\n");
        return;
    }
    this->bar_addr = convert_physical_to_virtual(device->bars[0].address);
    KERNEL_MAPPER mapper(PAGE_TABLE_BASE_ADDR);
    mapper.map_page(this->bar_addr, device->bars[0].address, 0x3);

    uint32_t version = this->get_regs32(NVME_REGS32_VS);
    uint8_t vs_h = (version >> 16) & 0xFFFF;
    uint8_t vs_m = (version >> 8) & 0xFF;
    uint8_t vs_l = (version >> 0) & 0xFF;
    write_serial_string("NVME Version: [");
    write_serial_dec(vs_h);
    write_serial_string(":");
    write_serial_dec(vs_m);
    write_serial_string(":");
    write_serial_dec(vs_l);
    write_serial_string("]\n");
}