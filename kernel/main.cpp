#include <krlibc.h>
#include <mm/frame.h>
#include <task/pcb.h>
#include <efi/boot.h>
#include <efi/efi.h>
#include <efi/fbc.h>
#include <pci/pci.h>
#include <proto.hpp>
#include <ahci/ahci.h>
#include <fs/partition.h>
#include <fs/vfs/sys.h>
#include <fs/vfs/vfs.h>
#include <fs/fatfs/fatfs.h>
#include <fs/fs_syscall.h>
#include <fs/vfs/dev.h>
#include <graphics/sheet.h>
#include <graphics/GOP.hpp>

#define NULL 0
const FrameBufferConfig *fbc_addr;
EFI_SYSTEM_TABLE        *ST;
BOOT_CONFIG             *BC;
uint64_t                *saved_mtrrs;
void                    *temp_stack[MAX_CPU_NUM];
bool                     no_interrupt = false; // 为true时代表严禁开启中断

void test_task()
{
    uint64_t time = nanoTime();
    while (true)
    {
        time = nanoTime();
        write_serial_fmt("Hello from `test_task` in %llu\n", time);
    }
}

void test_task1()
{
    uint64_t time = nanoTime();
    while (true)
    {
        time = nanoTime();
        write_serial_fmt("Hello from `test_task1` in %llu\n", time);
    }
}

void test_task2()
{
    while (true)
    {
        __asm__ volatile ("hlt");
    }
}

extern "C" void KernelMain(const FrameBufferConfig &fbc, EFI_SYSTEM_TABLE &SystemTable, BOOT_CONFIG &BootConfig)
{
    (void)fbc;         // 暂时不用
    (void)SystemTable; // 暂时不用
    no_interrupt = true;
    disable_intr();
    init_idt();
    init_gdt();
    init_hpet(BootConfig.HPET);
    init_apic(BootConfig.MADT);
    init_hhdm();
    init_frame(BootConfig.MemoryMap);
    page_setup();
    init_heap();

    pci_setup(BootConfig.MCFG);
    pci_init();
    ahci_init();
    vfs_init();
    dev_init();
    partition_init();
    sysfs_init();
    fs_syscall_init();
    fatfs_init();

    mount_root();

    keyboard_init();
    mouse_init();

    vfs_node_t f;
    f=vfs_open("/1.txt");
    char buf[1024];
    if(vfs_read(f,buf,0,f->size)==-1)write_serial_fmt("read failed\n");
    write_serial_fmt("read file: %s\n",buf);

    // 超绝偷懒解法，等有内存管理且在这之前初始化的时候叫我来改
    saved_mtrrs = (uint64_t *)(BootConfig.saved_mtrrs);
    for (int i = 0; i < MAX_CPU_NUM; ++i)
        temp_stack[i] = BootConfig.temp_stack[i];

    init_smp(BootConfig.MADT);

    uint64_t phys = alloc_frames(2);
    write_serial_string("Alloc memory: ");
    write_serial_hex(phys);
    write_serial_string("\n");

    process_setup();

    create_kernel_thread((void *)test_task, NULL, "test_task", NULL);
    create_kernel_thread((void *)test_task1, NULL, "test_task1", NULL);

    pcb_t ugp = create_process_group("User Group", NULL, NULL);
    create_user_thread((void *)test_task2, NULL, "test_task2", ugp);


    open_interrupt;
    no_interrupt = false;
    enable_scheduler();

    while (true)
    {
        __asm__ __volatile__("pause");
    }
}
