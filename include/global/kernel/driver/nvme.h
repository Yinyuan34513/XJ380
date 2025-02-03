#pragma once

#define QUEUE_LENGTH 64
#include "../../../include.h"

#define NVME_REGS32_VS 0x08
#define NVME_REGS32_INTMS 0x0C
#define NVME_REGS32_INTMC 0x10
#define NVME_REGS32_CC 0x14
#define NVME_REGS32_CSTS 0x1C
#define NVME_REGS32_NSSR 0x18
#define NVME_REGS32_AQA 0x24
#define NVME_REGS64_ASQ 0x28
#define NVME_REGS32_CMBLOC 0x38
#define NVME_REGS32_CMBSZ 0x3C
#define NVME_REGS32_BPINFO 0x40
#define NVME_REGS32_BPRSEL 0x44
#define NVME_REGS32_BPMBL 0x48
#define NVME_REGS32_CMBSTS 0x58
#define NVME_REGS32_PMRCAP 0xE00
#define NVME_REGS32_PMRCTL 0xE04
#define NVME_REGS32_PMRSTS 0xE08
#define NVME_REGS32_PMREBS 0xE0C
#define NVME_REGS32_PMRSWTP 0xE10

#define NVME_REGS64_CAP 0x0
#define NVME_REGS64_ASQ 0x28
#define NVME_REGS64_ACQ 0x30
#define NVME_REGS64_CMBMSC 0x50
#define NVME_PMRMSC 0xE14

typedef struct
{
    uint64_t nsize;
    uint64_t ncap;
    uint64_t nuse;
    uint8_t nsfeat;
    uint8_t nlbaf;
    uint8_t flbas;
    uint8_t mc;
    uint8_t dpc;
    uint8_t dps;
    uint8_t nmic;
    uint8_t rescap;
    uint8_t fpi;
    uint8_t dlfeat;
    uint16_t nawun;
    uint16_t nawupf;
    uint16_t nacwu;
    uint16_t nabsn;
    uint16_t nabo;
    uint16_t nabspf;
    uint16_t noiob;
    uint64_t nvmcap[2];
    uint16_t npwg;
    uint16_t npwa;
    uint16_t npdg;
    uint16_t npda;
    uint16_t nows;
    uint8_t _rsvd1[18];
    uint32_t anagrpid;
    uint8_t _rsvd2[3];
    uint8_t nsattr;
    uint16_t nvmsetid;
    uint16_t endgid;
    uint8_t nguid[16];
    uint64_t eui64;
    uint32_t lba_format_support[16];
    uint8_t _rsvd3[192];
    uint8_t vendor_specific[3712];
} __attribute__((packed)) IdentifyNamespaceData;

typedef struct
{
    /// Opcode
    uint8_t opcode;
    /// Flags; FUSE (2 bits) | Reserved (4 bits) | PSDT (2 bits)
    uint8_t flags;
    /// Command ID
    uint16_t c_id;
    /// Namespace ID
    uint32_t ns_id;
    /// Reserved
    uint64_t _rsvd;
    /// Metadata pointer
    uint64_t md_ptr;
    /// Data pointer
    uint64_t d_ptr[2];
    /// Command dword 10
    uint32_t cdw10;
    /// Command dword 11
    uint32_t cdw11;
    /// Command dword 12
    uint32_t cdw12;
    /// Command dword 13
    uint32_t cdw13;
    /// Command dword 14
    uint32_t cdw14;
    /// Command dword 15
    uint32_t cdw15;
} NvmeCommand;

typedef struct
{
    NvmeCommand *commands[QUEUE_LENGTH];
    uint64_t head;
    uint64_t tail;
    uint64_t len;
    uint64_t doorbell;
} NvmeSubQueue;

typedef struct
{

} NvmeCompQueue;

typedef struct
{
    NvmeCommand *commands[QUEUE_LENGTH];
    uint64_t head;
    bool phase;
    uint64_t len;
    uint64_t doorbell;
} NvmeQueuePair;

typedef struct
{
    uint32_t id;
    uint64_t blocks;
    uint64_t block_size;
} NvmeNamespace;

typedef struct
{
    uint64_t completions;
    uint64_t submissions;
} NvmeStats;

class NVME
{
public:
    NVME();
    ~NVME();

    void init();

private:
    uint32_t get_regs32(uint64_t offset);
    void set_regs32(uint64_t offset, uint32_t value);

    uint64_t get_regs64(uint64_t offset);
    void set_regs64(uint64_t offset, uint64_t value);

    uint64_t bar_addr;

    uint16_t dstrd;
    NvmeSubQueue admin_sq;
    NvmeCompQueue admin_cq;
    NvmeSubQueue io_sq;
    NvmeCompQueue io_cq;

    uint64_t **buffer;
    uint64_t *prp_list[512];
    // hashmap namespaces;
    NvmeStats stats;
    uint16_t q_id;
};

extern NVME NVME_MANAGER;
