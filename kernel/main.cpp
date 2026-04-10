#include "build_settings.h"
#include <ahci/ahci.h>
#include <cpu/regio.h>
#include <device.h>
#include <cpu/fsgsbase.h>
#include <dlinker.h>
#include <efi/boot.h>
#include <efi/efi.h>
#include <efi/fbc.h>
#include <font.h>
#include <fs/fatfs/fatfs.h>
#include <fs/partition.h>
#include <fs/vfs/devfs.h>
#include <fs/vfs/sys.h>
#include <fs/vfs/vfs.h>
#include <global_color.h>
#include <graphics/GOP.hpp>
#include <graphics/sheet.h>
#include <hda/hda.h>
#include <hda/pcspk.h>
#include <krlibc.h>
#include <mm/alloc/alloc.h>
#include <mm/frame.h>
#include <nvme/nvme.h>
#include <pci/pci.h>
#include <pipe.h>
#include <power.h>
#include <proto.hpp>
#include <ps2/keyboard.h>
#include <ps2/mouse.h>
#include <rtc.h>
#include <sb16.h>
#include <syscall/signal.h>
#include <syscall/syscall.h>
#include <task/pcb.h>
#include <ttf.h>
#include <user/runfile.h>

bool        desktop_done = false;
SHEET_INFO *sht_img;
XWM_INFO   *xwmii;
SHEET      *msid;

extern mouse_dec     ms_dec;
extern bool          winRD_lock;
extern int           scheduler_is_ready;
extern XSK_SMP_INFO *xsi;
size_t               tid;
#define NULL 0
const FrameBufferConfig *fbc_addr;
uint64_t                *saved_mtrrs;
void                    *temp_stack[MAX_CPU_NUM];
bool                     no_interrupt = false; // 为true时代表严禁开启中断
extern spin_t            fb_print_lock;

extern bool allow_to_flush;
extern void ahci_set_accel(bool enabled);
extern bool ahci_is_qemu_environment();

extern UserInfo *current_user;

static tcb_t alloc_zeroed_tcb(void)
{
    const size_t alloc_size = (sizeof(struct thread_control_block) + 15ULL) & ~15ULL;
    tcb_t        task = (tcb_t)aligned_alloc(16, alloc_size);
    if (task != NULL) memset(task, 0, sizeof(struct thread_control_block));
    return task;
}

static inline SHEET_BUFFER boot_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 0xff)
{
    return {r, g, b, a};
}

static void play_boot_animation(SHEET_INFO *sht, SHEET *csheet)
{
    static char kBootTitle[] = "XJ380";
    static char kBootSubtitle[] = "Starting desktop services";
    static char kBootStatus[] = "Loading shell, desktop and user services";

    const int scrx = (int)sht->scrx;
    const int scry = (int)sht->scry;
    const int frames = 18;
    const int logo_w = 84;
    const int logo_h = 64;
    const int title_size = scrx >= 1280 ? 28 : 24;
    const int subtitle_size = 12;

    int card_w = scrx - 96;
    if (card_w > 560) card_w = 560;
    if (card_w < 280) card_w = scrx - 32;

    int card_h = 210;
    if (card_h > scry - 40) card_h = scry - 40;

    const int card_x = (scrx - card_w) / 2;
    int card_y = (scry - card_h) / 2 - 20;
    if (card_y < 20) card_y = 20;

    const uint32_t title_width = (uint32_t)calc_ttf_length(kBootTitle, title_size);
    const uint32_t subtitle_width = (uint32_t)calc_ttf_length(kBootSubtitle, subtitle_size);

    for (int frame = 0; frame < frames; ++frame) {
        const int progress = (card_w - 72) * (frame + 1) / frames;
        const int logo_y = card_y + 28 + (frames - frame - 1) / 3;
        const int shine_w = 38;
        int shine_x = card_x + 36 + progress - shine_w / 2;
        if (shine_x < card_x + 36) shine_x = card_x + 36;
        if (shine_x + shine_w > card_x + card_w - 36) {
            shine_x = card_x + card_w - 36 - shine_w;
        }

        draw_rect(sht, csheet, 0, 0, scrx - 1, scry - 1, boot_color(8, 20, 40));
        draw_rect(sht, csheet, 0, scry * 2 / 3, scrx - 1, scry - 1, boot_color(12, 30, 56));

        draw_rect(sht, csheet, card_x + 8, card_y + 10, card_x + card_w + 7, card_y + card_h + 9,
                  boot_color(5, 12, 26));
        draw_rect(sht, csheet, card_x, card_y, card_x + card_w - 1, card_y + card_h - 1,
                  boot_color(238, 244, 250));
        draw_rect(sht, csheet, card_x, card_y, card_x + card_w - 1, card_y + 5, boot_color(0, 162, 232));

        draw_logo(sht, csheet, card_x + (card_w - logo_w) / 2, logo_y);
        print_box_ttf(sht, csheet, kBootTitle, boot_color(26, 36, 50),
                      card_x + (card_w - (int)title_width) / 2, card_y + 105, title_size);
        print_box_ttf(sht, csheet, kBootSubtitle, boot_color(86, 102, 120),
                      card_x + (card_w - (int)subtitle_width) / 2, card_y + 142, subtitle_size);
        print_box_ttf(sht, csheet, kBootStatus, boot_color(86, 102, 120), card_x + 36, card_y + card_h - 64, 11);

        draw_rect(sht, csheet, card_x + 36, card_y + card_h - 34, card_x + card_w - 37, card_y + card_h - 19,
                  boot_color(196, 208, 220));
        draw_rect(sht, csheet, card_x + 36, card_y + card_h - 34, card_x + 35 + progress, card_y + card_h - 19,
                  boot_color(0, 162, 232));
        draw_rect(sht, csheet, shine_x, card_y + card_h - 34, shine_x + shine_w, card_y + card_h - 19,
                  boot_color(255, 255, 255));

        refresh_sheet(sht);
        delay_ms(16);
    }
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

typedef struct TimerWidgetsData
{
    SHEET_INFO *sht;
    SHEET      *ct_sheet;
    tm          time;
    tm          old_time;
    int         scdx;
    uint8_t     flushing;
    uint8_t     need_flush;
} TimerWidgetsData;

TimerWidgetsData *tw_data = NULL;

bool have_full_screen_app = false;

int clock_hour_offset = 0;

void init_time()
{
    if (current_user == NULL) return;

    char setfile_path[256];
    memset(setfile_path, 0, 256);
    strcat(setfile_path, "/users/");
    strcat(setfile_path, current_user->name);
    strcat(setfile_path, "/settings.dat");
    vfs_node_t v = vfs_open(setfile_path);
    if (!v) return;

    SettingsDataFileFormat sdff;
    memset(&sdff, 0, sizeof(sdff));
    size_t read_size = v->size < sizeof(sdff) ? (size_t)v->size : sizeof(sdff);
    if (read_size == 0 || vfs_read(v, &sdff, 0, read_size) == -1)
    {
        vfs_close(v);
        return;
    }

    clock_hour_offset = sdff.ClockHourOffset;

    if (clock_hour_offset < 0)
        clock_hour_offset = 0;

    vfs_close(v);
}

void components_flusher()
{
    while (true)
    {
        if (allow_to_flush) break;
    }
    write_serial_string("components flusher is running\n");
    while (true)
    {
        if (!no_interrupt)
        {
            enable_intr();
            enable_scheduler();
        }
        if (tw_data != NULL && !have_full_screen_app)
        {
            tm         *time          = &tw_data->time;
            tm         *old_time      = &tw_data->old_time;
            SHEET_INFO *shtinf        = tw_data->sht;
            SHEET      *dock_ct_sheet = tw_data->ct_sheet;
            int         scdx          = tw_data->scdx;
            time_read(time);

            if ((old_time->tm_min != time->tm_min || old_time->tm_hour != time->tm_hour) && !tw_data->need_flush &&
                !tw_data->flushing)
            {
                tw_data->flushing = true;
                *old_time         = *time;
                draw_rect(shtinf, dock_ct_sheet, scdx - 40, 0, scdx - 3, 23, DOCK_COL);
                // 12 小时制
                print_fmt_box_ttf(shtinf, dock_ct_sheet, BLACK, scdx - 40, 1, 10, "%02u:%02u", time->tm_hour % 24,
                                  time->tm_min);
                tw_data->flushing   = false;
                tw_data->need_flush = true;
            }
        }
        if (ms_dec.need_flush) { process_mouse_info(); }
        if (sht_img != NULL) { flush_sheet_damage_queue(sht_img); }
    }
}

extern SHEET *mouse_ct_sheet_img;
SHEET        *desktop_ct_sheet; // 如果SheetTest那行输出的是1145的话那么绝对有问题
SHEET        *dock_ct_sheet;
SHEET        *mouse_ct_sheet;
SHEET *zhe_shi_yi_ge_sha_bi_dao_ji_zhi_de_cao_zuo_da_jia_bu_yao_xue___GuoqiFish_is_shabi_and_this_var_is_cao_gao_zhi;

void desktop_flusher()
{
    no_interrupt = true;
    disable_intr();
    disable_scheduler();

    // 之后应该挪到OOBE里的部分
    init_runfile();

    write_serial_string("desktop flusher is running\n");

    SHEET_INFO shtinf;
    init_sheet(*fbc_addr, &shtinf);

    sht_img = &shtinf;
    msid    = mouse_ct_sheet;

    create_sheet(&shtinf, 0, 0, fbc_addr->horizontal_resolution, fbc_addr->vertical_resolution, FixedSheetType, 0,
                 &desktop_ct_sheet);
    create_sheet(&shtinf, 0, 0, fbc_addr->horizontal_resolution, fbc_addr->vertical_resolution, FixedSheetType, 9999,
                 &dock_ct_sheet);
    create_sheet(&shtinf, (get_hor() - 16) / 2, (get_ver() - 28 - 29) / 2, 10, 21, FixedSheetType, 10000,
                 &mouse_ct_sheet);

    // 草稿纸吗，有点意思
    create_sheet(
        &shtinf, get_hor() + 114, get_ver() + 114, 1000, 1000, FixedSheetType, -1,
        &zhe_shi_yi_ge_sha_bi_dao_ji_zhi_de_cao_zuo_da_jia_bu_yao_xue___GuoqiFish_is_shabi_and_this_var_is_cao_gao_zhi);

    init_ttf();
    play_boot_animation(&shtinf, desktop_ct_sheet);

    rect(*fbc_addr, 0, 0, int(fbc_addr->horizontal_resolution) / 10 * 9, 7, {0x00, 0xa2, 0xe8}); // GUI好了来一下

    int fontx = 8;
    int fonty = 50;
    // DEBUG
#if 1 == 2
    // #if BUILD_EDITION == DEBUG_VERSION
    PrintString(&shtinf, &desktop_ct_sheet, fbc_addr->horizontal_resolution - 64, fbc_addr->vertical_resolution - 40,
                "X:", WHITE);
    PrintString(&shtinf, &desktop_ct_sheet, fbc_addr->horizontal_resolution - 64, fbc_addr->vertical_resolution - 24,
                "Y:", WHITE);
    PrintDec(&shtinf, &desktop_ct_sheet, fbc_addr->horizontal_resolution - 48, fbc_addr->vertical_resolution - 40,
             shtinf.scrx, WHITE);
    PrintDec(&shtinf, &desktop_ct_sheet, fbc_addr->horizontal_resolution - 48, fbc_addr->vertical_resolution - 24,
             shtinf.scry, WHITE);
    PrintString(&shtinf, &desktop_ct_sheet, 8, 34, "KeyboardInputOut", WHITE);
    // PrintString(&shtinf, &desktop_ct_sheet, 332, 34, "PointerInputOut", WHITE);
    // PrintString(&shtinf, &desktop_ct_sheet, 8, 480, "MapKey:", WHITE);
    // PrintHex(&shtinf, &desktop_ct_sheet, 8, 496, BC->MemoryMap.MapKey, WHITE);
    // 别问为什么这么多-40，问就是因为改起来太麻烦
    PrintString(&shtinf, &desktop_ct_sheet, 8, 284, "PrintTestColor", WHITE);

    draw_rect(&shtinf, &desktop_ct_sheet, 480 - 472, 300, 500 - 473, 319, WHITE);
    draw_rect(&shtinf, &desktop_ct_sheet, 500 - 472, 300, 520 - 473, 319, BLACK);
    draw_rect(&shtinf, &desktop_ct_sheet, 520 - 472, 300, 540 - 473, 319, XINGJI_BLUE);
    draw_rect(&shtinf, &desktop_ct_sheet, 540 - 472, 300, 560 - 473, 319, XINGJI_YELLOW);
    draw_rect(&shtinf, &desktop_ct_sheet, 560 - 472, 300, 580 - 473, 319, WIN_BLUE);
    draw_rect(&shtinf, &desktop_ct_sheet, 580 - 472, 300, 600 - 473, 319, GRAY);
    draw_rect(&shtinf, &desktop_ct_sheet, 600 - 472, 300, 620 - 473, 319, BGRAY);
    draw_rect(&shtinf, &desktop_ct_sheet, 620 - 472, 300, 640 - 473, 319, PURPLE);
    draw_rect(&shtinf, &desktop_ct_sheet, 640 - 472, 300, 660 - 473, 319, PINK);
    draw_rect(&shtinf, &desktop_ct_sheet, 660 - 472, 300, 680 - 473, 319, RED);
    draw_rect(&shtinf, &desktop_ct_sheet, 680 - 472, 300, 700 - 473, 319, CHINA_RED);
    draw_rect(&shtinf, &desktop_ct_sheet, 700 - 472, 300, 720 - 473, 319, GOLD);
    draw_rect(&shtinf, &desktop_ct_sheet, 720 - 472, 300, 740 - 473, 319, GREEN);
    draw_rect(&shtinf, &desktop_ct_sheet, 740 - 472, 300, 760 - 473, 319, LIGHT_GREEN);
    draw_rect(&shtinf, &desktop_ct_sheet, 760 - 472, 300, 780 - 473, 319, BLUE);
    draw_rect(&shtinf, &desktop_ct_sheet, 780 - 472, 300, 800 - 473, 319, SKY_BLUE);

    PrintString(&shtinf, &desktop_ct_sheet, 8, 332, "MemoryMapTypeSheet", WHITE);
    PrintString(&shtinf, &desktop_ct_sheet, 8, 348, "0x0: Free", WHITE);
    PrintString(&shtinf, &desktop_ct_sheet, 8, 364, "0xB: EFI Code & Data", WHITE);

    PrintString(&shtinf, &desktop_ct_sheet, 8, 400, "SheetManagerTest", WHITE);
    PrintDec(&shtinf, &desktop_ct_sheet, 8, 416, (uint64_t)desktop_ct_sheet, WHITE);
    // PrintDec(&shtinf, &desktop_ct_sheet, 164, 416, (uint64_t)*shtinf.sheet[0].number, WHITE);

    PrintHex(&shtinf, &desktop_ct_sheet, 8, 432, (uint64_t)shtinf.temp_buffer, WHITE);
    PrintDec(&shtinf, &desktop_ct_sheet, 164, 432, (uint64_t)shtinf.sheet_num, WHITE);

    PrintHex(&shtinf, &desktop_ct_sheet, 8, 448, 0xFEDCBA9876543210, WHITE);
    // PrintHex(&shtinf, &desktop_ct_sheet, 164, 448, (uint64_t)shtinf.sheet[255].buffer, WHITE);

    // CPU型号
    char CPU_Model[48];
    get_cpu_name(CPU_Model);
    PrintString(&shtinf, &desktop_ct_sheet, 8, 528, "CPU Model:", WHITE);
    PrintString(&shtinf, &desktop_ct_sheet, 96, 528, CPU_Model, WHITE);
    write_serial_string("CPU Model: ");
    write_serial_string(CPU_Model);
    write_serial_string("\n");

    print_fmt_box_ttf(&shtinf, &desktop_ct_sheet, WHITE, 64, 64, 16,
                      "MAKE \nXJ380 \nGREAT \nAGAIN! \n实现XJ380的伟大复兴！");
#endif
    // wsod_debug();

    // print_box_ttf(&shtinf, &desktop_ct_sheet, (char *)"MAKE XJ380 GREAT AGAIN! 实现XJ380的伟大复兴！", WHITE, 64, 64,
    //               16);

    draw_mouse(&shtinf, mouse_ct_sheet);
    XWM_INFO  xwmi;
    WINDOWLS *test_win;
    WINDOWLS *test_win2;
    init_xwm(&xwmi);
    xwmii = &xwmi;

    rect(*fbc_addr, 0, 0, int(fbc_addr->horizontal_resolution) / 10 * 10, 7, {0x00, 0xa2, 0xe8}); // 全都好了来一下

    mouse_ct_sheet_img = mouse_ct_sheet;

    no_interrupt = false;
    enable_intr();
    enable_scheduler();
    allow_to_flush = true;

    write_serial_string("desktop: init_user begin\n");
    init_user();
    write_serial_string("desktop: init_user done\n");
    write_serial_string("desktop: init_xuls begin\n");
    init_xuls();
    write_serial_string("desktop: init_xuls done\n");
    init_time();

    write_serial_string("user & settings initialized.\n");

    init_desktop(&shtinf, desktop_ct_sheet, dock_ct_sheet);

    write_serial_string("desktop initialized.\n");

    winRD_lock = true;

    refresh_sheet(&shtinf); // 刷新
    flush_sheet_damage_queue(&shtinf);

    uint8_t k_input;

    // struct EFI_RUNTIME_SERVICES *RuntimeServices = (struct EFI_RUNTIME_SERVICES *)convert_physical_to_virtual((uint64_t)(ST->RuntimeServices));
    // EFIAPI EFI_STATUS (*GetTime)(EFI_TIME *, EFI_TIME_CAPABILITIES *) = (EFIAPI EFI_STATUS(*)(EFI_TIME *, EFI_TIME_CAPABILITIES *))convert_physical_to_virtual((uint64_t)RuntimeServices->GetTime);
    int scdx = fbc_addr->horizontal_resolution * 13 / 16;

    tw_data = (TimerWidgetsData *)malloc(sizeof(TimerWidgetsData));
    if (tw_data == NULL)
    {
        write_serial_string("timer widget allocation failed\n");
        desktop_done = true;
        goto skip_timer_widget_init;
    }
    memset(tw_data, 0, sizeof(TimerWidgetsData));
    tw_data->sht      = &shtinf;
    tw_data->ct_sheet = dock_ct_sheet;
    tw_data->scdx     = scdx;
    time_read(&tw_data->time);
    tw_data->old_time = tw_data->time;

    desktop_done = true;

skip_timer_widget_init:

    delay_s_hp(1);
    
    

    // char *argv[2] = {"/system/icon/folder.png", NULL};
    // create_user_process_from_file((char *)"/apps/init.elf", NULL, NULL);
    create_user_process_from_file((char *)"/system/fmanager.elf", NULL, NULL);
    // create_user_process_from_file((char *)"/system/picturer.elf", NULL, argv);
    // create_user_process_from_file((char *)"/system/shell.elf", NULL, NULL);
    // create_user_process_from_file((char *)"/system/ctrlmenu.elf", NULL, NULL);
    // create_user_process_from_file((char *)"/apps/cp537.elf", NULL, NULL);
    // create_user_process_from_file((char *)"/apps/bcms-sp.elf", NULL, NULL);

    while (1)
    {
        if (!no_interrupt)
        {
            enable_intr();
            enable_scheduler();
        }
        if (tw_data->need_flush)
        {
            tw_data->need_flush = false;
            refresh_part_sheet(&shtinf, scdx - 40, 0, scdx, 24); // 刷新时间
        }

        k_input = get_keyboard_input();
        if (k_input != NULL)
        {
            WINDOWLSP current_window = sht_found_win(xwmii, sht_img, ms_dec.sht_now);
            if (current_window != NULL)
            {
                if (k_input != '\n' && k_input != '\b' && k_input < 128)
                {
                    do_message(MSG_CHAR, NULL, k_input, current_window->WinMPf, current_window->w_task);
                }
                else
                {
                    do_message(MSG_SPCHAR, NULL, k_input, current_window->WinMPf, current_window->w_task);
                }
            }
        }

#if BUILD_EDITION == DEBUG_VERSION
        if (k_input != NULL && k_input != '\n' && k_input != '\b')
        {
            PrintFont(&shtinf, desktop_ct_sheet, fontx, fonty, k_input, WHITE);
            refresh_part_sheet(&shtinf, fontx, fonty, fontx + 8, fonty + 16); // 刷新
            fontx += 8;
        }
        else if (k_input == '\n')
        {
            fontx  = 8;
            fonty += 16;
        }
        if (fontx > 304)
        {
            fontx  = 8;
            fonty += 16;
        }
#endif
        flush_sheet_damage_queue(&shtinf);
    }
}
#define KERNEL_HEAP_START 0xffffc00000000000
#define KERNEL_HEAP_SIZE  (256 * 1024 * 1024)

void init_heap()
{
    page_map_range_to_random(get_kernel_pagedir(), KERNEL_HEAP_START, KERNEL_HEAP_SIZE, KERNEL_PTE_FLAGS);
    memset((void *)KERNEL_HEAP_START, 0, KERNEL_HEAP_SIZE);
    heap_init((uint8_t *)KERNEL_HEAP_START, KERNEL_HEAP_SIZE);
}

extern size_t   now_tid;
extern void     nvme_setup();
extern void     mount_root();
extern void     stdio_init();
extern int      socketfs_setup();
extern int      dnsfs_setup();
extern int      nmfs_setup();
EFI_SYSTEM_TABLE *EFI_ST;
BOOT_CONFIG *EFI_BC;

extern "C" void KernelMain(const FrameBufferConfig &fbc, EFI_SYSTEM_TABLE &SystemTable, BOOT_CONFIG &BootConfig)
{
    fbc_addr = &fbc;
    no_interrupt = true;
    disable_intr();
    disable_scheduler();

    EFI_ST = &SystemTable;
    EFI_BC = &BootConfig;

    rect(*fbc_addr, 0, 0, int(fbc_addr->horizontal_resolution), 7, {0xe6, 0xe6, 0xe6}); // 开机进度条

    init_cpu();
    init_idt();
    init_gdt();
    init_hpet(BootConfig.HPET);
    init_apic(BootConfig.MADT);

    rect(*fbc_addr, 0, 0, int(fbc_addr->horizontal_resolution) / 10 * 1, 7, {0x00, 0xa2, 0xe8}); // CPU好了来一下

    init_hhdm();
    init_frame(BootConfig.MemoryMap);
    init_heap();

    rect(*fbc_addr, 0, 0, int(fbc_addr->horizontal_resolution) / 10 * 3, 7, {0x00, 0xa2, 0xe8}); // 内存好了来一下

    write_serial_string("BOOT: device_manager_init begin\n");
    device_manager_init();
    write_serial_string("BOOT: device_manager_init done\n");
    write_serial_string("BOOT: vfs_init begin\n");
    vfs_init();
    write_serial_string("BOOT: vfs_init done\n");
    write_serial_string("BOOT: fatfs_init begin\n");
    fatfs_init();
    write_serial_string("BOOT: fatfs_init done\n");
    write_serial_string("BOOT: devfs_setup begin\n");
    devfs_setup();
    write_serial_string("BOOT: devfs_setup done\n");

    write_serial_string("BOOT: pci_setup begin\n");
    pci_setup(BootConfig.MCFG);
    write_serial_string("BOOT: pci_setup done\n");
    write_serial_string("BOOT: pci_init begin\n");
    pci_init();
    write_serial_string("BOOT: pci_init done\n");
    write_serial_fmt("VM hint: %s\n", BootConfig.is_qemu == 1 ? "QEMU/BOCHS" : "generic/VMware/real hardware");
    ahci_set_environment(BootConfig.is_qemu == 1);
    write_serial_string("BOOT: ahci_setup begin\n");
    ahci_setup();
    write_serial_string("BOOT: ahci_setup done\n");
    ahci_set_accel(false);
    write_serial_string("BOOT: nvme_setup begin\n");
    nvme_setup();
    write_serial_string("BOOT: nvme_setup done\n");
    
    extern void ide_setup(void);
    write_serial_string("BOOT: ide_setup begin\n");
    ide_setup();
    write_serial_string("BOOT: ide_setup done\n");
    write_serial_string("BOOT: partition_init begin\n");
    partition_init();
    write_serial_string("BOOT: partition_init done\n");
    // enable_intr();

    // disable_intr();
    // hda_init();
    // sb16_init();

    keyboard_init();
    mouse_init();

    rect(*fbc_addr, 0, 0, int(fbc_addr->horizontal_resolution) / 10 * 5, 7, {0x00, 0xa2, 0xe8}); // 驱动好了来一下

    disable_intr();

    process_setup();

    // 超绝偷懒解法，等有内存管理且在这之前初始化的时候叫我来改
    saved_mtrrs = (uint64_t *)((uint64_t)BootConfig.saved_mtrrs + 0xffff800000000000);
    for (int i = 0; i < MAX_CPU_NUM; ++i)
        temp_stack[i] = BootConfig.temp_stack[i];

    init_smp(BootConfig.MADT);

    tcb_t idle_thread         = alloc_zeroed_tcb();
    if (idle_thread == NULL) while (true) { __asm__ volatile("hlt"); }
    idle_thread->tid          = now_tid++;
    idle_thread->parent_group = kernel_group;
    idle_thread->kernel_stack = get_rsp();
    idle_thread->status       = RUNNING;
    strcpy(idle_thread->name, "idle thread");
    idle_thread->group_index  = queue_enqueue(kernel_group->thread_queue, idle_thread);
    idle_thread->context0.rsp = get_rsp();
    set_kernel_stack(get_rsp());
    idle_thread->queue_index = queue_enqueue(get_current_cpu()->scheduler_queue, idle_thread);

    idle_thread->str_cwd = "/";
    idle_thread->cwd     = rootdir;
    idle_thread->cwd->refcount++;
    save_fpu_context(&idle_thread->fpu_context);

    get_current_cpu()->current_task = idle_thread;
    write_kgsbase((uint64_t)get_current_cpu());

    memset(phys_to_virt(get_cr3()), 0, PAGE_SIZE / 2);

    disable_scheduler();

    rect(*fbc_addr, 0, 0, int(fbc_addr->horizontal_resolution) / 20 * 11, 7, {0x00, 0xa2, 0xe8}); // 多任务好了来一下

    mount_root();
    pipefs_setup();
    socketfs_setup();
    dnsfs_setup();
    nmfs_setup();
    stdio_init();

    signal_init();
    init_syscall();
    init_message();

    module_setup();
    dlinker_init();
    load_all_kernel_module();

    rect(*fbc_addr, 0, 0, int(fbc_addr->horizontal_resolution) / 10 * 6, 7, {0x00, 0xa2, 0xe8}); // 驱动好了来一下

    while (true)
    {
        __asm__ volatile("pause");
        if (scheduler_is_ready == xsi->cpu_count) break;
    }

    rect(*fbc_addr, 0, 0, int(fbc_addr->horizontal_resolution) / 10 * 7, 7, {0x00, 0xa2, 0xe8}); // 驱动好了来一下

    init_killer();
    write_serial_string("Process Killer is ready\n");
    

    if (BootConfig.is_qemu == 1) pr_warn("XJ380 is running under the QEMU/BOCHS.\n");

    // create_kernel_thread((void *)tst, NULL, (char *)"1", NULL);
    if (ahci_is_qemu_environment()) {
        ahci_set_accel(true);
    } else {
        write_serial_string("AHCI: keeping conservative IO path on vmware/real hardware\n");
    }
    create_kernel_thread((void *)desktop_flusher, NULL, (char *)"desktop_flusher", NULL);
    // delay_s_hp(60);

    // uint64_t utsk = page_alloc_random(get_current_directory(), 114, PTE_PRESENT | PTE_USER);
    // memcpy((void *)utsk, (void *)test_task, 114);
    // create_user_thread((void *)utsk, NULL, (char *)"test_task2", ugp);


    enable_scheduler();
    open_interrupt;
    no_interrupt = false;

    while (!desktop_done)
        cpu_relax();

    no_interrupt = true;
    close_interrupt;
    disable_scheduler();

    create_kernel_thread((void *)components_flusher, NULL, (char *)"components_flusher", NULL);

    enable_scheduler();
    open_interrupt;
    no_interrupt = false;

    while (true)
    {
        if (!no_interrupt)
        {
            enable_intr();
            enable_scheduler();
        }

        __asm__ __volatile__("pause");
    }
}
