#include "../../include/include.h"
#include "../../kernel/alloc/page.h"

#define AHCI_GHC_RST (0x00000001) /* reset controller; self-clear */
#define AHCI_GHC_IE (0x00000002)  /* global IRQ enable */
#define AHCI_GHC_AE (0x80000000)  /* AHCI enabled */

#define AHCI_PORT_CMD_ST 0x00000001  // Start
#define AHCI_PORT_CMD_SUD 0x00000002 // Spin-Up Device
#define AHCI_PORT_CMD_POD 0x00000004 // Power On Device
#define AHCI_PORT_CMD_CLO 0x00000008 // Command List Override
#define AHCI_PORT_CMD_FRE 0x00000010 // FIS Receive Enable
#define AHCI_PORT_CMD_FR 0x00004000  // FIS Receive Running
#define AHCI_PORT_CMD_CR 0x00008000  // Command List Running

#define AHCI_PORT_IPM_ACTIVE 1
#define AHCI_PORT_DET_PRESENT 3

#define AHCI_PORT_DEV_BUSY 0x80
#define AHCI_PORT_DEV_DRQ 0x08

#define AHCI_PORT_IST_TFES 0x40000000 // Task File Error Status

#define AHCI_PORT_SIG_ATA 0x00000101   // SATA drive
#define AHCI_PORT_SIG_ATAPI 0xEB140101 // SATAPI drive
#define AHCI_PORT_SIG_SEMB 0xC33C0101  // Enclosure management bridge
#define AHCI_PORT_SIG_PM 0x96690101    // Port multiplier

#define ATA_CMD_READ_DMA_EX 0x25
#define ATA_CMD_WRITE_DMA_EXT 0x35
#define ATA_CMD_IDENTIFY_DEVICE 0xEC

void ReverseWord(void *p, uint64_t cnt)
{
    uint8_t(*buffer)
        [2] = (uint8_t(*)[2])p;
    for (uint32_t i = 0; i < cnt; i++)
    {
        uint8_t c = buffer[i][0];
        buffer[i][0] = buffer[i][1];
        buffer[i][1] = c;
    }
}

void AHCICommandRST(AHCI_HBA_PORT *port)
{
    // Clear ST
    port->CMD &= ~AHCI_PORT_CMD_ST;
    // Clear FRE
    port->CMD &= ~AHCI_PORT_CMD_FRE;
    // Wait FR and CR clear
    while (port->CMD & (AHCI_PORT_CMD_FR | AHCI_PORT_CMD_CR))
        __asm__ __volatile__("pause");
}

uint32_t AHCICommandEN(AHCI_HBA_PORT *port)
{
    AHCICommandRST(port);

    uint32_t cmd = port->CMD;
    cmd |= AHCI_PORT_CMD_FRE;
    cmd |= AHCI_PORT_CMD_SUD;
    cmd |= AHCI_PORT_CMD_ST;
    port->CMD = cmd;
    // Wait for LINK UP
    while (1)
    {
        if ((port->SAS & 7) == 3)
            return 0;
    }
    write_serial_string("AHCI PORT LINK DOWN\n");
    return -1;
}

uint32_t AHCICommandWR(uint32_t cmd)
{
    switch (cmd)
    {
    case ATA_CMD_WRITE_DMA_EXT:
        return 1;
    }
    return 0;
}

uint32_t OperationATA(AHCI_HBA_PORT *port, uint64_t lba, uint16_t count, uint32_t cmd, void *buf)
{
    if (!count)
        return 0;
    if (count > 128)
        count = 128;
    if (cmd == ATA_CMD_IDENTIFY_DEVICE)
        count = 1;

    port->IST = -1;
    int slot = 0; // SearchCMD

    AHCI_COMMAND_HEAD *cmdh = (AHCI_COMMAND_HEAD *)convert_physical_to_virtual(port->CLB);
    cmdh += slot;
    cmdh->CFL = 5; // sizeof(FIS_REG_H2D) / sizeof(uint32_t)
    cmdh->WRT = AHCICommandWR(cmd);
    cmdh->DTL = ((count - 1) >> 4) + 1; // UPPER BOUND (count / 16)

    AHCI_COMMAND_TABLE *tbl = (AHCI_COMMAND_TABLE *)convert_physical_to_virtual((cmdh->TBL));
    xmemset(tbl, 0, sizeof(AHCI_COMMAND_TABLE) + (cmdh->DTL * sizeof(AHCI_PRDT_ENTRY)));

    uint64_t bufAddr = convert_virtual_to_physical((uint64_t)buf);
    uint16_t count1 = count;
    uint32_t i = 0;
    while (count1)
    {
        uint32_t cnt = (count1 < 16) ? count1 : 16;
        tbl->PRD[i].DBA = bufAddr;
        tbl->PRD[i].DBC = (cnt << 9) - 1;
        tbl->PRD[i].IOC = 0; // Wrong ?
        count1 -= cnt;
        bufAddr += ((uint64_t)cnt) << 9;
        i++;
    }

    AHCI_FIS_H2D *fis = &tbl->FIS;
    xmemset(fis, 0, sizeof(AHCI_FIS_H2D));

    fis->TYP = 0x27; // H2D
    fis->CCC = 1;    // Command
    fis->CMD = cmd;
    fis->FTL = 1;

    fis->BA0 = (lba >> 0x00) & 0xFF;
    fis->BA1 = (lba >> 0x08) & 0xFF;
    fis->BA2 = (lba >> 0x10) & 0xFF;
    fis->DVC = 1 << 6; // LBA MODE
    fis->BA3 = (lba >> 0x18) & 0xFF;
    fis->BA4 = (lba >> 0x20) & 0xFF;
    fis->BA5 = (lba >> 0x28) & 0xFF;

    fis->CNT = count;

    port->SAE = port->SAE;
    // The below loop waits until the prot is no longer busy before issuing a new command
    while ((port->TFD & (AHCI_PORT_DEV_DRQ | AHCI_PORT_DEV_BUSY)))
    {
        __asm__ __volatile__("pause");
    }
    port->CIS = 1 << slot;

    while ((port->CIS & (1 << slot)) && !(port->IST & AHCI_PORT_IST_TFES))
        ;
    if (port->IST & AHCI_PORT_IST_TFES)
    {
        return 2 << 16;
    }

    return count;
}

uint32_t OperationAHCI(bool write, void *dev, uint64_t idx, uint64_t count, void *buf)
{
    disable_intr();

    AHCI_PORT *port = (AHCI_PORT *)dev;
    uint32_t cc = 0;
    {
        uint32_t cmd = 0;

        // DISK_CMD cmd to ATA CMD
        if (write)
            cmd = ATA_CMD_WRITE_DMA_EXT;
        else
            cmd = ATA_CMD_READ_DMA_EX;

        uint32_t isWrite = AHCICommandWR(cmd);
        uint32_t blocks = OperationATA(port->PRT, idx, count, cmd, buf);
        if (blocks >> 16)
        {
            count = 0;
            cc = (uint32_t)-EIO;
            goto AHCI_OVER;
        }
        count = blocks;
    }
AHCI_OVER:;
    enable_intr();

    return count;
}

int ahci_read(void *dev, void *buf, size_t count, int idx, int flags)
{
    return OperationAHCI(false, dev, idx, count, buf) ? 0 : -EIO;
}

int ahci_write(void *dev, void *buf, size_t count, int idx, int flags)
{
    return OperationAHCI(true, dev, idx, count, buf) ? 0 : -EIO;
}

void init_ahci()
{
    pci_device_t *device = pci_find_class(0x010601);
    if (!device)
    {
        write_serial_string("Cannot found Serial ATA device.\n");
        return;
    }

    uint64_t iobase = convert_physical_to_virtual(device->bars[5].address);

    AHCI_CONTROLLER *ctrl = (AHCI_CONTROLLER *)malloc(sizeof(AHCI_CONTROLLER));
    xmemset(ctrl, 0, sizeof(AHCI_CONTROLLER));

    ctrl->HBA = (AHCI_HBA_MEMORY *)iobase;

    ctrl->HBA->GHC |= AHCI_GHC_AE;
    uint32_t pi = ctrl->HBA->PTI;
    uint32_t pn = 0;
    while (pi)
    {
        if (pi & 1)
        {
            AHCI_HBA_PORT *port = &(ctrl->HBA->PRT[pn]);
            uint32_t sas = port->SAS;
            uint8_t ipm = (sas >> 8) & 0xF;
            uint8_t det = (sas >> 0) & 0xF;
            if (det != AHCI_PORT_DET_PRESENT || ipm != AHCI_PORT_IPM_ACTIVE)
                goto AHCI_NEXT_PORT;

            if (port->SIG == AHCI_PORT_SIG_ATA)
            {
                // Maybe not need to realloc
                if (AHCICommandEN(port))
                {
                    write_serial_string("CANNOT LINK UP PORT ");
                    write_serial_hex(port - ctrl->HBA->PRT);
                    write_serial_string("\n");
                    AHCICommandRST(port);
                    goto AHCI_NEXT_PORT;
                }
                uint8_t *buf = (uint8_t *)PAGE_ALLOC.alloc_page();
                buf = (uint8_t *)convert_physical_to_virtual((uint64_t)buf);
                xmemset(buf, 0, 4096);
                uint32_t cc = OperationATA(port, 0, 1, ATA_CMD_IDENTIFY_DEVICE, buf);
                if (cc >> 16)
                {
                    write_serial_string("CANNOT READ AHCI DISK ");
                    write_serial_hex(cc >> 16);
                    write_serial_string("\n");
                    while (1)
                        __asm__ __volatile__("pause");
                }

                AHCI_PORT *ap = (AHCI_PORT *)malloc(sizeof(AHCI_PORT));
                xmemset(ap, 0, sizeof(AHCI_PORT));
                ap->CTRL = ctrl;
                ap->PRT = port;

                /*
                PRINTRAX(port - ctrl->HBA->PRT, 2);
                OUTCHAR(':');
                OUTCHAR('[');
                */

                char *identify = (char *)(buf + 0x14);
                identify[0x14] = 0;
                ReverseWord(identify, 0x0A);
                identify = LeadingWhitespace(identify, identify + 0x14);
                xmemcpy(ap->SER, identify, strlen(identify));

                /*
                OUTPUTTEXT(ap->SER);
                OUTCHAR(']');
                OUTCHAR('[');
                */

                identify = (char *)(buf + 0x36);
                identify[0x28] = 0;
                ReverseWord(identify, 0x14);
                identify = LeadingWhitespace(identify, identify + 0x28);
                xmemcpy(ap->MOD, identify, strlen(identify));

                device_install(DEV_BLOCK, DEV_DISK, ap, "AHCI", 0, nullptr, (void *)ahci_read, (void *)ahci_write);

                /*
                OUTPUTTEXT(ap->MOD);
                OUTCHAR(']');
                PRINTRAX(ap->DRV.SCT, 16);
                write_serial_string("\n");
                */
                // Try to read sector
                /*
                cc = OperationATA(port, 1, 1, ATA_CMD_READ_DMA_EX, buf);
                if (cc >> 16)
                {
                    OUTPUTTEXT("CANNOT READ AHCI DISK ");
                    PRINTRAX(cc >> 16, 4);
                    write_serial_string("\n");
                    while (1) __halt();
                }
                ((uint64_t *) buf)[1] = '\n';
                PRINTRAX(*((uint64_t *) buf), 16);
                write_serial_string("\n");

                memset(buf, 0, 4096);
                DISK_OPERATION dop;
                memset(&dop, 0, sizeof(DISK_OPERATION));
                dop.DRV = (DISK_DRIVER *) ap;
                dop.CMD = CMD_READ;
                dop.CNT = 8;
                dop.DAT = buf;
                dop.LBA = 0;
                ExecuteDiskOperation(&dop);
                PRINTRAX(*((uint64_t *) (buf + 512)), 16);
                write_serial_string("\n");
                */

                PAGE_ALLOC.free_page((void *)convert_virtual_to_physical((uint64_t)buf));
            }
        }
    AHCI_NEXT_PORT:;
        pi >>= 1;
        pn++;
    }
}