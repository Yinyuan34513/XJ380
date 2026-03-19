#include "ahci/ahci.h"
#include "proto.hpp"

void sata_read_error(struct hba_port *port)
{
    write_serial_fmt("SATA read error\n");
    uint32_t tfd                        = port->regs[HBA_RPxTFD];
    port->device->last_result.sense_key = (tfd & 0xf000) >> 12;
    port->device->last_result.error     = (tfd & 0x0f00) >> 8;
    port->device->last_result.status    = tfd & 0x00ff;
}

void sata_submit(struct hba_device *dev, struct blkio_req *io_req)
{
    struct hba_port *port = dev->port;
    struct hba_cmdh *header;
    struct hba_cmdt *table;
    io_req->status        = -1;
    int write = !!(io_req->flags & BLKIO_WRITE);
    int slot  = hba_prepare_cmd(port, &table, &header);
    if (slot < 0) {
        write_serial_fmt("AHCI: No free command slot for %s lba=%llu len=%llu\n", write ? "write" : "read",
                         io_req->lba, io_req->len);
        return;
    }
    if (hba_bind_sbuf(header, table, (void *)io_req->buf, io_req->len) != 0) {
        write_serial_fmt("AHCI: Failed to bind %s buffer lba=%llu len=%llu\n", write ? "write" : "read", io_req->lba,
                         io_req->len);
        return;
    }
    if (!write && io_req->lba < 8) {
        uint64_t table_phys = (uint64_t)driver_virt_to_phys((uint64_t)table);
        uint64_t buf_phys   = (uint64_t)driver_virt_to_phys((uint64_t)io_req->buf);
        write_serial_fmt("AHCI TRACE: lba=%llu len=0x%llx slot=%d table=0x%llx table_phys=0x%llx buf=0x%llx buf_phys=0x%llx prdt=0x%llx\n",
                         io_req->lba, io_req->len, slot, (uint64_t)table, table_phys, io_req->buf, buf_phys,
                         ((uint64_t)table->entries[0].data_base_upper << 32) | table->entries[0].data_base);
    }
    header->options |= HBA_CMDH_WRITE * write;
    uint16_t             count = ICEIL(io_req->len, port->device->block_size);
    struct sata_reg_fis *fis   = (struct sata_reg_fis *)(&table->command_fis);
    if ((port->device->flags & HBA_DEV_FEXTLBA))
    {
        sata_create_fis(fis, write ? ATA_WRITE_DMA_EXT : ATA_READ_DMA_EXT, io_req->lba, count);
    }
    else { sata_create_fis(fis, write ? ATA_WRITE_DMA : ATA_READ_DMA, io_req->lba, count); }
    fis->dev = (1 << 6);
    if (!write && io_req->lba < 8) {
        write_serial_fmt("AHCI TRACE: FIS=%02x %02x %02x %02x %02x %02x %02x %02x count=0x%x\n",
                         fis->head.type, fis->head.options, fis->head.status_cmd, fis->head.feat_err, fis->lba0,
                         fis->lba8, fis->lba16, fis->dev, fis->count);
    }
    if (ahci_try_send(port, slot)) {
        io_mfence();
        if (!write && io_req->lba < 8) {
            write_serial_fmt("AHCI TRACE: done lba=%llu prdbc=0x%x tfd=0x%x\n",
                             io_req->lba, header->transferred_size, port->regs[HBA_RPxTFD]);
        }
        delay_us_hp(50);
        io_req->status = 0;
    }
}