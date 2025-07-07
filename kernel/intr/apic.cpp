#include <apic/apic.h>
#include <apic/madt.h>
#include <cpu/msr.h>
#include <proto.h>

APIC_INFO ApicInfo;

/**
 *
 * @brief 检测APIC是否存在
 *
 * @return 0=NotSupport 1=APIC 2=x2APIC
 *
 */
UINT8 check_apic()
{
    UINT32 cpuid_eax, cpuid_ebx, cpuid_ecx, cpuid_edx;
    asm_cpuid(1, 0, &cpuid_eax, &cpuid_ebx, &cpuid_ecx, &cpuid_edx);
    if (cpuid_ecx & CPUID_ECX_x2APIC)
    {
        // 支持x2APIC
        return 2;
    }
    else if (cpuid_edx & CPUID_EDX_APIC)
    {
        // 支持xAPIC
        return 1;
    }
    return 0;
}

static inline void mmio_write32(UINT32 *addr, UINT32 data)
{
    *(volatile UINT32 *)convert_physical_to_virtual((uint64_t)addr) = data;
}

static inline UINT32 mmio_read32(void *addr)
{
    return *(volatile UINT32 *)convert_physical_to_virtual((uint64_t)addr);
}

void lapic_write(UINT32 reg, UINT64 value)
{
    if (ApicInfo.x2Apic)
    {
        wrmsr(0x800 + (reg >> 4), value);
        return;
    }
    mmio_write32((UINT32 *)((UINT64)ApicInfo.LocalApicAddr + reg), value);
}

UINT32 lapic_read(UINT32 reg)
{
    if (ApicInfo.x2Apic)
    {
        return rdmsr(0x800 + (reg >> 4));
    }
    return mmio_read32((UINT32 *)((UINT64)ApicInfo.LocalApicAddr + reg));
}

UINT64 lapic_id()
{
    if (ApicInfo.x2Apic)
    {
        return lapic_read(LAPIC_REG_ID);
    }
    return (lapic_read(LAPIC_REG_ID) >> 24);
}

static void ioapic_write(UINT32 reg, UINT32 value)
{
    mmio_write32((UINT32 *)((UINT64)ApicInfo.ioApicAddr), reg);
    mmio_write32((UINT32 *)((UINT64)ApicInfo.ioApicAddr + 0x10), value);
}

static uint32_t ioapic_read(UINT32 reg)
{
    mmio_write32((UINT32 *)((UINT64)ApicInfo.ioApicAddr), reg);
    return mmio_read32((UINT32 *)((UINT64)ApicInfo.ioApicAddr + 0x10));
}

void ioapic_disable(UINT8 vector)
{
    uint64_t index = 0x10 + ((vector - 32) * 2);
    uint64_t value = (uint64_t)ioapic_read(index + 1) << 32 | (uint64_t)ioapic_read(index);
    value |= (0x10000UL);
    ioapic_write(index, (uint32_t)(value & 0xFFFFFFFF));
    ioapic_write(index + 1, (uint32_t)(value >> 32));
}

void ioapic_enable(UINT8 vector)
{
    uint64_t index = 0x10 + ((vector - 32) * 2);
    uint64_t value = (uint64_t)ioapic_read(index + 1) << 32 | (uint64_t)ioapic_read(index);
    value &= (~0x10000UL);
    ioapic_write(index, (uint32_t)(value & 0xFFFFFFFF));
    ioapic_write(index + 1, (uint32_t)(value >> 32));
}

void ioapic_add(UINT8 vector, UINT32 irq)
{
    UINT32 ioredtbl = (UINT32)(0x10 + (uint32_t)(irq * 2));
    UINT64 redirect = (UINT64)vector;
    redirect |= lapic_id() << 56;
    ioapic_write(ioredtbl, (UINT32)redirect);
    ioapic_write(ioredtbl + 1, (UINT32)(redirect >> 32));
}

/**
 *
 * @brief 初始化Local APIC
 *
 * @param MADT MADT表地址
 *
 */
void init_apic(UINT64 MADT0)
{
    write_serial_string("Initializing APIC...\n");
    __asm__ volatile("cli" ::: "memory");

    // 屏蔽8259A
    disable_8259A();
    write_serial_string("Mask 8259A Success.\n");
    io_mfence();

    switch (check_apic())
    {
    case 2:
        write_serial_string("You PC support x2APIC.\n");
        ApicInfo.x2Apic = true;
        break;
    case 1:
        write_serial_string("You PC support xAPIC.\n");
        ApicInfo.x2Apic = false;
        break;
    default:
        // wsod_apic_not_found();
        break; // 虽然这里理论上永远都不会执行...
    }

    UINT64 ia32_apic_base = rdmsr(0x1b);
    ia32_apic_base |= 1 << 11;
    if (ApicInfo.x2Apic)
    {
        ia32_apic_base |= 1 << 10;
    }
    wrmsr(0x1b, ia32_apic_base);

    MADT *madt = (MADT *)MADT0;
    ApicInfo.LocalApicAddr = (UINT64)madt->local_apic_address;
    UINT64 current = 0;
    for (;;)
    {
        if (current + ((UINT32)sizeof(MADT) - 1) >= madt->h.Length)
        {
            break;
        }
        MadtHeader *header = (MadtHeader *)((UINT64)(&madt->entries) + current);
        if (header->entry_type == MADT_APIC_IO)
        {
            MadtIOApic *ioapic = (MadtIOApic *)((UINT64)(&madt->entries) + current);
            ApicInfo.ioApicAddr = (UINT64)ioapic->address;
        }
        current += (UINT64)header->length;
    }

    write_serial_string("APIC Initialize Success.\n");
}
