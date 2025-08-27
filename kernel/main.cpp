#include <ahci/ahci.h>
#include <efi/boot.h>
#include <efi/efi.h>
#include <efi/fbc.h>
#include <font.h>
#include <fs/fatfs/fatfs.h>
#include <fs/fs_syscall.h>
#include <fs/partition.h>
#include <fs/vfs/dev.h>
#include <fs/vfs/sys.h>
#include <fs/vfs/vfs.h>
#include <global_color.h>
#include <graphics/GOP.hpp>
#include <graphics/sheet.h>
#include <krlibc.h>
#include <mm/frame.h>
#include <pci/pci.h>
#include <proto.hpp>
#include <ps2/keyboard.h>
#include <ps2/mouse.h>
#include <rtc.h>
#include <syscall/syscall.h>
#include <task/pcb.h>
#include <ttf.h>

SHEET_INFO *sht_img;
XWM_INFO   *xwmii;
uint16_t   *msid;

extern mouse_dec ms_dec;
extern bool      winRD_lock;

#define NULL 0
const FrameBufferConfig *fbc_addr;
EFI_SYSTEM_TABLE        *ST;
BOOT_CONFIG             *BC;
uint64_t                *saved_mtrrs;
void                    *temp_stack[MAX_CPU_NUM];
bool                     no_interrupt = false; // 为true时代表严禁开启中断
extern spin_t            fb_print_lock;

void test_task()
{
    for (int i = 0; 1 == 1; i++); // 防优化
}

void init_cpu()
{
    __asm__ __volatile__("movq %%cr0, %%rax\n\t"
                         "and $0xFFF3, %%ax	\n\t" // clear coprocessor emulation CR0.EM and CR0.TS
                         "or $0x2, %%ax\n\t"      // set coprocessor monitoring  CR0.MP
                         "movq %%rax, %%cr0\n\t"
                         "movq %%cr4, %%rax\n\t"
                         "or $(3 << 9), %%ax\n\t" // set CR4.OSFXSR and CR4.OSXMMEXCPT at the same time
                         "movq %%rax, %%cr4\n\t" ::
                             : "rax");
}

void desktop_flusher()
{
    disable_intr();

    write_serial_string("desktop flusher is running\n");

    uint16_t   desktop_sheet_id = 1145; // 如果SheetTest那行输出的是1145的话那么绝对有问题
    uint16_t   dock_sheet_id    = 1145;
    uint16_t   mouse_sheet_id   = 1145;
    SHEET_INFO shtinf;
    init_sheet(*fbc_addr, &shtinf);

    sht_img = &shtinf;
    msid    = &mouse_sheet_id;

    create_sheet(&shtinf, 0, 0, fbc_addr->horizontal_resolution, fbc_addr->vertical_resolution, FixedSheetType, 0,
                 &desktop_sheet_id);
    create_sheet(&shtinf, 0, 0, fbc_addr->horizontal_resolution, fbc_addr->vertical_resolution, FixedSheetType, 254,
                 &dock_sheet_id);
    create_sheet(&shtinf, (get_hor() - 16) / 2, (get_ver() - 28 - 29) / 2, 10, 21, FixedSheetType, 255,
                 &mouse_sheet_id);

    init_ttf();
    init_desktop(&shtinf, &desktop_sheet_id, &dock_sheet_id);

    write_serial_string("desktop initialized.\n");

    int fontx = 8;
    int fonty = 50;
    // DEBUG
#if BUILD_EDITION == DEBUG_VERSION
    PrintString(&shtinf, &desktop_sheet_id, fbc_addr->horizontal_resolution - 64, fbc_addr->vertical_resolution - 40,
                "X:", WHITE);
    PrintString(&shtinf, &desktop_sheet_id, fbc_addr->horizontal_resolution - 64, fbc_addr->vertical_resolution - 24,
                "Y:", WHITE);
    PrintDec(&shtinf, &desktop_sheet_id, fbc_addr->horizontal_resolution - 48, fbc_addr->vertical_resolution - 40,
             shtinf.scrx, WHITE);
    PrintDec(&shtinf, &desktop_sheet_id, fbc_addr->horizontal_resolution - 48, fbc_addr->vertical_resolution - 24,
             shtinf.scry, WHITE);
    PrintString(&shtinf, &desktop_sheet_id, 8, 34, "KeyboardInputOut", WHITE);
    // PrintString(&shtinf, &desktop_sheet_id, 332, 34, "PointerInputOut", WHITE);
    PrintString(&shtinf, &desktop_sheet_id, 8, 480, "MapKey:", WHITE);
    PrintHex(&shtinf, &desktop_sheet_id, 8, 496, BC->MemoryMap.MapKey, WHITE);
    // 别问为什么这么多-40，问就是因为改起来太麻烦
    PrintString(&shtinf, &desktop_sheet_id, 8, 284, "PrintTestColor", WHITE);

    draw_rect(&shtinf, &desktop_sheet_id, 480 - 472, 300, 500 - 473, 319, WHITE);
    draw_rect(&shtinf, &desktop_sheet_id, 500 - 472, 300, 520 - 473, 319, BLACK);
    draw_rect(&shtinf, &desktop_sheet_id, 520 - 472, 300, 540 - 473, 319, XINGJI_BLUE);
    draw_rect(&shtinf, &desktop_sheet_id, 540 - 472, 300, 560 - 473, 319, XINGJI_YELLOW);
    draw_rect(&shtinf, &desktop_sheet_id, 560 - 472, 300, 580 - 473, 319, WIN_BLUE);
    draw_rect(&shtinf, &desktop_sheet_id, 580 - 472, 300, 600 - 473, 319, GRAY);
    draw_rect(&shtinf, &desktop_sheet_id, 600 - 472, 300, 620 - 473, 319, BGRAY);
    draw_rect(&shtinf, &desktop_sheet_id, 620 - 472, 300, 640 - 473, 319, PURPLE);
    draw_rect(&shtinf, &desktop_sheet_id, 640 - 472, 300, 660 - 473, 319, PINK);
    draw_rect(&shtinf, &desktop_sheet_id, 660 - 472, 300, 680 - 473, 319, RED);
    draw_rect(&shtinf, &desktop_sheet_id, 680 - 472, 300, 700 - 473, 319, CHINA_RED);
    draw_rect(&shtinf, &desktop_sheet_id, 700 - 472, 300, 720 - 473, 319, GOLD);
    draw_rect(&shtinf, &desktop_sheet_id, 720 - 472, 300, 740 - 473, 319, GREEN);
    draw_rect(&shtinf, &desktop_sheet_id, 740 - 472, 300, 760 - 473, 319, LIGHT_GREEN);
    draw_rect(&shtinf, &desktop_sheet_id, 760 - 472, 300, 780 - 473, 319, BLUE);
    draw_rect(&shtinf, &desktop_sheet_id, 780 - 472, 300, 800 - 473, 319, SKY_BLUE);

    PrintString(&shtinf, &desktop_sheet_id, 8, 332, "MemoryMapTypeSheet", WHITE);
    PrintString(&shtinf, &desktop_sheet_id, 8, 348, "0x0: Free", WHITE);
    PrintString(&shtinf, &desktop_sheet_id, 8, 364, "0xB: EFI Code & Data", WHITE);

    PrintString(&shtinf, &desktop_sheet_id, 8, 400, "SheetManagerTest", WHITE);
    PrintDec(&shtinf, &desktop_sheet_id, 8, 416, (uint64_t)desktop_sheet_id, WHITE);
    // PrintDec(&shtinf, &desktop_sheet_id, 164, 416, (uint64_t)*shtinf.sheet[0].number, WHITE);

    PrintHex(&shtinf, &desktop_sheet_id, 8, 432, (uint64_t)shtinf.temp_buffer, WHITE);
    PrintDec(&shtinf, &desktop_sheet_id, 164, 432, (uint64_t)shtinf.sheet_num, WHITE);

    PrintHex(&shtinf, &desktop_sheet_id, 8, 448, 0xFEDCBA9876543210, WHITE);
    // PrintHex(&shtinf, &desktop_sheet_id, 164, 448, (uint64_t)shtinf.sheet[255].buffer, WHITE);

    // CPU型号
    char CPU_Model[48];
    get_cpu_name(CPU_Model);
    PrintString(&shtinf, &desktop_sheet_id, 8, 528, "CPU Model:", WHITE);
    PrintString(&shtinf, &desktop_sheet_id, 96, 528, CPU_Model, WHITE);
    write_serial_string("CPU Model: ");
    write_serial_string(CPU_Model);
    write_serial_string("\n");

    draw_rect(&shtinf, &desktop_sheet_id, 440, fbc_addr->vertical_resolution - 70, 479,
              fbc_addr->vertical_resolution - 21, {0x00, 0xa2, 0xe8, 0xff});
#endif
    // wsod_debug();

    print_box_ttf(&shtinf, &desktop_sheet_id, (char *)"MAKE XJ380 GREAT AGAIN! 实现XJ380的伟大复兴！", WHITE, 64, 64,
                  16);

    draw_mouse(&shtinf, &mouse_sheet_id);
    XWM_INFO  xwmi;
    WINDOWLS *test_win;
    WINDOWLS *test_win2;
    init_xwm(&xwmi);
    xwmii = &xwmi;

    winRD_lock = true;

    create_window(&xwmi, &shtinf, &test_win, (char *)"114514", 320, 180);
    create_window(&xwmi, &shtinf, &test_win2, (char *)"窗口", 800, 600);

    // delete_window(&xwmi, &shtinf, test_win);

    refresh_sheet(&shtinf); // 刷新
    // delete_sheet(&shtinf, &dock_sheet_id);

    uint8_t k_input;
    int     tby;
    int     tbx;

    // struct EFI_RUNTIME_SERVICES *RuntimeServices = (struct EFI_RUNTIME_SERVICES *)convert_physical_to_virtual((uint64_t)(ST->RuntimeServices));
    // EFIAPI EFI_STATUS (*GetTime)(EFI_TIME *, EFI_TIME_CAPABILITIES *) = (EFIAPI EFI_STATUS(*)(EFI_TIME *, EFI_TIME_CAPABILITIES *))convert_physical_to_virtual((uint64_t)RuntimeServices->GetTime);
    int scdx = fbc_addr->horizontal_resolution * 0.8125;
    tm  time;

    while (1)
    {
        enable_intr();

        if (ms_dec.need_flush)
        {
            tbx         = getBX(&shtinf, &mouse_sheet_id);
            tby         = getBY(&shtinf, &mouse_sheet_id);
            int mouse_x = get_mouse_x();
            int mouse_y = get_mouse_y();
            move_mouse(&shtinf, &mouse_sheet_id, mouse_x, mouse_y);
            refresh_part_sheet(&shtinf, tbx, tby, tbx + getXsize(&shtinf, &mouse_sheet_id),
                               tby + getYsize(&shtinf, &mouse_sheet_id));
            refresh_part_sheet(&shtinf, getBX(&shtinf, &mouse_sheet_id), getBY(&shtinf, &mouse_sheet_id),
                               getBX(&shtinf, &mouse_sheet_id) + getXsize(&shtinf, &mouse_sheet_id),
                               getBY(&shtinf, &mouse_sheet_id) + getYsize(&shtinf, &mouse_sheet_id));
            process_mouse_info();
        }

        k_input = get_keyboard_input();
        if (k_input != NULL && k_input != KEY_ENTER && k_input != KEY_SHIFT && k_input != KEY_CTRL &&
            k_input != KEY_CAPS && k_input != KEY_BACKSPACE)
        {
            PrintFont(&shtinf, &desktop_sheet_id, fontx, fonty, k_input, WHITE);
            refresh_part_sheet(&shtinf, fontx, fonty, fontx + 8, fonty + 16); // 刷新
            fontx += 8;
        }
        else if (k_input == KEY_ENTER)
        {
            fontx  = 8;
            fonty += 16;
        }
        if (fontx > 304)
        {
            fontx  = 8;
            fonty += 16;
        }
        time_read(&time);
        draw_rect(&shtinf, &dock_sheet_id, scdx - 44, 0, scdx - 3, 23, DOCK_COL);
        //12 小时制
        PrintDec(&shtinf, &dock_sheet_id, scdx - 44, 4, (time.tm_hour + 8) % 24, BLACK);
        PrintString(&shtinf, &dock_sheet_id, scdx - 28, 4, ":", BLACK);
        PrintDec(&shtinf, &dock_sheet_id, scdx - 20, 4, time.tm_min, BLACK);
        refresh_part_sheet(&shtinf, scdx - 44, 0, scdx, 24);
    }
}

extern "C" void KernelMain(const FrameBufferConfig &fbc, EFI_SYSTEM_TABLE &SystemTable, BOOT_CONFIG &BootConfig)
{
    fbc_addr = &fbc;
    (void)SystemTable; // 暂时不用
    no_interrupt = true;
    disable_intr();
    init_cpu();
    init_idt();
    init_gdt();
    init_hpet(BootConfig.HPET);
    init_apic(BootConfig.MADT);
    init_hhdm();
    init_frame(BootConfig.MemoryMap);
    page_setup();
    init_heap();

    serial_wprintf("Output by Serial Writer!!\n");

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
    f = vfs_open("/1.txt");
    char buf[1024];
    if (vfs_read(f, buf, 0, f->size) == -1)
        write_serial_fmt("read failed\n");
    else
        write_serial_fmt("read file: %s\n", buf);

    // 超绝偷懒解法，等有内存管理且在这之前初始化的时候叫我来改
    saved_mtrrs = (uint64_t *)(BootConfig.saved_mtrrs);
    for (int i = 0; i < MAX_CPU_NUM; ++i)
        temp_stack[i] = BootConfig.temp_stack[i];

    init_smp(BootConfig.MADT);

    uint64_t phys = alloc_frames(2);
    write_serial_string("Alloc memory: ");
    write_serial_hex(phys);
    write_serial_string("\n");

    init_syscall();

    process_setup();

    create_kernel_thread((void *)desktop_flusher, NULL, (char *)"desktop_flusher", NULL);

    pcb_t ugp = create_process_group("User Group", NULL, NULL);
    uint64_t utsk = page_alloc_random(get_current_directory(), 114, PTE_PRESENT | PTE_USER);
    memcpy((void *)utsk, (void *)test_task, 114);
    create_user_thread((void *)test_task, NULL, (char *)"test_task2", ugp);

    open_interrupt;
    no_interrupt = false;
    enable_scheduler();

    while (true)
    {
        __asm__ __volatile__("pause");
    }
}
