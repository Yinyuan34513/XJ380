#include "../../../include.h"

#pragma once

#define PCI_MCFG_MAX_ENTRIES_LEN 1024
#define PCI_DEVICE_MAX 256

typedef struct
{
    ACPI_TABLE_HEADER Header;
    uint64_t Reserved;
} __attribute__((packed)) MCFG;

typedef struct
{
    uint64_t base_address;
    uint16_t pci_segment_group;
    uint8_t start_bus;
    uint8_t end_bus;
    uint32_t reserved;
} __attribute__((packed)) MCFG_ENTRY;

void mcfg_addr_to_entries(MCFG *mcfg, MCFG_ENTRY **entries, uint64_t *len);
uint64_t get_mmio_address(uint32_t pci_address, uint16_t offset);

uint32_t segment_bus_device_functon_to_pci_address(uint16_t segment, uint8_t bus, uint8_t device, uint8_t function);
uint32_t pci_read(uint32_t pci_address, uint16_t offset);
void pci_write(uint32_t pci_address, uint16_t offset, uint32_t value);

typedef struct
{
    uint64_t address;
    bool mmio;
} pci_bar_base_address;

typedef struct
{
    const char *name;
    uint32_t class_code;
    uint8_t header_type;

    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t segment;
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    pci_bar_base_address bars[6];

    uint32_t capability_point;
    uint64_t msix_mmio_vaddr;
    uint64_t msix_mmio_size;  // msix映射长度
    uint32_t msix_offset;     // msix表的offset
    uint16_t msix_table_size; // msix表的表项数量

    uint64_t device_controller;
} pci_device_t;

uint32_t pci_enumerate_capability_list(pci_device_t *pci_dev, uint32_t cap_type);

const char *pci_classname(uint32_t classcode);
pci_device_t *pci_find_vid_did(uint16_t vendor_id, uint16_t device_id); // 根据供应商ID和设备ID寻找PCI设备
pci_device_t *pci_find_class(uint32_t class_code);                      // 根据指定class_code寻找PCI设备 (找不到返回NULL)
void init_pci(uint64_t mcfg_addr);
