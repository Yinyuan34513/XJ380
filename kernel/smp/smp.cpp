#include <smp/smp.h>
#include <pctable/gdt.h>
#include <apic/apic.h>
#include <apic/madt.h>
#include <cpu/msr.h>
#include <proto.h>
#include <stdint.h>
#include <krlibc.h>
#include "../build_settings.h"
#include "cpu/lock.h"
#include "pctable/idt.h"

XSK_SMP_INFO *xsi;

extern GDT_PTR gdt_ptr;
extern APIC_INFO ApicInfo;
extern void *temp_stack[MAX_CPU_NUM];
extern uint32_t smp_trampoline_rs;

typedef char symbol[];

extern uint8_t _APU_boot_start;
extern uint8_t _APU_boot_end;

extern IDTR_TYPE idt_ptr;

#define APU_BASE_ADDR 0x20000

#define smp_lock_read(var) ({ \
    typeof(*var) locked_read__ret = 0; \
    asm volatile ( \
        "lock xadd %0, %1" \
        : "+r" (locked_read__ret) \
        : "m" (*(var)) \
        : "memory" \
    ); \
    locked_read__ret; \
})

spin_t apu_lock = SPIN_INIT;

extern "C" void apu_entry(uint64_t cpu_id, uint64_t tr)
{
    spin_lock(apu_lock);

    __asm__ volatile("lidt %0" : : "m"(idt_ptr));

    PROCESSOR_INFO *info = NULL;
    for (size_t i = 0; i < MAX_CPU_NUM; i++) {
        PROCESSOR_INFO *inter_info = &xsi->pcr_inf[i];
        if(inter_info->lapic_id == lapic_id()){
            info = inter_info;
            break;
        }
    }
    if(info == NULL){
        write_serial_string("APU Entry Failed: CPU not found.\n");
        while (true)
        {
            asm volatile ("hlt");
        }
    }

    spin_unlock(apu_lock);
    while (true) 
    {
        asm volatile ("hlt");
    }
}

bool start_ap(uint32_t cs_number, uint32_t lapic_id, GDT_PTR *gdtr, PROCESSOR_INFO *cpuinf)
{
    write_serial_string("Initializing CPU ");
    write_serial_dec(lapic_id);
    write_serial_string(".\n");

    // 准备跳板
    static void *trampoline = (void *)(APU_BASE_ADDR);
    memcpy((void *)(APU_BASE_ADDR + 0xFFFF800000000000), &_APU_boot_start, (uint64_t)&_APU_boot_end - (uint64_t)&_APU_boot_start);

    uint32_t eax, ebx, ecx, edx;
    bool nx = true;
    // 看看你的NX
    asm_cpuid(0x80000001, 0, &eax, &ebx, &ecx, &edx);
    if (!(edx & (1 << 20))) {
        nx = false;
    }

    uint64_t cr3;
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3));
    uint64_t *page_table = (uint64_t *)(APU_BASE_ADDR + 8 + 0xFFFF800000000000);
    uint64_t *ap_entry = page_table + 1;
    uint64_t *rsp = ap_entry + 1;
    uint64_t *cpu_id = rsp + 1;
    uint64_t *tr = cpu_id + 1;
    uint64_t *ready = tr + 1;

    *page_table = cr3;
    *ap_entry = (uint64_t)apu_entry;

    *rsp = (uint64_t)((uint64_t)(temp_stack[cs_number]) + STACK_SIZE);
    *cpu_id = cs_number;

    asm volatile ("" ::: "memory");

    // Send the INIT IPI
    if (ApicInfo.x2Apic) {
        lapic_write(LAPIC_REG_ICR0, ((uint64_t)lapic_id << 32) | 0x4500);
    } else {
        lapic_write(LAPIC_REG_ICR1, lapic_id << 24);
        lapic_write(LAPIC_REG_ICR0, 0x4500);
    }

    delay_ms(100);

    // Send the Startup IPI
    if (ApicInfo.x2Apic) {
        lapic_write(LAPIC_REG_ICR0, ((uint64_t)lapic_id << 32) |
                                     ((size_t)trampoline >> 12) | 0x4600);
    } else {
        lapic_write(LAPIC_REG_ICR1, lapic_id << 24);
        lapic_write(LAPIC_REG_ICR0, ((size_t)trampoline >> 12) | 0x4600);
    }

    for (int i = 0; i < 100; i++) {
        if (*ready == 1) {
            write_serial_string("CPU ");
            write_serial_dec(lapic_id);
            write_serial_string(" Initialize Success.\n");
            return true;
        }
        delay_ms(1000);
    }

    return false;
}

void init_smp(uint64_t MADT0)
{
    xsi = (XSK_SMP_INFO*)malloc(sizeof(XSK_SMP_INFO));
    write_serial_string("Initializing SMP...\n");
    GDT_PTR gdtr = gdt_ptr;

    uint8_t bsp_lapic_id;
    uint32_t bsp_x2apic_id;

    if (ApicInfo.x2Apic) {
        bsp_x2apic_id = lapic_read(LAPIC_REG_ID);
        bsp_lapic_id = bsp_x2apic_id;
    } else {
        bsp_lapic_id = lapic_read(LAPIC_REG_ID) >> 24;
        bsp_x2apic_id = bsp_lapic_id;
    }

    xsi->bsp_lapic_id = bsp_x2apic_id;

    xsi->cpu_count = 0;

    write_serial_string("Searching CPUs...\n");
    size_t max_cpus = 0;
    MADT *madt = (MADT *)MADT0;
    uint64_t current = 0;
    for (;;)
    {
        if (current + ((uint32_t)sizeof(MADT) - 1) >= madt->h.Length)
        {
            break;
        }
        MadtHeader *header = (MadtHeader *)((uint64_t)(&madt->entries) + current);
        if (header->entry_type == MADT_APIC_LOCAL)
        {
            madt_local_apic *lapic = (madt_local_apic *)((uint64_t)(&madt->entries) + current);

            // 让我看看你发育正不正常呀
            if ((lapic->flags & 1) ^ ((lapic->flags >> 1) & 1))
                max_cpus++;
        }
        else if (header->entry_type == MADT_X2APIC_LOCAL)
        {
            if (!ApicInfo.x2Apic)
                continue;
            
            madt_x2_localapic *x2lapic = (madt_x2_localapic *)((uint64_t)(&madt->entries) + current);

            if ((x2lapic->flags & 1) ^ ((x2lapic->flags >> 1) & 1))
                max_cpus++;
        }
        current += (uint64_t)header->length;
    }

    xsi->cpu_count = 0;
    
    mtrr_save();

    write_serial_string("Start Up APs...\n");
    // 尝试启动所有AP
    current = 0;
    for (;;)
    {
        if (current + ((uint32_t)sizeof(MADT) - 1) >= madt->h.Length)
        {
            break;
        }
        MadtHeader *header = (MadtHeader *)((uint64_t)(&madt->entries) + current);
        if (header->entry_type == MADT_APIC_LOCAL)
        {
            madt_local_apic *lapic = (madt_local_apic *)((uint64_t)(&madt->entries) + current);

            // 让我看看你发育正不正常呀
            if (!((lapic->flags & 1) ^ ((lapic->flags >> 1) & 1))) {
                current += (uint64_t)header->length;
                continue;
            }
            
            PROCESSOR_INFO *cpuinf = &xsi->pcr_inf[xsi->cpu_count];
            cpuinf->processor_id = lapic->ACPI_Processor_UID;
            cpuinf->lapic_id = lapic->local_apic_id;
            cpuinf->scheduler_queue = queue_init();

            // 如果是BSP就跑路
            if (lapic->local_apic_id == bsp_lapic_id) {
                (xsi->cpu_count)++;
                current += (uint64_t)header->length;
                continue;
            }

            if (!start_ap(xsi->cpu_count, lapic->local_apic_id, &gdtr, cpuinf)) {
                write_serial_string("[Warning][SMP] Failed to Bring Up AP.\n");
                current += (uint64_t)header->length;
                continue;
            }

            (xsi->cpu_count)++;
        }
        else if (header->entry_type == MADT_X2APIC_LOCAL)
        {
            madt_x2_localapic *x2lapic = (madt_x2_localapic *)((uint64_t)(&madt->entries) + current);

            // 让我看看你发育正不正常呀
            if (!((x2lapic->flags & 1) ^ ((x2lapic->flags >> 1) & 1))) {
                current += (uint64_t)header->length;
                continue;
            }
            
            PROCESSOR_INFO *cpuinf = &xsi->pcr_inf[xsi->cpu_count];
            cpuinf->processor_id = x2lapic->acpi_processor_uid;
            cpuinf->lapic_id = x2lapic->x2apic_id;
            cpuinf->scheduler_queue = queue_init();

            // 如果是BSP就跑路
            if (x2lapic->x2apic_id == bsp_lapic_id) {
                (xsi->cpu_count)++;
                current += (uint64_t)header->length;
                continue;
            }

            if (!start_ap(xsi->cpu_count, x2lapic->x2apic_id, &gdtr, cpuinf)) {
                write_serial_string("[Warning][SMP] Failed to Bring Up AP.\n");
                current += (uint64_t)header->length;
                continue;
            }

            (xsi->cpu_count)++;
        }
        current += (uint64_t)header->length;
    }
}