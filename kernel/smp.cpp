#include "./alloc/page.h"
#include "../include/include.h"

extern uint64_t idle_entry(uint64_t arg);

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

uint64_t lapicid_to_cpuid[MAX_CPU_NUM];

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
    delay_ms(100);

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

extern bool TASK_INITIALIZED;

extern "C" void apu_entry(uint64_t cpu_id, uint64_t tr)
{
    __asm__ volatile("lgdt %0" : : "m"(gdt_ptr));

    __asm__ volatile(
        "movq %%rax, %%ds \n\t"
        "movq %%rax, %%es \n\t"
        "movq %%rax, %%fs \n\t"
        "movq %%rax, %%gs \n\t"
        "movq %%rax, %%ss \n\t"
        "pushq %[SELECTOR_CODE64] \n\t"
        "leaq .ap_next(%%RIP),%%rax \n\t"
        "pushq %%rax \n\t"
        "lretq \n\r"
        ".ap_next: \n\t"
        :
        : [SELECTOR_CODE64] "i"(SELECTOR_KERNEL_CS),
          [SELECTOR_DATA64] "a"(SELECTOR_KERNEL_DS)
        :);

    __asm__ volatile("lidt %0" : : "m"(idt_ptr));

    ltr(tr);

    write_serial_string("APU ");
    write_serial_dec(cpu_id);
    write_serial_string(" load GDT & IDT done\n");

    current_task->cpu_id = cpu_id;
    list_init(&current_task->list);
    current_task->mm = &init_mm;
    current_task->flags = PF_KTHREAD;
    current_task->preempt_count = 0;
    current_task->priority = 2;
    current_task->signal = 0;
    current_task->vrun_time = 0;
    current_task->state = TASK_RUNNING;

    current_task->thread = (struct thread_struct *)(current_task + 1);
    xmemset(current_task->thread, 0, sizeof(struct thread_struct));
    uint64_t stack_start = convert_physical_to_virtual((uint64_t)PAGE_ALLOC.alloc_pages(STACK_SIZE / PAGE_SIZE) + STACK_SIZE);
    current_task->thread->rsp0 = stack_start;
    current_task->thread->rsp = stack_start - sizeof(struct X64_REGS);
    struct X64_REGS regs;
    regs.cs = SELECTOR_KERNEL_CS;
    regs.ds = SELECTOR_KERNEL_DS;
    regs.es = SELECTOR_KERNEL_DS;
    regs.ss = SELECTOR_KERNEL_DS;
    regs.rflags = (1 << 9);
    regs.rip = (uint64_t)kernel_thread_func;
    regs.rbx = (uint64_t)idle_entry;
    regs.rdx = 0;
    xmemcpy((void *)current_task->thread->rsp, &regs, sizeof(struct X64_REGS));
    current_task->thread->fs = SELECTOR_KERNEL_DS;
    current_task->thread->gs = SELECTOR_KERNEL_DS;
    init_task_union.task.next = current_task;
    current_task->parent = &init_task_union.task;
    init_task[cpu_id] = current_task;

    write_serial_string("APU ");
    write_serial_dec(cpu_id);
    write_serial_string(" set current_task done\n");

    disable_intr();

    UINT64 ia32_apic_base = rdmsr(0x1b);
    ia32_apic_base |= 1 << 11;
    if (ApicInfo.x2Apic)
    {
        ia32_apic_base |= 1 << 10;
    }
    wrmsr(0x1b, ia32_apic_base);

    init_lApic();

    write_serial_string("APU ");
    write_serial_dec(cpu_id);
    write_serial_string(" init_lApic done\n");

    while (!TASK_INITIALIZED)
    {
        __asm__ __volatile__("pause");
    }

    write_serial_string("APU ");
    write_serial_dec(cpu_id);
    write_serial_string(" ready to schedule\n");

    enable_intr();

    while (1)
    {
        __asm__ __volatile__("pause");
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

    xmemcpy((void *)convert_physical_to_virtual(APU_BASE_ADDR), &_APU_boot_start, (UINTN)&_APU_boot_end - (UINTN)&_APU_boot_start);

    uint64_t cr3;
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3));
    uint64_t *page_table = (uint64_t *)convert_physical_to_virtual(APU_BASE_ADDR + 8);
    uint64_t *ap_entry = page_table + 1;
    uint64_t *rsp = ap_entry + 1;
    uint64_t *cpu_id = rsp + 1;
    uint64_t *tr = cpu_id + 1;
    uint64_t *ready = tr + 1;

    *page_table = cr3;
    *ap_entry = (uint64_t)apu_entry;

    for (int i = 0; i < this->apu_count; i++)
    {
        if (this->apu_list[i]->local_apic_id == lapic_id())
            continue;

        write_serial_string("Initializing CPU ");
        write_serial_dec(this->apu_list[i]->local_apic_id);
        write_serial_string("\n");

        *ready = 0;
        *cpu_id = i;
        uint64_t tr_value = GDT_TYPE_TSS + i * 2;
        *tr = tr_value;
        init_tss[i] = INIT_TSS;
        gdt_set_system_gate(tr_value, (UINT64)&init_tss[i], sizeof(init_tss[i]) - 1, 0x89, 0xAF);
        uint64_t stack_start = convert_physical_to_virtual((uint64_t)PAGE_ALLOC.alloc_pages(STACK_SIZE / PAGE_SIZE) + STACK_SIZE);
        xmemset((void *)stack_start, 0, STACK_SIZE);
        *rsp = stack_start;
        init_tss[i].rsp0 = stack_start;
        init_tss[i].rsp1 = stack_start;
        init_tss[i].rsp2 = stack_start;
        uint64_t ist_stack_start = convert_physical_to_virtual((uint64_t)PAGE_ALLOC.alloc_pages(STACK_SIZE / PAGE_SIZE) + STACK_SIZE);
        xmemset((void *)ist_stack_start, 0, STACK_SIZE);
        init_tss[i].ist1 = ist_stack_start;
        init_tss[i].ist2 = ist_stack_start;
        init_tss[i].ist3 = ist_stack_start;
        init_tss[i].ist4 = ist_stack_start;
        init_tss[i].ist5 = ist_stack_start;
        init_tss[i].ist6 = ist_stack_start;
        init_tss[i].ist7 = ist_stack_start;

        lapicid_to_cpuid[i] = this->apu_list[i]->local_apic_id;

        send_init_and_startup_ipi(this->apu_list[i]->local_apic_id);

        while (*ready == 0)
        {
            __asm__ __volatile__("pause");
        }

        write_serial_string("CPU ");
        write_serial_dec(this->apu_list[i]->local_apic_id);
        write_serial_string(" initialized\n");
    }

    xmemset((void *)PAGE_TABLE_BASE_ADDR, 0, PAGE_SIZE / 2);
}

int SMP::ap_count()
{
    return this->apu_count;
}

uint64_t get_cpuid_by_lapicid(uint64_t lapicid)
{
    for (uint64_t i = 0; i < MAX_CPU_NUM; i++)
    {
        if (lapicid_to_cpuid[i] == lapicid)
        {
            return i;
        }
    }

    return 0;
}
