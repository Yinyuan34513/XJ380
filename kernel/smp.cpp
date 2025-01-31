#include "../include/include.h"

extern uint8_t _APU_boot_start;
extern uint8_t _APU_boot_end;

#define APU_BASE_ADDR 0x80000

// 定义APIC寄存器的偏移量
#define APIC_ICR_LOW 0x300
#define APIC_ICR_HIGH 0x310

// 定义IPI命令
#define IPI_INIT 0x4500
#define IPI_STARTUP 0x4600

SMP SMP_MANAGER;

void delay()
{
    for (int i = 0; i < 1000000; i++)
    {
        __asm__ __volatile__("nop");
    }
}

// 发送IPI的函数
void send_ipi(UINT32 apic_id, UINT32 command)
{
    // 写入高32位，指定目标APIC ID
    lapic_write(APIC_ICR_HIGH, apic_id << 24);
    // 写入低32位，指定命令
    lapic_write(APIC_ICR_LOW, command);
}

// 向AP发送INIT IPI和STARTUP IPI
void send_init_and_startup_ipi(UINT32 apic_id)
{
    // 发送INIT IPI
    send_ipi(apic_id, IPI_INIT);
    delay();

    // 发送STARTUP IPI
    send_ipi(apic_id, IPI_STARTUP | (APU_BASE_ADDR >> 12)); // 0x08是启动代码的段选择符
}

extern IDTR_TYPE idt_ptr;

static inline uint64_t rdmsr(UINT32 msr)
{
    UINT32 eax, edx;
    __asm__ volatile("rdmsr" : "=a"(eax), "=d"(edx) : "c"(msr));
    return ((UINT64)edx << 32) | eax;
}

static inline void wrmsr(UINT32 msr, UINT64 value)
{
    UINT32 eax = (UINT32)value;
    UINT32 edx = value >> 32;
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(eax), "d"(edx));
}
extern APIC_INFO ApicInfo;
extern void init_lApic();

extern "C" void apu_entry()
{
    write_serial_string("APU Starting...\n");

    __asm__ volatile("lidt %0" : : "m"(idt_ptr));

    UINT64 ia32_apic_base = rdmsr(0x1b);
    ia32_apic_base |= 1 << 11;
    if (ApicInfo.x2Apic)
    {
        ia32_apic_base |= 1 << 10;
    }
    wrmsr(0x1b, ia32_apic_base);

    init_lApic();

    while (1)
    {
        __asm__ __volatile__("hlt");
    }
}

SMP::SMP()
{
    this->apu_count = 0;
}

SMP::~SMP()
{
}

void SMP::ap_startup(uint64_t madt_addr)
{
    MADT *madt = (MADT *)madt_addr;
    UINT64 current = 0;
    for (;;)
    {
        if (current + ((UINT32)sizeof(MADT) - 1) >= madt->h.Length)
        {
            break;
        }
        MadtHeader *header = (MadtHeader *)((uint64_t)(&madt->entries) + current);
        if (header->entry_type == MADT_APIC_LOCAL)
        {
            MadtLocalApic *lapic = (MadtLocalApic *)((uint64_t)(&madt->entries) + current);
            this->apu_list[this->apu_count] = lapic;
            this->apu_count++;
        }
        current += (UINT64)header->length;
    }

    xmemcpy((void *)APU_BASE_ADDR, &_APU_boot_start, (UINTN)&_APU_boot_end - (UINTN)&_APU_boot_start);

    uint64_t cr3;
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3));
    uint64_t *page_table = (uint64_t *)(APU_BASE_ADDR + 8);
    uint64_t *ap_entry = page_table + 1;
    uint64_t *ready = ap_entry + 1;

    *page_table = cr3;
    *ap_entry = (uint64_t)apu_entry;

    for (int i = 0; i < this->apu_count; i++)
    {
        if (this->apu_list[i]->local_apic_id == (lapic_read(LAPIC_REG_ID) >> 24))
        {
            continue;
        }
        write_serial_string("Initializing CPU ");
        write_serial_dec(this->apu_list[i]->local_apic_id);
        write_serial_string("\n");
        send_init_and_startup_ipi(this->apu_list[i]->local_apic_id);
        while (*ready == 0)
        {
            __asm__ __volatile__("pause");
        }
        write_serial_string("CPU ");
        write_serial_dec(this->apu_list[i]->local_apic_id);
        write_serial_string(" initialized\n");
    }
}
