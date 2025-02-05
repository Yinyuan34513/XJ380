#include "../../include/include.h"
#include "../../kernel/alloc/page.h"

#define NVME_CSTS_FATAL (1U << 1)
#define NVME_CSTS_RDY (1U << 0)

#define NVME_SQE_OPC_ADMIN_CREATE_IO_SQ 1U
#define NVME_SQE_OPC_ADMIN_CREATE_IO_CQ 5U
#define NVME_SQE_OPC_ADMIN_IDENTIFY 6U

#define NVME_SQE_OPC_IO_WRITE 1U
#define NVME_SQE_OPC_IO_READ 2U

#define NVME_ADMIN_IDENTIFY_CNS_ID_NS 0x00U
#define NVME_ADMIN_IDENTIFY_CNS_ID_CTRL 0x01U
#define NVME_ADMIN_IDENTIFY_CNS_ACT_NSL 0x02U

void *NVME_DMA_MEMORY = 0;

char *LeadingWhitespace(char *beg, char *end)
{
    while (end > beg && *--end <= 0x20)
    {
        *end = 0;
    }
    while (beg < end && *beg <= 0x20)
    {
        beg++;
    }
    return beg;
}

void NVMEConfigureQ(NVME_CONTROLLER *ctrl, NVME_QUEUE_COMMON *q, uint32_t idx, uint32_t len)
{
    xmemset(q, 0, sizeof(NVME_QUEUE_COMMON));
    q->DBL = (uint32_t *)(((uint8_t *)ctrl->CAP) + 0x1000 + idx * ctrl->DST);
    q->MSK = len - 1;
}
int NVMEConfigureCQ(NVME_CONTROLLER *ctrl, NVME_COMPLETION_QUEUE *cq, uint32_t idx, uint32_t len)
{
    NVMEConfigureQ(ctrl, &cq->COM, idx, len);
    cq->CQE = 0;
    uint64_t phyAddr = 0;
    uint64_t pagCont = 1;
    phyAddr = (uint64_t)PAGE_ALLOC.alloc_pages(pagCont);
    cq->CQE = (NVME_COMPLETION_QUEUE_ENTRY *)convert_physical_to_virtual(phyAddr);
    xmemset(cq->CQE, 0, 4096);
    cq->COM.HAD = 0;
    cq->COM.TAL = 0;
    cq->COM.PHA = 1;
    return 0;
}
int NVMEConfigureSQ(NVME_CONTROLLER *ctrl, NVME_SUBMISSION_QUEUE *sq, uint32_t idx, uint32_t len)
{
    NVMEConfigureQ(ctrl, &sq->COM, idx, len);
    sq->SQE = 0;
    uint64_t phyAddr = 0;
    uint64_t pagCont = 1;
    phyAddr = (uint64_t)PAGE_ALLOC.alloc_pages(pagCont);
    sq->SQE = (NVME_SUBMISSION_QUEUE_ENTRY *)convert_physical_to_virtual(phyAddr);
    xmemset(sq->SQE, 0, 4096);
    sq->COM.HAD = 0;
    sq->COM.TAL = 0;
    sq->COM.PHA = 0;
    return 0;
}
int NVMEWaitingRDY(NVME_CONTROLLER *ctrl, uint32_t rdy)
{
    uint32_t csts;
    while (rdy != ((csts = ctrl->CAP->CST) & NVME_CSTS_RDY))
    {
        __asm__ __volatile__("pause");
    }
    return 0;
}
NVME_COMPLETION_QUEUE_ENTRY NVMEWaitingCMD(NVME_SUBMISSION_QUEUE *sq, NVME_SUBMISSION_QUEUE_ENTRY *e)
{
    NVME_COMPLETION_QUEUE_ENTRY errcqe;
    xmemset(&errcqe, 0xFF, sizeof(NVME_COMPLETION_QUEUE_ENTRY));

    if (((sq->COM.HAD + 1) % (sq->COM.MSK + 1ULL)) == sq->COM.TAL)
    {
        write_serial_string("SUBMISSION QUEUE IS FULL\n");
        return errcqe;
    }

    // Commit
    NVME_SUBMISSION_QUEUE_ENTRY *sqe = sq->SQE + sq->COM.TAL;
    xmemcpy(sqe, e, sizeof(NVME_SUBMISSION_QUEUE_ENTRY));
    sqe->CDW0 |= sq->COM.TAL << 16;

    // Doorbell
    sq->COM.TAL++;
    sq->COM.TAL %= (sq->COM.MSK + 1ULL);
    sq->COM.DBL[0] = sq->COM.TAL;

    // Check completion
    NVME_COMPLETION_QUEUE *cq = sq->ICQ;
    while ((cq->CQE[cq->COM.HAD].STS & 1) != cq->COM.PHA)
    {
        __asm__ __volatile__("pause");
    }

    // Consume CQE
    NVME_COMPLETION_QUEUE_ENTRY *cqe = cq->CQE + cq->COM.HAD;
    uint16_t cqNextHAD = (cq->COM.HAD + 1) % (cq->COM.MSK + 1ULL);
    if (cqNextHAD < cq->COM.HAD)
    {
        cq->COM.PHA ^= 1;
    }
    cq->COM.HAD = cqNextHAD;

    if (cqe->QHD != sq->COM.HAD)
    {
        sq->COM.HAD = cqe->QHD;
    }
    // Doorbell
    cq->COM.DBL[0] = cq->COM.HAD;
    return *cqe;
}

void nvme_rwfail(uint16_t status)
{
    write_serial_string("Status: ");
    write_serial_hex(status);
    write_serial_string("\nStatus code: ");
    write_serial_hex(status & 0xFF);
    write_serial_string("\nStatus code type: ");
    write_serial_hex((status >> 8) & 0x7);
    write_serial_string("\n");
}

uint32_t NVMETransfer(NVME_NAMESPACE *ns, void *buf, uint64_t lba, uint32_t count, uint32_t write)
{
    if (!count)
        return 0;

    uint64_t bufAddr = (uint64_t)buf;
    uint32_t maxCount = (4096 / ns->BSZ) - ((bufAddr & 0xFFF) / ns->BSZ);
    if (count > maxCount)
        count = maxCount;
    if (count > ns->MXRS)
        count = ns->MXRS;
    uint64_t size = count * ns->BSZ;

    NVME_SUBMISSION_QUEUE_ENTRY sqe;
    xmemset(&sqe, 0, sizeof(NVME_SUBMISSION_QUEUE_ENTRY));
    sqe.CDW0 = write ? NVME_SQE_OPC_IO_WRITE : NVME_SQE_OPC_IO_READ;
    sqe.META = 0;
    sqe.DATA[0] = convert_virtual_to_physical(bufAddr);
    sqe.DATA[1] = 0;
    sqe.NSID = ns->NSID;
    sqe.CDWA = lba;
    sqe.CDWB = lba >> 32;
    sqe.CDWC = (1UL << 31) | ((count - 1) & 0xFFFF);
    NVME_COMPLETION_QUEUE_ENTRY cqe = NVMEWaitingCMD(&ns->CTRL->ISQ, &sqe);
    if ((cqe.STS >> 1) & 0xFF)
    {
        if (write)
        {
            write_serial_string("NVME CANNOT WRITE\n");
        }
        else
            write_serial_string("NVME CANNOT READ\n");

        nvme_rwfail(cqe.STS);
        return -1;
    }
    return count ? 0 : -1;
}

void failed_nvme(NVME_CONTROLLER *ctrl)
{

    if (ctrl->ICQ.CQE)
    {
        PAGE_ALLOC.free_pages((void *)convert_virtual_to_physical((uint64_t)ctrl->ICQ.CQE), 1);
    }
    if (ctrl->ISQ.SQE)
    {
        PAGE_ALLOC.free_pages((void *)convert_virtual_to_physical((uint64_t)ctrl->ISQ.SQE), 1);
    }
    if (ctrl->ACQ.CQE)
    {
        PAGE_ALLOC.free_pages((void *)convert_virtual_to_physical((uint64_t)ctrl->ACQ.CQE), 1);
    }
    if (ctrl->ASQ.SQE)
    {
        PAGE_ALLOC.free_pages((void *)convert_virtual_to_physical((uint64_t)ctrl->ASQ.SQE), 1);
    }
}

void failed_namespace(NVME_IDENTIFY_NAMESPACE *identifyNS)
{
    PAGE_ALLOC.free_pages((void *)convert_virtual_to_physical((uint64_t)identifyNS), 1);
}

int NvmeRead(void *dev, void *buf, size_t count, int idx, int flags)
{
    return NVMETransfer((NVME_NAMESPACE *)dev, buf, idx, count, false);
}

int NvmeWrite(void *dev, void *buf, size_t count, int idx, int flags)
{
    return NVMETransfer((NVME_NAMESPACE *)dev, buf, idx, count, true);
}

void init_nvme()
{
    pci_device_t *device = pci_find_class(0x010802);
    if (!device)
    {
        return;
    }

    NVME_CAPABILITY *cap = (NVME_CAPABILITY *)convert_physical_to_virtual(device->bars[0].address);

    if (!cap)
    {
        write_serial_string("Cannot find NVME Controller.");
        return;
    }

    KERNEL_MAPPER mapper(PAGE_TABLE_BASE_ADDR);
    mapper.map_range((uint64_t)cap, device->bars[0].address, PAGE_SIZE * 4, 0x3);

    if (!((cap->CAP >> 37) & 1))
    {
        write_serial_string("NVME CONTROLLER DOES NOT SUPPORT NVME COMMAND SET\n");
        return;
    }

    NVME_CONTROLLER *ctrl = (NVME_CONTROLLER *)malloc(sizeof(NVME_CONTROLLER));
    xmemset(ctrl, 0, sizeof(NVME_CONTROLLER));
    ctrl->DVC = device;
    ctrl->CAP = cap;
    ctrl->WTO = 500 * ((cap->CAP >> 24) & 0xFF);

    // RST controller
    ctrl->CAP->CC = 0;
    if (NVMEWaitingRDY(ctrl, 0))
    {
        write_serial_string("NVME FATAL ERROR DURING CONTROLLER SHUTDOWN\n");
        failed_nvme(ctrl);
        return;
    }
    ctrl->DST = 4ULL << ((cap->CAP >> 32) & 0xF);

    int rc = NVMEConfigureCQ(ctrl, &ctrl->ACQ, 1, 4096 / sizeof(NVME_COMPLETION_QUEUE_ENTRY));
    if (rc)
    {
        failed_nvme(ctrl);
        return;
    }

    rc = NVMEConfigureSQ(ctrl, &ctrl->ASQ, 0, 4096 / sizeof(NVME_SUBMISSION_QUEUE_ENTRY));
    if (rc)
    {
        failed_nvme(ctrl);
        return;
    }
    ctrl->ASQ.ICQ = &ctrl->ACQ;

    ctrl->CAP->AQA = (ctrl->ACQ.COM.MSK << 16) | ctrl->ASQ.COM.MSK;
    ctrl->CAP->ASQ = convert_virtual_to_physical((uint64_t)ctrl->ASQ.SQE);
    ctrl->CAP->ACQ = convert_virtual_to_physical((uint64_t)ctrl->ACQ.CQE);

    ctrl->CAP->CC = 1 | (4 << 20) | (6 << 16);
    if (NVMEWaitingRDY(ctrl, 1))
    {
        write_serial_string("NVME FATAL ERROR DURING CONTROLLER ENABLING\n");
        failed_nvme(ctrl);
        return;
    }

    /* The admin queue is set up and the controller is ready. Let's figure out
       what namespaces we have. */
    // Identify Controller
    NVME_IDENTIFY_CONTROLLER *identify = 0;
    uint64_t pagCont = 1;
    identify = (NVME_IDENTIFY_CONTROLLER *)PAGE_ALLOC.alloc_pages(pagCont);
    identify = (NVME_IDENTIFY_CONTROLLER *)convert_physical_to_virtual((uint64_t)identify);
    xmemset(identify, 0, 4096);

    NVME_SUBMISSION_QUEUE_ENTRY sqe;
    xmemset(&sqe, 0, sizeof(NVME_SUBMISSION_QUEUE_ENTRY));
    sqe.CDW0 = NVME_SQE_OPC_ADMIN_IDENTIFY;
    sqe.META = 0;
    sqe.DATA[0] = convert_virtual_to_physical((uint64_t)identify);
    sqe.DATA[1] = 0;
    sqe.NSID = 0;
    sqe.CDWA = NVME_ADMIN_IDENTIFY_CNS_ID_CTRL;
    NVME_COMPLETION_QUEUE_ENTRY cqe = NVMEWaitingCMD(&ctrl->ASQ, &sqe);
    if ((cqe.STS >> 1) & 0xFF)
    {
        write_serial_string("CANNOT IDENTIFY NVME CONTROLLER\n");
        failed_nvme(ctrl);
        return;
    }

    char buf[41];
    xmemcpy(buf, identify->SERN, sizeof(identify->SERN));
    buf[sizeof(identify->SERN)] = 0;
    char *serialN = LeadingWhitespace(buf, buf + sizeof(identify->SERN));
    xmemcpy(ctrl->SER, serialN, strlen(serialN));
    // write_serial_string(serialN);
    // LINEFEED();
    xmemcpy(buf, identify->MODN, sizeof(identify->MODN));
    buf[sizeof(identify->MODN)] = 0;
    serialN = LeadingWhitespace(buf, buf + sizeof(identify->MODN));
    xmemcpy(ctrl->MOD, serialN, strlen(serialN));
    // write_serial_string(serialN);
    // LINEFEED();

    ctrl->NSC = identify->NNAM;
    uint8_t mdts = identify->MDTS;
    PAGE_ALLOC.free_pages((void *)convert_virtual_to_physical((uint64_t)identify), 1);

    if (ctrl->NSC == 0)
    {
        write_serial_string("NO NAMESPACE\n");
        failed_nvme(ctrl);
        return;
    }

    // Create I/O Queue
    // Create I/O CQ
    {
        uint32_t qidx = 3;
        uint32_t entryCount = 1 + (ctrl->CAP->CAP & 0xFFFF);
        if (entryCount > 4096 / sizeof(NVME_COMPLETION_QUEUE_ENTRY))
            entryCount = 4096 / sizeof(NVME_COMPLETION_QUEUE_ENTRY);
        if (NVMEConfigureCQ(ctrl, &ctrl->ICQ, qidx, entryCount))
        {
            write_serial_string("CANNOT INIT I/O CQ\n");
            failed_nvme(ctrl);
            return;
        }
        NVME_SUBMISSION_QUEUE_ENTRY ccq;
        xmemset(&ccq, 0, sizeof(NVME_SUBMISSION_QUEUE_ENTRY));
        ccq.CDW0 = NVME_SQE_OPC_ADMIN_CREATE_IO_CQ;
        ccq.META = 0;
        ccq.DATA[0] = convert_virtual_to_physical((uint64_t)ctrl->ICQ.CQE);
        ccq.DATA[1] = 0;
        ccq.CDWA = (ctrl->ICQ.COM.MSK << 16) | (qidx >> 1);
        ccq.CDWB = 1;

        cqe = NVMEWaitingCMD(&ctrl->ASQ, &ccq);
        if ((cqe.STS >> 1) & 0xFF)
        {
            write_serial_string("CANNOT CREATE I/O CQ\n");
            failed_nvme(ctrl);
            return;
        }
    }

    // Create I/O SQ
    {
        uint32_t qidx = 2;
        uint32_t entryCount = 1 + (ctrl->CAP->CAP & 0xFFFF);
        if (entryCount > 4096 / sizeof(NVME_SUBMISSION_QUEUE_ENTRY))
            entryCount = 4096 / sizeof(NVME_SUBMISSION_QUEUE_ENTRY);
        if (NVMEConfigureSQ(ctrl, &ctrl->ISQ, qidx, entryCount))
        {
            write_serial_string("CANNOT INIT I/O SQ\n");
            failed_nvme(ctrl);
            return;
        }
        NVME_SUBMISSION_QUEUE_ENTRY csq;
        xmemset(&csq, 0, sizeof(NVME_SUBMISSION_QUEUE_ENTRY));
        csq.CDW0 = NVME_SQE_OPC_ADMIN_CREATE_IO_SQ;
        csq.META = 0;
        csq.DATA[0] = convert_virtual_to_physical((uint64_t)ctrl->ISQ.SQE);
        csq.DATA[1] = 0;
        csq.CDWA = (ctrl->ICQ.COM.MSK << 16) | (qidx >> 1);
        csq.CDWB = ((qidx >> 1) << 16) | 1;

        cqe = NVMEWaitingCMD(&ctrl->ASQ, &csq);
        if ((cqe.STS >> 1) & 0xFF)
        {
            write_serial_string("CANNOT CREATE I/O SQ\n");
            failed_nvme(ctrl);
            return;
        }
        ctrl->ISQ.ICQ = &ctrl->ICQ;
    }

    /* Populate namespace IDs */
    for (uint32_t nsidx = 0; nsidx < ctrl->NSC; nsidx++)
    {
        // Probe Namespace
        uint32_t nsid = nsidx + 1;

        NVME_IDENTIFY_NAMESPACE *identifyNS = 0;
        pagCont = 1;
        identifyNS = (NVME_IDENTIFY_NAMESPACE *)PAGE_ALLOC.alloc_pages(pagCont);
        identifyNS = (NVME_IDENTIFY_NAMESPACE *)convert_physical_to_virtual((uint64_t)identifyNS);
        xmemset(identifyNS, 0, 4096);

        xmemset(&sqe, 0, sizeof(NVME_SUBMISSION_QUEUE_ENTRY));
        sqe.CDW0 = NVME_SQE_OPC_ADMIN_IDENTIFY;
        sqe.META = 0;
        sqe.DATA[0] = convert_virtual_to_physical((uint64_t)identifyNS);
        sqe.DATA[1] = 0;
        sqe.NSID = nsid;
        sqe.CDWA = NVME_ADMIN_IDENTIFY_CNS_ID_NS;
        cqe = NVMEWaitingCMD(&ctrl->ASQ, &sqe);
        if ((cqe.STS >> 1) & 0xFF)
        {
            write_serial_string("CANNOT IDENTIFY NAMESPACE ");
            write_serial_hex(nsid);
            write_serial_string("\n");
            failed_namespace(identifyNS);
            return;
        }

        uint8_t currentLBAFormat = identifyNS->FLBA & 0xF;
        if (currentLBAFormat > identifyNS->NLBA)
        {
            write_serial_string("NVME NAMESPACE ");
            write_serial_hex(nsid);
            write_serial_string(" CURRENT LBA FORMAT ");
            write_serial_hex(currentLBAFormat);
            write_serial_string(" IS BEYOND WHAT THE NAMESPACE SUPPORTS ");
            write_serial_hex(identifyNS->NLBA + 1);
            write_serial_string("\n");
            failed_namespace(identifyNS);
            return;
        }

        if (!identifyNS->SIZE)
        {
            failed_namespace(identifyNS);
            return;
        }

        if (!NVME_DMA_MEMORY)
        {
            pagCont = 1;
            NVME_DMA_MEMORY = PAGE_ALLOC.alloc_pages(pagCont);
        }

        NVME_NAMESPACE *ns = (NVME_NAMESPACE *)malloc(sizeof(NVME_NAMESPACE));
        xmemset(ns, 0, sizeof(NVME_NAMESPACE));
        ns->CTRL = ctrl;
        ns->NSID = nsid;
        ns->NLBA = identifyNS->SIZE;

        NVME_LOGICAL_BLOCK_ADDRESS *fmt = identifyNS->LBAF + currentLBAFormat;

        ns->BSZ = 1ULL << fmt->DS;
        ns->META = fmt->MS;
        if (ns->BSZ > 4096)
        {
            write_serial_string("NVME NAMESPACE ");
            write_serial_hex(nsid);
            write_serial_string(" BLOCK SIZE ");
            write_serial_hex(ns->BSZ);
            write_serial_string(" > 0x1000");
            free(ns);
            failed_namespace(identifyNS);
            return;
        }

        if (mdts)
        {
            ns->MXRS = ((1ULL << mdts) * 4096) / ns->BSZ;
        }
        else
        {
            ns->MXRS = -1;
        }

        device_install(DEV_BLOCK, DEV_DISK, ns, "NVME", 0, nullptr, (void *)NvmeRead, (void *)NvmeWrite);
    }

    return;

FAILED_NVME:
    free(ctrl);
}
