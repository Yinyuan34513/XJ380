#include "build_settings.h"
#include <ahci/ahci.h>
#include <cpu/regio.h>
#include <efi/boot.h>
#include <efi/efi.h>
#include <efi/fbc.h>
#include <font.h>
#include <fs/fatfs/fatfs.h>
#include <syscall/signal.h>
#include <fs/partition.h>
#include <fs/vfs/devfs.h>
#include <fs/vfs/sys.h>
#include <fs/vfs/vfs.h>
#include <global_color.h>
#include <graphics/GOP.hpp>
#include <graphics/sheet.h>
#include <krlibc.h>
#include <mm/frame.h>
#include <nvme/nvme.h>
#include <pci/pci.h>
#include <proto.hpp>
#include <ps2/keyboard.h>
#include <ps2/mouse.h>
#include <rtc.h>
#include <syscall/syscall.h>
#include <task/pcb.h>
#include <ttf.h>
#include <usb/hid.h>
#include <xhci/xhci-hcd.h>
#include <hda/hda.h>
#include <hda/pcspk.h>
#include <device.h>
#include <sb16.h>
#include <fs/vfs/proc.h>
#include <pipe.h>

bool        desktop_done = false;
bool        xhci_done    = false;
SHEET_INFO *sht_img;
XWM_INFO   *xwmii;
uint16_t   *msid;

extern mouse_dec     ms_dec;
extern bool          winRD_lock;
extern int           scheduler_is_ready;
extern XSK_SMP_INFO *xsi;
size_t               tid;
#define NULL 0
const FrameBufferConfig *fbc_addr;
EFI_SYSTEM_TABLE        *ST;
BOOT_CONFIG             *BC;
uint64_t                *saved_mtrrs;
void                    *temp_stack[MAX_CPU_NUM];
bool                     no_interrupt = false; // 为true时代表严禁开启中断
extern spin_t            fb_print_lock;

extern bool allow_to_flush;

extern UserInfo *current_user;

void test_task()
{
    uint64_t ret;
    uint64_t syscall_number = 1145;
    uint64_t arg1;
    uint64_t arg2;
    uint64_t arg3;
    uint64_t arg4;
    uint64_t arg5;
    uint64_t arg6;
    __asm__ __volatile__("movq %2, %%rdi\n\t"
                         "movq %3, %%rsi\n\t"
                         "movq %4, %%rdx\n\t"
                         "movq %5, %%r10\n\t"
                         "movq %6, %%r8\n\t"
                         "movq %7, %%r9\n\t"
                         "syscall\n\t"
                         : "=a"(ret)
                         : "a"(syscall_number), "r"(arg1), "r"(arg2), "r"(arg3), "r"(arg4), "r"(arg5), "r"(arg6)
                         : "memory", "rdi", "rsi", "rdx", "r10", "r8", "r9");

    while (1)
        ;
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
    uint16_t   *sheet_id;
    tm          time;
    tm          old_time;
    int         scdx;
    uint8_t     flushing;
    uint8_t     need_flush;
} TimerWidgetsData;

TimerWidgetsData *tw_data = NULL;

bool have_full_screen_app = false;

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
            uint16_t   *dock_sheet_id = tw_data->sheet_id;
            int         scdx          = tw_data->scdx;
            time_read(time);

            if ((old_time->tm_min != time->tm_min || old_time->tm_hour != time->tm_hour) && !tw_data->need_flush &&
                !tw_data->flushing)
            {
                tw_data->flushing = true;
                *old_time         = *time;
                draw_rect(shtinf, dock_sheet_id, scdx - 40, 0, scdx - 3, 23, DOCK_COL);
                // 12 小时制
                print_fmt_box_ttf(shtinf, dock_sheet_id, BLACK, scdx - 40, 1, 10, "%02u:%02u", time->tm_hour % 24,
                                  time->tm_min);
                tw_data->flushing   = false;
                tw_data->need_flush = true;
            }
        }
        if (ms_dec.need_flush) { process_mouse_info(); }
    }
}

extern uint16_t *mouse_sheet_id_img;
uint16_t         desktop_sheet_id = 1145; // 如果SheetTest那行输出的是1145的话那么绝对有问题
uint16_t         dock_sheet_id    = 1145;
uint16_t         mouse_sheet_id   = 1145;
uint16_t zhe_shi_yi_ge_sha_bi_dao_ji_zhi_de_cao_zuo_da_jia_bu_yao_xue___GuoqiFish_is_shabi_and_this_var_is_cao_gao_zhi = 1145;

void desktop_flusher()
{
    no_interrupt = true;
    disable_intr();
    disable_scheduler();

    write_serial_string("desktop flusher is running\n");

    SHEET_INFO shtinf;
    init_sheet(*fbc_addr, &shtinf);

    sht_img = &shtinf;
    msid    = &mouse_sheet_id;

    create_sheet(&shtinf, 0, 0, fbc_addr->horizontal_resolution, fbc_addr->vertical_resolution, FixedSheetType, 0,
                 &desktop_sheet_id);
    create_sheet(&shtinf, 0, 0, fbc_addr->horizontal_resolution, fbc_addr->vertical_resolution, FixedSheetType, 9999,
                 &dock_sheet_id);
    create_sheet(&shtinf, (get_hor() - 16) / 2, (get_ver() - 28 - 29) / 2, 10, 21, FixedSheetType, 10000,
                 &mouse_sheet_id);

    // 草稿纸吗，有点意思
    create_sheet(&shtinf, get_hor() + 114, get_ver() + 114, 1000, 1000, FixedSheetType, -1,
                 &zhe_shi_yi_ge_sha_bi_dao_ji_zhi_de_cao_zuo_da_jia_bu_yao_xue___GuoqiFish_is_shabi_and_this_var_is_cao_gao_zhi);

    init_ttf();

    rect(*fbc_addr, 0, 0, int(fbc_addr->horizontal_resolution) / 10 * 9, 7, {0x00, 0xa2, 0xe8}); // GUI好了来一下

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
    // PrintString(&shtinf, &desktop_sheet_id, 8, 480, "MapKey:", WHITE);
    // PrintHex(&shtinf, &desktop_sheet_id, 8, 496, BC->MemoryMap.MapKey, WHITE);
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

    print_fmt_box_ttf(&shtinf, &desktop_sheet_id, WHITE, 64, 64, 16,
                      "MAKE \nXJ380 \nGREAT \nAGAIN! \n实现XJ380的伟大复兴！");
#endif
    // wsod_debug();

    // print_box_ttf(&shtinf, &desktop_sheet_id, (char *)"MAKE XJ380 GREAT AGAIN! 实现XJ380的伟大复兴！", WHITE, 64, 64,
    //               16);

    draw_mouse(&shtinf, &mouse_sheet_id);
    XWM_INFO  xwmi;
    WINDOWLS *test_win;
    WINDOWLS *test_win2;
    init_xwm(&xwmi);
    xwmii = &xwmi;

    rect(*fbc_addr, 0, 0, int(fbc_addr->horizontal_resolution) / 10 * 10, 7, {0x00, 0xa2, 0xe8}); // 全都好了来一下

    mouse_sheet_id_img = &mouse_sheet_id;

    no_interrupt = false;
    enable_intr();
    enable_scheduler();

    winRD_lock = true;
    // create_window(&xwmi, &shtinf, &test_win2, (char *)"窗口", 800, 600);

    // delete_window(&xwmi, &shtinf, test_win);

    allow_to_flush = true;
    refresh_sheet(&shtinf); // 刷新
    // delete_sheet(&shtinf, &dock_sheet_id);
    uint8_t k_input;

    // struct EFI_RUNTIME_SERVICES *RuntimeServices = (struct EFI_RUNTIME_SERVICES *)convert_physical_to_virtual((uint64_t)(ST->RuntimeServices));
    // EFIAPI EFI_STATUS (*GetTime)(EFI_TIME *, EFI_TIME_CAPABILITIES *) = (EFIAPI EFI_STATUS(*)(EFI_TIME *, EFI_TIME_CAPABILITIES *))convert_physical_to_virtual((uint64_t)RuntimeServices->GetTime);
    int scdx = fbc_addr->horizontal_resolution * 13 / 16;

    tw_data           = (TimerWidgetsData *)malloc(sizeof(TimerWidgetsData));
    tw_data->sht      = &shtinf;
    tw_data->sheet_id = &dock_sheet_id;
    tw_data->scdx     = scdx;

    desktop_done = true;
    while (!xhci_done)
        cpu_relax();

    delay_s_hp(3);

    // char *argv[10]    = {"a", "b", "c", "d"};
    // create_user_process_from_file((char *)"/system/shell.elf", NULL, NULL);
    create_user_process_from_file((char *)"/system/fmanager.elf", NULL, NULL);
    create_user_process_from_file((char *)"/system/calc.elf", NULL, NULL);
    // create_user_process_from_file((char *)"/system/ctrlmenu.elf", NULL, NULL);
    // create_user_process_from_file((char *)"/apps/cp537.elf", NULL, NULL);
    // create_user_process_from_file((char *)"/apps/bcms-sp.elf", NULL, NULL);
    // calc_ttf_length("114514 测试", 10);
   
    while (1)
    {
        if (!no_interrupt)
        {
            enable_intr();
            enable_scheduler();
        }
        if (tw_data->flushing) { continue; } // Sync, but bad performance
        if (tw_data->need_flush)
        {
            tw_data->need_flush = false;
            refresh_part_sheet(&shtinf, scdx - 40, 0, scdx, 24); // 刷新时间
        }

        k_input = kernel_getch();
        if (k_input != NULL)
        {
            WINDOWLSP current_window = sht_found_win(xwmii, sht_img, ms_dec.sht_now);
            if (current_window != NULL)
            {
                if (k_input != '\n' && k_input != '\b' && k_input < 128)
                {
                    do_message(0, NULL, k_input, current_window->WinMPf, current_window->w_task);
                }
                else
                {
                    do_message(7, NULL, k_input, current_window->WinMPf, current_window->w_task);
                }
            }
        }
#if BUILD_EDITION == DEBUG_VERSION
        if (k_input != NULL && k_input != '\n' && k_input != '\b')
        {
            PrintFont(&shtinf, &desktop_sheet_id, fontx, fonty, k_input, WHITE);
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
    }
}

void tst()
{
    write_serial_fmt("Stack Test\n");
    while (1)
        ;
}

void judge_bigend_littleend1() {
   int i = 48;
   char c = *((char*)&i);
   if (c == '0')
       write_serial_fmt("Littleend LE\n");
   else
       write_serial_fmt("Bigend BE\n");
}

extern size_t   now_tid;
extern void nvme_setup();
extern int xhci_setup();
extern void mount_root();
extern void stdio_init();
extern "C" void KernelMain(const FrameBufferConfig &fbc, EFI_SYSTEM_TABLE &SystemTable, BOOT_CONFIG &BootConfig)
{
    fbc_addr = &fbc;
    (void)SystemTable; // 暂时不用
    no_interrupt = true;
    disable_intr();
    disable_scheduler();

    rect(*fbc_addr, 0, 0, int(fbc_addr->horizontal_resolution), 7, {0xe6, 0xe6, 0xe6}); // 开机进度条

    init_cpu();
    init_idt();
    init_gdt();
    init_hpet(BootConfig.HPET);
    init_apic(BootConfig.MADT);
    judge_bigend_littleend1();

    rect(*fbc_addr, 0, 0, int(fbc_addr->horizontal_resolution) / 10 * 1, 7, {0x00, 0xa2, 0xe8}); // CPU好了来一下

    init_hhdm();
    init_frame(BootConfig.MemoryMap);
    page_setup();
    init_heap();

    rect(*fbc_addr, 0, 0, int(fbc_addr->horizontal_resolution) / 10 * 3, 7, {0x00, 0xa2, 0xe8}); // 内存好了来一下

    serial_wprintf("Output by Serial Writer!!\n");

    device_manager_init();
    vfs_init();
    fatfs_init();
    devfs_setup();

    pci_setup(BootConfig.MCFG);
    pci_init();
    ahci_setup();
    nvme_setup();
    partition_init();
    // enable_intr();
    
    // disable_intr();
    usb_init();
    // hda_init();
    // sb16_init();

    keyboard_setup();
    write_serial_fmt("KEYBOARD (PS2) \n");
    mouse_init();
    write_serial_fmt("MOUSE (PS2) \n");

    rect(*fbc_addr, 0, 0, int(fbc_addr->horizontal_resolution) / 10 * 5, 7, {0x00, 0xa2, 0xe8}); // 驱动好了来一下

    disable_intr();

    process_setup();

    // 超绝偷懒解法，等有内存管理且在这之前初始化的时候叫我来改
    saved_mtrrs = (uint64_t *)((uint64_t)BootConfig.saved_mtrrs + 0xffff800000000000);
    for (int i = 0; i < MAX_CPU_NUM; ++i)
        temp_stack[i] = BootConfig.temp_stack[i];

    init_smp(BootConfig.MADT);

    tcb_t idle_thread         = (tcb_t)phys_to_virt(alloc_frames(KERNEL_STACK_SIZE / PAGE_SIZE));
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

    get_current_cpu()->current_task = idle_thread;

    memset(phys_to_virt(get_cr3()), 0, PAGE_SIZE / 2);

    disable_scheduler();

    rect(*fbc_addr, 0, 0, int(fbc_addr->horizontal_resolution) / 20 * 11, 7, {0x00, 0xa2, 0xe8}); // 多任务好了来一下

    
    mount_root();
    procfs_setup();
    pipefs_setup();
    stdio_init();


    signal_init();
    init_syscall();
    init_message();

    rect(*fbc_addr, 0, 0, int(fbc_addr->horizontal_resolution) / 10 * 6, 7, {0x00, 0xa2, 0xe8}); // 驱动好了来一下

    while (true)
    {
        __asm__ volatile("pause");
        if (scheduler_is_ready == xsi->cpu_count) break;
    }

    rect(*fbc_addr, 0, 0, int(fbc_addr->horizontal_resolution) / 10 * 7, 7, {0x00, 0xa2, 0xe8}); // 驱动好了来一下

    
    init_user();
    init_killer();
    write_serial_string("User & Killer is ready\n");

    if (BootConfig.is_qemu == 1) pr_warn("XJ380 is running under the QEMU/BOCHS.\n");

    // create_kernel_thread((void *)tst, NULL, (char *)"1", NULL);
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

    hid_init();
    xhci_setup();

    no_interrupt = true;
    close_interrupt;
    disable_scheduler();
    
    create_kernel_thread((void *)components_flusher, NULL, (char *)"components_flusher", NULL);

    enable_scheduler();
    open_interrupt;
    no_interrupt = false;

    while (true)
    {
        if(!no_interrupt)
        {
            enable_intr();
            enable_scheduler();
        }
        
        __asm__ __volatile__("pause");
    }
}
