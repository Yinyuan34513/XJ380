#include <global_color.h>
#include <graphics/GOP.hpp>
#include <graphics/components/button.h>
#include <graphics/mouse.hpp>
#include <graphics/sheet.h>
#include <proto.hpp>
#include <ps2/keyboard.h>
#include <ps2/mouse.h>
#include <rtc.h>
#include <syscall/syscall.h>
#include <efi/boot.h>
#include <power.h>

mouse_dec ms_dec;
int       mouse_x = 0;
int       mouse_y = 0;
int       tby;
int       tbx;

extern char wbutton_close[18][33];
extern char wbutton_little[18][33];
bool        win_c_bt_isl = false;
bool        win_m_bt_isl = false;
#define WINDOW_ALPHA_VALUE 0xaa

SHEET        *mouse_ct_sheet_img;
extern SHEET *desktop_ct_sheet;
extern SHEET *dock_ct_sheet;
MouseType     type;

extern bool have_full_screen_app;
extern bool logo_menu_just_born;
extern bool logo_menu_is_open;
extern bool allow_to_flush;

static volatile bool desktop_refresh_pending = false;

extern EFI_SYSTEM_TABLE *EFI_ST;
extern BOOT_CONFIG *EFI_BC;

void process_mouse_button()
{
    if (!allow_to_flush) return;

    if (ms_dec.btn & 0x01)
    {
        // 左键按下
        ms_dec.left = true;
    }
    else if ((ms_dec.btn & 0x01) == 0 && ms_dec.left)
    {
        tm time;
        time_read(&time);
        uint64_t click_time = mktime(&time);
        if (ms_dec.first_click && click_time - ms_dec.click_time < 2)
        {
            ms_dec.first_click  = false;
            ms_dec.double_click = true;
        }
        else
        {
            ms_dec.first_click = true;
            ms_dec.click_time  = click_time;
        }

        WINDOWLSP current_window = sht_found_win(xwmii, sht_img, ms_dec.sht_now);
        if (current_window != NULL)
        {
            if (tbx < get_sheet(sht_img, current_window->w_sheet)->bx + 12 ||
                tby < get_sheet(sht_img, current_window->w_sheet)->by + 27 ||
                tbx > get_sheet(sht_img, current_window->w_sheet)->bx +
                          get_sheet(sht_img, current_window->w_sheet)->width - 12 ||
                tby > get_sheet(sht_img, current_window->w_sheet)->by +
                          get_sheet(sht_img, current_window->w_sheet)->height - 20)
            { /* do nothing */
            }
            else
            {
                if (current_window->type == XWIN_NORMAL)
                {
                    do_message(MSG_LBUTTON, tbx - get_sheet(sht_img, current_window->w_sheet)->bx - 12,
                               tby - get_sheet(sht_img, current_window->w_sheet)->by - 27, current_window->WinMPf,
                               current_window->w_task);
                }
                else
                {
                    do_message(MSG_LBUTTON, tbx - get_sheet(sht_img, current_window->w_sheet)->bx,
                               tby - get_sheet(sht_img, current_window->w_sheet)->by, current_window->WinMPf,
                               current_window->w_task);
                }

                process_click_event(found_sheet_byid(sht_img, current_window->w_sheet),
                                    tbx - get_sheet(sht_img, current_window->w_sheet)->bx,
                                    tby - get_sheet(sht_img, current_window->w_sheet)->by);
            }
            if (current_window->type == XWIN_NORMAL &&
                tbx > get_sheet(sht_img, current_window->w_sheet)->bx +
                          get_sheet(sht_img, current_window->w_sheet)->width - 12 - 21 &&
                tbx < get_sheet(sht_img, current_window->w_sheet)->bx +
                          get_sheet(sht_img, current_window->w_sheet)->width + 18 &&
                tby > get_sheet(sht_img, current_window->w_sheet)->by + 5 &&
                tby < get_sheet(sht_img, current_window->w_sheet)->by + 18 + 5)
            {
                // 关闭窗口
                ms_dec.sht_now         = NULL;
                tcb_t current_win_task = current_window->w_task;
                delete_window(xwmii, sht_img, current_window);
                current_window = NULL;
                if (current_win_task->window_count == 0) { kill_thread(current_win_task); }
            }
            else if (current_window->type == XWIN_NORMAL &&
                     tbx > get_sheet(sht_img, current_window->w_sheet)->bx +
                               get_sheet(sht_img, current_window->w_sheet)->width - 12 - 48 &&
                     tbx < get_sheet(sht_img, current_window->w_sheet)->bx +
                               get_sheet(sht_img, current_window->w_sheet)->width - 12 - 21 &&
                     tby > get_sheet(sht_img, current_window->w_sheet)->by + 5 &&
                     tby < get_sheet(sht_img, current_window->w_sheet)->by + 18 + 5)
            {
                // 最小化
                ms_dec.sht_now = NULL;
                save_window_xy(current_window);
                int bx = get_sheet(sht_img, current_window->w_sheet)->bx;
                // 按理来说应该给图层挪到-1层才是正解，但是太麻烦了而且由于机制问题会拖慢刷新，所以干脆扔到屏幕外面得了
                get_sheet(sht_img, current_window->w_sheet)->bx = sht_img->scrx + 10; // 扔到屏幕外面
                refresh_part_sheet(sht_img, bx, get_sheet(sht_img, current_window->w_sheet)->by,
                                   bx + get_sheet(sht_img, current_window->w_sheet)->width,
                                   get_sheet(sht_img, current_window->w_sheet)->by +
                                       get_sheet(sht_img, current_window->w_sheet)->height);
            }
        }
        ms_dec.sht_now              = found_sheetmb(sht_img, mouse_x, mouse_y);
        ms_dec.left_release_window  = current_window;
        ms_dec.left_release_pending = true;

        ms_dec.left          = false;
        ms_dec.win_move_lock = false;
    }
    if (ms_dec.btn & 0x02) { ms_dec.right = true; }
    else if ((ms_dec.btn & 0x02) == 0 && ms_dec.right)
    {
        WINDOWLSP current_window = sht_found_win(xwmii, sht_img, ms_dec.sht_now);
        if (current_window != NULL)
        {
            if (tbx < get_sheet(sht_img, current_window->w_sheet)->bx + 12 ||
                tby < get_sheet(sht_img, current_window->w_sheet)->by + 27 ||
                tbx > get_sheet(sht_img, current_window->w_sheet)->bx +
                          get_sheet(sht_img, current_window->w_sheet)->width - 12 ||
                tby > get_sheet(sht_img, current_window->w_sheet)->by +
                          get_sheet(sht_img, current_window->w_sheet)->height - 20)
            { /* do nothing */
            }
            else
            {
                if (current_window->type == XWIN_NORMAL)
                {
                    do_message(MSG_RBUTTON, tbx - get_sheet(sht_img, current_window->w_sheet)->bx - 12,
                               tby - get_sheet(sht_img, current_window->w_sheet)->by - 27, current_window->WinMPf,
                               current_window->w_task);
                }
                else
                {
                    do_message(MSG_RBUTTON, tbx - get_sheet(sht_img, current_window->w_sheet)->bx,
                               tby - get_sheet(sht_img, current_window->w_sheet)->by, current_window->WinMPf,
                               current_window->w_task);
                }
                close_menu(current_window, tbx, tby);
                delete_logo_menu();
                process_right_button_click_event(current_window, tbx, tby);
            }
        }

        ms_dec.right = false;
    }
    if (ms_dec.btn & 0x04) { ms_dec.center = true; }
    else if ((ms_dec.btn & 0x04) == 0 && ms_dec.center)
    {
        WINDOWLSP current_window = sht_found_win(xwmii, sht_img, ms_dec.sht_now);
        if (current_window != NULL)
        {
            if (tbx < get_sheet(sht_img, current_window->w_sheet)->bx + 12 ||
                tby < get_sheet(sht_img, current_window->w_sheet)->by + 27 ||
                tbx > get_sheet(sht_img, current_window->w_sheet)->bx +
                          get_sheet(sht_img, current_window->w_sheet)->width - 12 ||
                tby > get_sheet(sht_img, current_window->w_sheet)->by +
                          get_sheet(sht_img, current_window->w_sheet)->height - 20)
            { /* do nothing */
            }
            else
            {
                if (current_window->type == XWIN_NORMAL)
                {
                    do_message(MSG_MBUTTON, tbx - get_sheet(sht_img, current_window->w_sheet)->bx - 12,
                               tby - get_sheet(sht_img, current_window->w_sheet)->by - 27, current_window->WinMPf,
                               current_window->w_task);
                }
                else
                {
                    do_message(MSG_MBUTTON, tbx - get_sheet(sht_img, current_window->w_sheet)->bx,
                               tby - get_sheet(sht_img, current_window->w_sheet)->by, current_window->WinMPf,
                               current_window->w_task);
                }
            }
        }

        ms_dec.center = false;
    }

    tbx         = getBX(sht_img, mouse_ct_sheet_img);
    tby         = getBY(sht_img, mouse_ct_sheet_img);
    int mouse_x = get_mouse_x();
    int mouse_y = get_mouse_y();
    move_mouse(sht_img, mouse_ct_sheet_img, mouse_x, mouse_y);
    refresh_part_sheet(sht_img, tbx, tby, tbx + getXsize(sht_img, mouse_ct_sheet_img),
                       tby + getYsize(sht_img, mouse_ct_sheet_img));
    refresh_part_sheet(sht_img, getBX(sht_img, mouse_ct_sheet_img), getBY(sht_img, mouse_ct_sheet_img),
                       getBX(sht_img, mouse_ct_sheet_img) + getXsize(sht_img, mouse_ct_sheet_img),
                       getBY(sht_img, mouse_ct_sheet_img) + getYsize(sht_img, mouse_ct_sheet_img));

    WINDOWLSP current_window = sht_found_win(xwmii, sht_img, ms_dec.sht_now);
    if (current_window != NULL)
    {
        if (tbx < get_sheet(sht_img, current_window->w_sheet)->bx + 12 ||
            tby < get_sheet(sht_img, current_window->w_sheet)->by + 27 ||
            tbx > get_sheet(sht_img, current_window->w_sheet)->bx + get_sheet(sht_img, current_window->w_sheet)->width -
                      12 ||
            tby > get_sheet(sht_img, current_window->w_sheet)->by +
                      get_sheet(sht_img, current_window->w_sheet)->height - 20)
        { /* do nothing */
        }
        else
        {
            if (current_window->type == XWIN_NORMAL)
            {
                do_message(MSG_MOVE, tbx - get_sheet(sht_img, current_window->w_sheet)->bx - 12,
                           tby - get_sheet(sht_img, current_window->w_sheet)->by - 27, current_window->WinMPf,
                           current_window->w_task);
            }
            else
            {
                do_message(MSG_MOVE, tbx - get_sheet(sht_img, current_window->w_sheet)->bx,
                           tby - get_sheet(sht_img, current_window->w_sheet)->by, current_window->WinMPf,
                           current_window->w_task);
            }
        }
    }
    
    // 处理滚轮事件
    int scroll = get_mouse_scroll();
    if (scroll != 0 && current_window != NULL)
    {
        // 发送滚轮消息到当前窗口
        // MSG_ROLLER 消息格式：参数1 = 滚轮方向（正=向上，负=向下）
        do_message(MSG_ROLLER, scroll, 0, current_window->WinMPf, current_window->w_task);
    }
}

extern "C" void c_mouse_handler(void *regs_ptr, uint64_t error_code)
{
    send_eoi();
    uint8_t data = inb(PS2_DATA_PORT);
    if (mousedecode(data))
    {
        ms_dec.need_flush = true;
        process_mouse_button();
    }
}

void process_mouse_info()
{
    if (desktop_refresh_pending)
    {
        desktop_refresh_pending = false;
        init_desktop(sht_img, desktop_ct_sheet, dock_ct_sheet);
        refresh_sheet(sht_img);
    }

    if (ms_dec.left_release_pending)
    {
        if (ms_dec.sht_now == NULL)
        {
            // 点在了桌面/快捷栏上
            if (tbx > 24 && tby > sht_img->scry - 24 - 63 && tbx < 24 + 84 && !have_full_screen_app)
            {
                create_user_process_from_file((char *)"/system/ctrlmenu.elf", NULL, NULL);
            }
            else if (tbx > 132 && tby > sht_img->scry - 24 - 63 && !have_full_screen_app)
            {
                TDB_t tdb = find_dock_icon((tbx - 132) / 80);
                if (tdb)
                {
                    if (tdb->min_mode)
                    {
                        tdb->windowls->w_sheet->bx = tdb->bmx;
                        tdb->windowls->w_sheet->by = tdb->bmy;
                        tdb->min_mode              = false;
                    }
                    lift_sheet(sht_img, tdb->windowls->w_sheet);
                    ms_dec.sht_now = tdb->windowls->w_sheet;
                    focus_window_dock(tdb->windowls);
                    refresh_part_sheet(sht_img, tdb->windowls->w_sheet->bx, tdb->windowls->w_sheet->by,
                                       tdb->windowls->w_sheet->bx + tdb->windowls->w_sheet->width,
                                       tdb->windowls->w_sheet->by + tdb->windowls->w_sheet->height);
                }
            }
            else if (tby < 24 && !have_full_screen_app)
            {
                // 快捷栏
                int x1 = sht_img->scrx * 0.1875;
                if (tbx > x1 && tbx < x1 + 32)
                {
                    // logo 徽标
                    draw_logo_menu();
                }
                else if (tbx > x1 + 32 && tbx < x1 + 32 + 25 + 8)
                {
                    // 文件
                    create_user_process_from_file((char *)"/system/fmanager.elf", NULL, NULL);
                }
                else if (tbx > x1 + 32 + 25 + 8 && tbx < x1 + 32 + 25 + 8 + 25 + 9)
                {
                    // 设置
                    char *argv[2] = {"shortdock-open-settings", NULL};
                    create_user_process_from_file((char *)"/system/ctrlmenu.elf", NULL, argv);
                }
                else if (tbx > x1 + 32 + 25 + 8 + 25 + 9 && tbx < x1 + 32 + 25 + 8 + 25 + 9 + 25 + 9)
                {
                    // 终端
                    create_user_process_from_file((char *)"/system/shell.elf", NULL, NULL);
                }
                else if (tbx > x1 + 32 + 25 + 8 + 25 + 9 + 25 + 9 && tbx < x1 + 32 + 25 + 8 + 25 + 9 + 25 + 9 + 25 + 8)
                {
                    // 图形
                    char *argv[2] = {"shortdock-open-graphics", NULL};
                    create_user_process_from_file((char *)"/system/ctrlmenu.elf", NULL, argv);
                }
                else if (tbx > x1 + 32 + 25 + 8 + 25 + 9 + 25 + 9 + 25 + 8 &&
                         tbx < x1 + 32 + 25 + 8 + 25 + 9 + 25 + 9 + 25 + 8 + 25)
                {
                    // 关于
                    char *argv[2] = {"shortdock-open-about", NULL};
                    create_user_process_from_file((char *)"/system/ctrlmenu.elf", NULL, argv);
                }
            }
            else if (tbx > sht_img->scrx * 0.1875 && tby > 28 && tbx < sht_img->scrx * 0.1875 + 100 && tby < 50 &&
                     logo_menu_is_open)
            {
                // 关机
                power_shutdown(EFI_ST, EFI_BC);
            }
            else if (tbx > sht_img->scrx * 0.1875 && tby > 50 && tbx < sht_img->scrx * 0.1875 + 100 && tby < 72 &&
                     logo_menu_is_open)
            {
                // 刷新
                desktop_refresh_pending = true;
            }
        }
        close_menu(ms_dec.left_release_window, tbx, tby);
        delete_logo_menu();
        ms_dec.left_release_window  = NULL;
        ms_dec.left_release_pending = false;
    }

    if (ms_dec.left)
    {
        if (!ms_dec.win_move_lock)
        {
            ms_dec.sht_now = found_sheet_movable(sht_img, mouse_x, mouse_y);
            if (ms_dec.sht_now == NULL || have_full_screen_app)
            {
                // 点在了标题栏外
                ms_dec.win_move_lock = false;
            }
            else
            {
                // 点在了窗口上
                ms_dec.win_move_lock = true;
                ms_dec.sht_now       = get_sheet(sht_img, ms_dec.sht_now);
                lift_sheet(sht_img, ms_dec.sht_now);
                refresh_part_sheet(sht_img, ms_dec.sht_now->bx, ms_dec.sht_now->by,
                                   ms_dec.sht_now->bx + ms_dec.sht_now->width,
                                   ms_dec.sht_now->by + ms_dec.sht_now->height);
                ms_dec.win_x_offset = mouse_x - ms_dec.sht_now->bx;
                ms_dec.win_y_offset = mouse_y - ms_dec.sht_now->by;
            }
            ms_dec.sht_now = found_sheetmb(sht_img, mouse_x, mouse_y);
            if (ms_dec.sht_now == NULL)
            {
                // 点在了桌面或快捷栏上
                // do nothing
                if (ms_dec.double_click)
                {
                    ms_dec.double_click = false;
                    process_desktop_find_block(tbx, tby);
                }
            }
            else
            {
                ms_dec.sht_now = get_sheet(sht_img, ms_dec.sht_now);
                if (!have_full_screen_app) lift_sheet(sht_img, ms_dec.sht_now);
                refresh_part_sheet(sht_img, ms_dec.sht_now->bx, ms_dec.sht_now->by,
                                   ms_dec.sht_now->bx + ms_dec.sht_now->width,
                                   ms_dec.sht_now->by + ms_dec.sht_now->height);

                focus_window_dock(sht_found_win(xwmii, sht_img, ms_dec.sht_now));
            }
        }
        else if (ms_dec.win_move_lock)
        {
            int his_x = ms_dec.sht_now->bx;
            int his_y = ms_dec.sht_now->by;
            // 移动窗口
            ms_dec.sht_now->bx = mouse_x - ms_dec.win_x_offset;
            ms_dec.sht_now->by = mouse_y - ms_dec.win_y_offset;
            refresh_part_sheet(sht_img, ms_dec.sht_now->bx, ms_dec.sht_now->by,
                               ms_dec.sht_now->bx + ms_dec.sht_now->width, ms_dec.sht_now->by + ms_dec.sht_now->height);
            refresh_part_sheet(sht_img, his_x, his_y, his_x + ms_dec.sht_now->width, his_y + ms_dec.sht_now->height);
        }
    }

    ms_dec.need_flush = false;
}

bool mousedecode(uint8_t data)
{
    if (ms_dec.phase == 0)
    {
        if (data == 0xfa) { ms_dec.phase = 1; }
        return false;
    }
    if (ms_dec.phase == 1)
    {
        if ((data & 0xc8) == 0x08)
        {
            ms_dec.buf[0] = data;
            ms_dec.phase  = 2;
        }
        return 0;
    }
    if (ms_dec.phase == 2)
    {
        ms_dec.buf[1] = data;
        ms_dec.phase  = 3;
        return 0;
    }
    if (ms_dec.phase == 3)
    {
        ms_dec.buf[2] = data;
        ms_dec.phase  = 1;
        ms_dec.btn    = ms_dec.buf[0] & 0x07;
        ms_dec.x      = ms_dec.buf[1];
        ms_dec.y      = ms_dec.buf[2];

        if ((ms_dec.buf[0] & 0x10) != 0) ms_dec.x |= 0xffffff00;
        if ((ms_dec.buf[0] & 0x20) != 0) ms_dec.y |= 0xffffff00;
        ms_dec.y = -ms_dec.y;
        if (type == OnlyScroll || type == FiveButton)
        {
            ms_dec.phase = 4;
            return 0;
        }
        else
        {
            mouse_x += ms_dec.x;
            mouse_y += ms_dec.y;
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x > get_hor() - 1) mouse_x = get_hor() - 1;
            if (mouse_y > get_ver() - 1) mouse_y = get_ver() - 1;
            return true;
        }
    }
    if (ms_dec.phase == 4)
    {
        ms_dec.buf[3]   = data;
        ms_dec.phase    = 1;
        int wheel_delta = ms_dec.buf[3];
        if (wheel_delta >= 255) wheel_delta = -1;

        ms_dec.scroll = -wheel_delta;
        // 更新鼠标位置
        mouse_x += ms_dec.x;
        mouse_y += ms_dec.y;
        if (mouse_x < 0) mouse_x = 0;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_x > get_hor() - 1) mouse_x = get_hor() - 1;
        if (mouse_y > get_ver() - 1) mouse_y = get_ver() - 1;
        return true;
    }
    return false;
}

int get_mouse_x()
{
    return mouse_x;
}

int get_mouse_y()
{
    return mouse_y;
}

int get_mouse_scroll()
{
    int scroll = ms_dec.scroll;
    ms_dec.scroll = 0;  // 读取后重置
    return scroll;
}

static bool send_command(uint8_t value)
{
    wait_ps2_write();
    outb(PS2_CMD_PORT, KB_SEND2MOUSE);
    wait_ps2_write();
    outb(PS2_DATA_PORT, value);
    return inb(PS2_DATA_PORT) != 0xfa;
}

void mouse_wait(uint8_t a_type)
{
    uint64_t time_ns = nanoTime() + 1ULL * 1000000000ULL;
    if (!a_type)
    {
        while (nanoTime() < time_ns)
        {
            if (inb(PORT_KB_STATUS) & MOUSE_BBIT) break;
        }
    }
    else
    {
        while (nanoTime() < time_ns)
        {
            if (!((inb(PORT_KB_STATUS) & MOUSE_ABIT))) break;
        }
    }
}

void mouse_write(uint8_t write)
{
    mouse_wait(1);
    outb(PORT_KB_CMD, KB_SEND2MOUSE);
    mouse_wait(1);
    outb(PORT_KB_DATA, write);
}

uint8_t mouse_read()
{
    mouse_wait(0);
    char t = inb(PORT_KB_DATA);
    return t;
}

static MouseType get_mouse_type()
{
    send_command(0xf3);
    send_command(200);

    send_command(0xf3);
    send_command(100);

    send_command(0xf3);
    send_command(80);

    send_command(0xf2);
    wait_ps2_read();
    uint8_t type0 = inb(PS2_DATA_PORT);
    return type0 == 0x3 ? OnlyScroll : (type0 == 0x4 ? FiveButton : Standard);
}

// 修正后的 mouse_init()
void mouse_init()
{
    // 1. 禁用中断
    disable_intr();

    // 2. 启用辅助PS/2端口（鼠标）
    wait_KB_write();
    outb(PORT_KB_CMD, 0xA8);

    // 3. 清空输出缓冲区
    inb(PORT_KB_DATA);

    // 4. 设置控制器命令字节
    wait_KB_write();
    outb(PORT_KB_CMD, 0x60); // 写命令字节命令
    wait_KB_write();

    // 启用IRQ1（键盘）和IRQ12（鼠标），保留其他位
    uint8_t cmd_byte = 0x47; // 0110 0111
    // 位7: 保留
    // 位6: 翻译键盘扫描码
    // 位5: 禁用鼠标
    // 位4: 禁用键盘
    // 位3: 保留
    // 位2: 系统标志
    // 位1: 启用鼠标IRQ
    // 位0: 启用键盘IRQ
    outb(PORT_KB_DATA, cmd_byte);

    delay_ms(20);

    // 5. 重置鼠标
    mouse_write(0xFF); // 重置命令
    delay_ms(100);

    // 读取ACK和自检结果
    uint8_t ack = mouse_read();
    if (ack != 0xFA)
    {
        printk("Mouse reset ACK failed: 0x%x\n", ack);
        return;
    }

    delay_ms(500);
    uint8_t self_test = mouse_read();
    if (self_test != 0xAA)
    {
        printk("Mouse self-test failed: 0x%x\n", self_test);
        return;
    }

    uint8_t mouse_id = mouse_read();
    if (mouse_id != 0x00) { printk("Mouse ID: 0x%x\n", mouse_id); }

    // 6. 设置默认参数
    mouse_write(0xF6); // 设置默认参数
    delay_ms(10);
    ack = mouse_read();
    if (ack != 0xFA)
    {
        printk("Set defaults ACK failed: 0x%x\n", ack);
        return;
    }

    // 7. 设置采样率（重要！实体机需要）
    mouse_write(0xF3); // 设置采样率
    delay_ms(10);
    ack = mouse_read();
    if (ack != 0xFA) return;

    mouse_write(100); // 100采样/秒（适合大多数鼠标）
    delay_ms(10);
    ack = mouse_read();
    if (ack != 0xFA) return;

    // 8. 设置分辨率
    mouse_write(0xE8); // 设置分辨率
    delay_ms(10);
    ack = mouse_read();
    if (ack != 0xFA) return;

    mouse_write(0x03); // 8 counts/mm（中等分辨率）
    delay_ms(10);
    ack = mouse_read();
    if (ack != 0xFA) return;

    // 9. 设置缩放（1:1）
    mouse_write(0xE6); // 设置缩放1:1
    delay_ms(10);
    ack = mouse_read();
    if (ack != 0xFA) return;

    // 10. 启用数据报告
    mouse_write(0xF4);
    delay_ms(10);
    ack = mouse_read();
    if (ack != 0xFA) return;

    // 11. 识别鼠标类型（支持滚轮检测）
    mouse_write(0xF2); // 获取设备ID
    delay_ms(10);
    ack = mouse_read();
    if (ack != 0xFA) return;

    uint8_t device_id = mouse_read();
    if (device_id == 0x00)
    {
        type = Standard;
        printk("Standard PS/2 mouse detected\n");
    }
    else if (device_id == 0x03)
    {
        type = OnlyScroll;
        printk("Scroll wheel mouse detected\n");
    }
    else if (device_id == 0x04)
    {
        type = FiveButton;
        printk("5-button mouse detected\n");
    }
    else
    {
        type = Standard;
        printk("Unknown mouse ID 0x%x, assuming standard\n", device_id);
    }

    // 12. 重新启用数据报告（某些鼠标需要）
    mouse_write(0xF4);
    delay_ms(10);
    ack = mouse_read();
    if (ack != 0xFA) return;

    // 13. 清空可能的数据包
    while (inb(PORT_KB_STATUS) & 0x01)
    {
        inb(PORT_KB_DATA);
    }

    // 14. 初始化状态变量
    mouse_x              = (get_hor() - 16) / 2;
    mouse_y              = (get_ver() - 28 - 29) / 2;
    // 所有初始化命令的 ACK 都已经在 mouse_init() 内同步读走了，
    // 这里应当直接进入首字节等待状态，否则运行期数据包会被一直丢掉。
    ms_dec.phase         = 1;
    ms_dec.win_move_lock = false;
    ms_dec.sht_now       = NULL;
    ms_dec.need_flush    = false;
    ms_dec.first_click   = false;
    ms_dec.double_click  = false;
    ms_dec.left_release_pending = false;
    ms_dec.left          = false;
    ms_dec.left_release_window  = NULL;
    ms_dec.right         = false;
    ms_dec.center        = false;
    ms_dec.scroll        = 0;

    // 15. 启用中断
    enable_intr();

    printk("Mouse initialized successfully, type: %d\n", type);
}

// void mouse_init()
// {
//     wait_KB_write();
//     outb(PORT_KB_CMD, KB_EN_MOUSE_INTFACE);

//     delay_ms(100);

//     wait_KB_write();
//     outb(PORT_KB_CMD, KB_SEND2MOUSE);
//     wait_KB_write();
//     outb(PORT_KB_DATA, MOUSE_EN);

//     delay_ms(100);

//     wait_KB_write();
//     outb(PORT_KB_CMD, KBCMD_WRITE_CMD);
//     wait_KB_write();
//     outb(PORT_KB_DATA, KB_INIT_MODE);

//     mouse_x      = (get_hor() - 16) / 2;
//     mouse_y      = (get_ver() - 28 - 29) / 2;
//     ms_dec.phase = 0;

//     ms_dec.win_move_lock = false;
//     ms_dec.sht_now       = NULL;
// }

void draw_mouse(SHEET_INFO *sht, SHEET *csheet)
{
    for (int y = 0; y < 21; y++)
    {
        for (int x = 0; x < 10; x++)
        {
            if (mouse[y][x] == '.') { draw_point(sht, csheet, x, y, {0, 0, 0, 0}); }
            else if (mouse[y][x] == 'w') { draw_point(sht, csheet, x, y, WHITE); }
            else if (mouse[y][x] == '@') { draw_point(sht, csheet, x, y, BLACK); }
        }
    }
}

void move_mouse(SHEET_INFO *sht, SHEET *csheet, int px, int py)
{
    setBX(sht, csheet, px);
    setBY(sht, csheet, py);
}
