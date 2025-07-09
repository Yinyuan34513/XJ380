#include <smp/smp.h>
#include <pctable/gdt.h>
#include <apic/apic.h>
#include <apic/madt.h>
#include <cpu/msr.h>
#include <proto.h>
#include <stdint.h>

extern GDT_PTR gdt_ptr;
extern APIC_INFO ApicInfo;
extern void *temp_stack[MAX_CPU_NUM];
extern void *smp_tpl[MAX_CPU_NUM];
extern uint32_t smp_trampoline_rs;

typedef char symbol[];
extern symbol smp_trampoline_start;
extern size_t smp_trampoline_size;

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

struct trampoline_passed_info {
    uint8_t  smp_tpl_booted_flag;
    uint8_t  smp_tpl_target_mode;
    uint32_t smp_tpl_pagemap;
    uint32_t smp_tpl_info_struct;
    GDT_PTR smp_tpl_gdt;
    uint64_t smp_tpl_hhdm;
    uint64_t smp_tpl_bsp_apic_addr_msr;
    uint64_t smp_tpl_mtrr_restore;
    uint64_t smp_tpl_temp_stack;
} __attribute__((packed));

bool start_ap(uint32_t cs_number, uint32_t lapic_id, GDT_PTR *gdtr, PROCESSOR_INFO *cpuinf)
{
    write_serial_string("Starting AP...\n");
    // 准备跳板
    if (smp_trampoline_size > smp_trampoline_rs) {
        write_serial_string("[Error][SMP] Trampoline Size is Too Small."); return false;
    }

    static void *trampoline = smp_tpl[cs_number];
    if (trampoline == NULL) {
        xmemcpy(trampoline, smp_trampoline_start, smp_trampoline_size);
    }

    static void *tmp_stack = temp_stack[cs_number];

    static struct trampoline_passed_info *passed_info = NULL;
    if (passed_info == NULL) {
        passed_info = (trampoline_passed_info *)(((uint64_t)trampoline + smp_trampoline_size)
                                                   - sizeof(struct trampoline_passed_info));
    }

    uint32_t eax, ebx, ecx, edx;
    bool nx = true;
    // 看看你的NX
    asm_cpuid(0x80000001, 0, &eax, &ebx, &ecx, &edx);
    if (!(edx & (1 << 20))) {
        nx = false;
    }

    passed_info->smp_tpl_info_struct = (uint64_t)(cpuinf);
    passed_info->smp_tpl_booted_flag = 0;
    passed_info->smp_tpl_pagemap     = 4;
    passed_info->smp_tpl_target_mode = ((uint32_t)(1) << 1)
                                     | ((uint32_t)(nx << 3))
                                     | ((uint32_t)(1 << 4));
    passed_info->smp_tpl_gdt = *gdtr;
    passed_info->smp_tpl_hhdm = 0xffff800000000000;
    passed_info->smp_tpl_bsp_apic_addr_msr = rdmsr(0x1b);
    passed_info->smp_tpl_mtrr_restore = (uint64_t)(mtrr_restore);
    passed_info->smp_tpl_temp_stack = (uint64_t)(tmp_stack);

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
                                     ((size_t)trampoline / 4096) | 0x4600);
    } else {
        lapic_write(LAPIC_REG_ICR1, lapic_id << 24);
        lapic_write(LAPIC_REG_ICR0, ((size_t)trampoline / 4096) | 0x4600);
    }

    for (int i = 0; i < 100; i++) {
        if (smp_lock_read(&passed_info->smp_tpl_booted_flag) == 1) {
            return true;
        }
        delay_ms(100);
    }

    return false;
}

void init_smp(UINT64 MADT0, XSK_SMP_INFO *xsi)
{
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
    UINT64 current = 0;
    for (;;)
    {
        if (current + ((UINT32)sizeof(MADT) - 1) >= madt->h.Length)
        {
            break;
        }
        MadtHeader *header = (MadtHeader *)((UINT64)(&madt->entries) + current);
        if (header->entry_type == MADT_APIC_LOCAL)
        {
            madt_local_apic *lapic = (madt_local_apic *)((UINT64)(&madt->entries) + current);

            // 让我看看你发育正不正常呀
            if ((lapic->flags & 1) ^ ((lapic->flags >> 1) & 1))
                max_cpus++;
        }
        else if (header->entry_type == MADT_X2APIC_LOCAL)
        {
            if (!ApicInfo.x2Apic)
                continue;
            
            madt_x2_localapic *x2lapic = (madt_x2_localapic *)((UINT64)(&madt->entries) + current);

            if ((x2lapic->flags & 1) ^ ((x2lapic->flags >> 1) & 1))
                max_cpus++;
        }
        current += (UINT64)header->length;
    }

    xsi->cpu_count = 0;
    
    mtrr_save();

    write_serial_string("Start Up APs...\n");
    // 尝试启动所有AP
    current = 0;
    for (;;)
    {
        if (current + ((UINT32)sizeof(MADT) - 1) >= madt->h.Length)
        {
            break;
        }
        MadtHeader *header = (MadtHeader *)((UINT64)(&madt->entries) + current);
        if (header->entry_type == MADT_APIC_LOCAL)
        {
            madt_local_apic *lapic = (madt_local_apic *)((UINT64)(&madt->entries) + current);

            // 让我看看你发育正不正常呀
            if (!((lapic->flags & 1) ^ ((lapic->flags >> 1) & 1))) {
                current += (UINT64)header->length;
                continue;
            }
            
            PROCESSOR_INFO *cpuinf = &xsi->pcr_inf[xsi->cpu_count];
            cpuinf->processor_id = lapic->ACPI_Processor_UID;
            cpuinf->lapic_id = lapic->local_apic_id;

            // 如果是BSP就跑路
            if (lapic->local_apic_id == bsp_lapic_id) {
                (xsi->cpu_count)++;
                current += (UINT64)header->length;
                continue;
            }

            if (!start_ap(xsi->cpu_count, lapic->local_apic_id, &gdtr, cpuinf)) {
                write_serial_string("[Warning][SMP] Failed to Bring Up AP.\n");
                current += (UINT64)header->length;
                continue;
            }

            (xsi->cpu_count)++;
        }
        else if (header->entry_type == MADT_X2APIC_LOCAL)
        {
            madt_x2_localapic *x2lapic = (madt_x2_localapic *)((UINT64)(&madt->entries) + current);

            // 让我看看你发育正不正常呀
            if (!((x2lapic->flags & 1) ^ ((x2lapic->flags >> 1) & 1))) {
                current += (UINT64)header->length;
                continue;
            }
            
            PROCESSOR_INFO *cpuinf = &xsi->pcr_inf[xsi->cpu_count];
            cpuinf->processor_id = x2lapic->acpi_processor_uid;
            cpuinf->lapic_id = x2lapic->x2apic_id;

            // 如果是BSP就跑路
            if (x2lapic->x2apic_id == bsp_lapic_id) {
                (xsi->cpu_count)++;
                current += (UINT64)header->length;
                continue;
            }

            if (!start_ap(xsi->cpu_count, x2lapic->x2apic_id, &gdtr, cpuinf)) {
                write_serial_string("[Warning][SMP] Failed to Bring Up AP.\n");
                current += (UINT64)header->length;
                continue;
            }

            (xsi->cpu_count)++;
        }
        current += (UINT64)header->length;
    }
}