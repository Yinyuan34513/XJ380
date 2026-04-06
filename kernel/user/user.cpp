#include "../build_settings.h"
#include <ahci/ahci.h>
#include <efi/boot.h>
#include <efi/efi.h>
#include <efi/fbc.h>
#include <elf.h>
#include <errno.h>
#include <font.h>
#include <fs/fatfs/fatfs.h>
#include <fs/partition.h>
#include <fs/vfs/devfs.h>
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
#include <stdint.h>
#include <syscall/syscall.h>
#include <task/pcb.h>
#include <user/user.h>

#include <ttf.h>

char *current_user_envp[100] = {
    ENVP_SYSTEM_VERSION,
    NULL // 必须以NULL结尾
};

UserInfo *current_user = NULL;

UserInfo root_user = {
    .name = "Root",
    .user_type = XUT_Root,
    .envc = 2,
    .envp = current_user_envp,
};

extern SHEET *desktop_ct_sheet;
extern mouse_dec ms_dec;
extern bool no_interrupt;
extern bool is_scheduler;

static void restore_runtime_state(bool was_scheduler_enabled, bool was_interrupt_enabled)
{
    if (was_scheduler_enabled) enable_scheduler();
    if (was_interrupt_enabled && !no_interrupt) open_interrupt;
    else close_interrupt;
}

static bool append_cmdline_arg(char *cmdline, size_t cmdline_size, char **cursor, const char *arg)
{
    if (cmdline == NULL || cursor == NULL || *cursor == NULL || arg == NULL) return false;

    size_t used = (size_t)(*cursor - cmdline);
    if (used >= cmdline_size) return false;

    int written = snprintf(*cursor, cmdline_size - used, "%s ", arg);
    if (written < 0 || (size_t)written >= (cmdline_size - used)) return false;

    *cursor += written;
    return true;
}

char password_layout_ball_bitmap[8][9] = {
    "..####..",
    ".######.",
    "########",
    "########",
    "########",
    "########",
    ".######.",
    "..####..",
};

static const int PASSWORD_BOX_X      = 128;
static const int PASSWORD_BOX_Y      = 168;
static const int PASSWORD_BOX_WIDTH  = 300;
static const int PASSWORD_BOX_HEIGHT = 24;
static const int PASSWORD_TEXT_X     = 136;
static const int PASSWORD_TEXT_Y     = 176;
static const int PASSWORD_SLOT_WIDTH = 16;
static const int PASSWORD_MAX_INPUT  = 18;
static const uint64_t PASSWORD_CURSOR_BLINK_NS = 500000000ULL;

static void refresh_user_ui_now(int x1, int y1, int x2, int y2)
{
    refresh_part_sheet(sht_img, x1, y1, x2, y2);
    flush_sheet_damage_queue(sht_img);
}

static void refresh_user_fullscreen_now()
{
    refresh_sheet(sht_img);
    flush_sheet_damage_queue(sht_img);
}

static void service_user_wait_loop()
{
    flush_sheet_damage_queue(sht_img);
    __asm__ __volatile__("pause");
}

static void render_password_input(const char *input_password, int input_count, bool show_cursor)
{
    (void)input_password;

    draw_rect(sht_img, desktop_ct_sheet, PASSWORD_BOX_X, PASSWORD_BOX_Y, PASSWORD_BOX_X + PASSWORD_BOX_WIDTH,
              PASSWORD_BOX_Y + PASSWORD_BOX_HEIGHT, WHITE);

    for (int i = 0; i < input_count; i++)
    {
        for (int y = 0; y < 8; y++)
        {
            for (int x = 0; x < 8; x++)
            {
                if (password_layout_ball_bitmap[y][x] == '#')
                {
                    draw_point(sht_img, desktop_ct_sheet, PASSWORD_TEXT_X + PASSWORD_SLOT_WIDTH * i + x,
                               PASSWORD_TEXT_Y + y, BLACK);
                }
            }
        }
    }

    if (show_cursor)
    {
        int cursor_x = PASSWORD_TEXT_X + PASSWORD_SLOT_WIDTH * input_count;
        draw_rect(sht_img, desktop_ct_sheet, cursor_x, PASSWORD_BOX_Y + 4, cursor_x + 1, PASSWORD_BOX_Y + 20, BLACK);
    }

    refresh_user_ui_now(PASSWORD_BOX_X, PASSWORD_BOX_Y, PASSWORD_BOX_X + PASSWORD_BOX_WIDTH,
                        PASSWORD_BOX_Y + PASSWORD_BOX_HEIGHT);
}

void init_user()
{
    char user_reg_data_path[256];
    memset(user_reg_data_path, 0, 256);
    strcat(user_reg_data_path, "/system/usereg.dat");
    vfs_mkfile(user_reg_data_path);

    char  urf_buffer[sizeof(UserRegisterList)];
    vfs_node_t urd_v = vfs_open(user_reg_data_path);
    if (!urd_v) return;

    vfs_read(urd_v, urf_buffer, 0, sizeof(UserRegisterList));

    UserRegisterList *urf_data = (UserRegisterList *)urf_buffer;
    int current_user_number = 1;

    // 选择用户
    draw_rect(sht_img, desktop_ct_sheet, 0, 0, sht_img->scrx - 1, sht_img->scry - 1, {0x3f, 0x48, 0xcc, 0xff});
    PrintPicture_blend(sht_img, desktop_ct_sheet, 0, 0, sht_img->scrx, sht_img->scry, "/system/image/background3.png");
    print_box_ttf(sht_img, desktop_ct_sheet, "选择一个用户", WHITE, 64, 44, 40);

    for (int i = 1; i < urf_data->user_count; i++)
    {
        PrintPicture_blend(sht_img, desktop_ct_sheet, 64, 130 + (i - 1) * 50, 50, 50, "/system/icon/user.png");
        print_box_ttf(sht_img, desktop_ct_sheet, urf_data->uinf[i].name, WHITE, 122, 138 + (i - 1) * 50, 16);
    }

    refresh_user_fullscreen_now();

    while (1)
    {
        service_user_wait_loop();
        if (ms_dec.left)
        {
            if (get_mouse_x() > 64 && get_mouse_y() > 130 && get_mouse_y() < 130 + (urf_data->user_count) * 50)
            {
                current_user_number = (get_mouse_y() - 130) / 50 + 1;
                break;
            }
        }
    }

    // 登录
    draw_rect(sht_img, desktop_ct_sheet, 0, 0, sht_img->scrx - 1, sht_img->scry - 1, {0x3f, 0x48, 0xcc, 0xff});
    PrintPicture_blend(sht_img, desktop_ct_sheet, 0, 0, sht_img->scrx, sht_img->scry, "/system/image/background3.png");
    print_box_ttf(sht_img, desktop_ct_sheet, "登录", WHITE, 64, 44, 40);
    print_box_ttf(sht_img, desktop_ct_sheet, "用户：", WHITE, 64, 130, 16);
    print_box_ttf(sht_img, desktop_ct_sheet, "密码：", WHITE, 64, 164, 16);

    draw_rect(sht_img, desktop_ct_sheet, PASSWORD_BOX_X, PASSWORD_BOX_Y, PASSWORD_BOX_X + PASSWORD_BOX_WIDTH,
              PASSWORD_BOX_Y + PASSWORD_BOX_HEIGHT, WHITE);
    print_box_ttf(sht_img, desktop_ct_sheet, urf_data->uinf[current_user_number].name, WHITE, 128, 130, 16);
    PrintPicture_blend(sht_img, desktop_ct_sheet, 128 + 300 + 8, 168, 24, 24, "/system/icon/nextstep.png");

    refresh_user_fullscreen_now();

    int input_count = 0;
    char input_password[64];
    memset(input_password, 0, sizeof(input_password));
    bool cursor_visible = true;
    uint64_t last_cursor_toggle = nanoTime();
    render_password_input(input_password, input_count, cursor_visible);
    while (1)
    {
        service_user_wait_loop();
        uint64_t now = nanoTime();
        if (now - last_cursor_toggle >= PASSWORD_CURSOR_BLINK_NS)
        {
            cursor_visible = !cursor_visible;
            last_cursor_toggle = now;
            render_password_input(input_password, input_count, cursor_visible);
        }

        uint8_t k_input = get_keyboard_input();
        if (k_input != '\b' && k_input != '\n' && k_input != NULL && k_input < 128 && input_count < PASSWORD_MAX_INPUT)
        {
            input_password[input_count] = k_input;
            input_count++;
            cursor_visible = true;
            last_cursor_toggle = nanoTime();
            render_password_input(input_password, input_count, cursor_visible);
        }
        else if (k_input == '\b')
        {
            if (input_count > 0)
            {
                input_count--;
                input_password[input_count] = '\0';
                cursor_visible = true;
                last_cursor_toggle = nanoTime();
                render_password_input(input_password, input_count, cursor_visible);
            }
        }
        else if (k_input == '\n')
        {
            goto check_password;
        }

        if (ms_dec.left)
        {
            if (get_mouse_x() > 128 + 300 + 8 && get_mouse_y() > 168 && 
                get_mouse_x() < 128 + 300 + 8 + 24 && get_mouse_y() < 168 + 24)
            {
                // 检查密码
check_password:
                input_password[input_count] = '\0';
                if (strcmp(input_password, urf_data->uinf[current_user_number].password) == 0)
                {
                    break;
                }
                else
                {
                    print_box_ttf(sht_img, desktop_ct_sheet, "密码错误，请重试。", RED, 64, 200, 12);
                    refresh_user_ui_now(64, 200, 220, 216);
                    delay_ms(200);
                }
            }
        }
    }

    current_user = (UserInfo *)malloc(sizeof(UserInfo));
    strcpy(current_user->name, urf_data->uinf[current_user_number].name);
    strcpy(current_user->password, urf_data->uinf[current_user_number].password);
    current_user->user_type = urf_data->uinf[current_user_number].user_type;
    current_user->envc      = 2;
    current_user->envp      = current_user_envp;

    center_mouse_cursor();

    vfs_mkdir("/users");
    char user_folder_path[265] = "/users/";
    strcat(user_folder_path, current_user->name);
    vfs_mkdir(user_folder_path);
}

void copy_args(char *dst[], char *src[], int n)
{
    for (int i = 0; i < n; i++)
    {
        dst[i] = NULL;
        if (src[i] != NULL)
        {
            size_t len = strlen(src[i]);
            dst[i] = (char *)malloc(len + 1);
            if (dst[i] == NULL)
            {
                for (int j = 0; j < i; ++j)
                {
                    if (dst[j] != NULL)
                    {
                        free(dst[j]);
                        dst[j] = NULL;
                    }
                }
                return;
            }
            memcpy(dst[i], src[i], len + 1);
        }
    }
}

void get_thread_name_from_filepath(char *path, char *name)
{
    if (path == NULL || name == NULL)
    {
        return;
    }

    char *start = path;
    for (char *p = path; *p != '\0'; p++)
    {
        if (*p == '\\' || *p == '/' || *p == '|') start = p + 1;
    }

    size_t i = 0;
    while (start[i] != '\0' && start[i] != '.' && i < 31)
    {
        name[i] = start[i];
        i++;
    }
    name[i] = '\0';
}
void *aligned_malloc(size_t size, size_t alignment)
{
    // 分配额外空间用于存储原始指针
    size_t extra    = alignment - 1 + sizeof(void *);
    void  *original = malloc(size + extra);

    if (!original) return nullptr;

    // 计算对齐后的地址
    void *aligned = (void *)(((uintptr_t)original + sizeof(void *) + alignment - 1) & ~(alignment - 1));

    // 在aligned指针前保存original指针
    ((void **)aligned)[-1] = original;

    return aligned;
}
uint64_t parse_elf_file(char *path, pcb_t group)
{
    Elf64_Ehdr  ehdr;
    Elf64_Phdr *phdrs  = nullptr;
    uint64_t    pos    = 0;
    int         retval = 0;
    uint8_t    *buf    = nullptr;

    vfs_node_t file;
    file               = vfs_open(path);

    if (file == NULL)
    {
        return NULL;
    }

    uint64_t file_size = file->size;

    if (group == NULL)
    {
        pr_warn("Parse ELF/EPF file failed.\n");
        write_serial_fmt("Reason:   ? Where is your parent group?\n");
        vfs_close(file);
        return NULL;
    }

    // 读取整个文件
    buf = (uint8_t *)phys_to_virt(alloc_frames((file_size + PAGE_SIZE - 1) / PAGE_SIZE));
    if (!buf)
    {
        pr_warn("Parse ELF/EPF file failed.\n");
        write_serial_fmt("Reason:   Cannot Alloc Memory.\n");
        vfs_close(file);
        return NULL;
    }
    memset(buf, 0, file_size);

    if (vfs_read(file, buf, 0, file->size) == -1)
    {
        free_frames((uint64_t)virt_to_phys((uint64_t)buf), (file_size + PAGE_SIZE - 1) / PAGE_SIZE);
        pr_warn("Parse ELF/EPF file failed.\n");
        write_serial_fmt("Reason:   Cannot Read File.\n");
        vfs_close(file);
        return NULL;
    }

    // 解析ELF/EPF头
    ehdr = *(Elf64_Ehdr *)buf;

    // 验证ELF/EPF魔数 (0x7F,'E','L','F') (0x24, 'E', 'P', 'F')
    if ((*(uint32_t *)ehdr.e_ident != 0x464C457F)     // ELF
        && (*(uint32_t *)ehdr.e_ident != 0x46504524)) // EPF
    {
        free_frames((uint64_t)virt_to_phys((uint64_t)buf), (file_size + PAGE_SIZE - 1) / PAGE_SIZE);
        pr_warn("Parse ELF/EPF file failed.\n");
        write_serial_fmt("Reason:   Bad Format.\n");
        vfs_close(file);
        return NULL;
    }

    uint64_t entry = ehdr.e_entry;

    elf_mapping_space *mapping_space = (elf_mapping_space *)aligned_alloc(16, sizeof(elf_mapping_space));
    if (mapping_space == NULL)
    {
        free_frames((uint64_t)virt_to_phys((uint64_t)buf), (file_size + PAGE_SIZE - 1) / PAGE_SIZE);
        pr_warn("Parse ELF/EPF file failed.\n");
        write_serial_fmt("Reason:   Cannot Alloc Mapping Space.\n");
        vfs_close(file);
        return NULL;
    }
    CalcLoadAddressRange((Elf64_Ehdr *)buf, (uint64_t *)&mapping_space->load_start,
                         (uint64_t *)&mapping_space->load_end);

    bool is_sti                = are_interrupts_enabled();
    bool was_scheduler_enabled = is_scheduler;
    if (!no_interrupt) close_interrupt;
    disable_scheduler();

    page_map_range_to_random(group->pagedir, mapping_space->load_start,
                             mapping_space->load_end - mapping_space->load_start,
                             PTE_PRESENT | PTE_WRITEABLE | PTE_USER);

    page_directory_t *current_pagedir = get_current_directory();

    switch_page_directory(group->pagedir);

    memset((void *)mapping_space->load_start, 0, mapping_space->load_end - mapping_space->load_start);

    // 获取程序头表
    phdrs = (Elf64_Phdr *)(buf + ehdr.e_phoff);

    // 呃，目前看起来好像没什么用？
    uint64_t start_code   = ~0ULL, end_code = 0;
    uint64_t start_data   = ~0ULL, end_data = 0;
    uint64_t start_rodata = ~0ULL, end_rodata = 0;
    uint64_t start_brk    = 0, end_brk = 0;
    uint64_t start_stack  = 0;

    // 遍历并加载程序段
    for (int i = 0; i < ehdr.e_phnum; i++)
    {
        if (phdrs[i].p_type != PT_LOAD) { continue; }

        // 对齐到页大小
        uint64_t v_addr = phdrs[i].p_vaddr & ~(PAGE_SIZE - 1);
        uint64_t v_size = (phdrs[i].p_memsz + (phdrs[i].p_vaddr & (PAGE_SIZE - 1)) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

        // 更新内存映射信息
        if (phdrs[i].p_flags & 0x1)
        { // 可执行
            start_code = min(start_code, v_addr);
            end_code   = max(end_code, v_addr + v_size);
        }
        else if (phdrs[i].p_flags & 0x2)
        { // 可写
            start_data = min(start_data, v_addr);
            end_data   = max(end_data, v_addr + v_size);
        }
        else
        {
            start_rodata = min(start_rodata, v_addr);
            end_rodata   = max(end_rodata, v_addr + v_size);
        }

        // 复制段内容
        uint64_t segm_in_file = (uint64_t)buf + phdrs[i].p_offset;

        memcpy((void *)phdrs[i].p_vaddr, (void *)segm_in_file, phdrs[i].p_filesz);

        // 清零未初始化数据
        if (phdrs[i].p_memsz > phdrs[i].p_filesz)
        {
            memset((void *)(phdrs[i].p_vaddr + phdrs[i].p_filesz), 0, phdrs[i].p_memsz - phdrs[i].p_filesz);
        }
    }

    // 何意味
    // uint64_t buf_len = (file_size + PAGE_SIZE - 1) & (~(PAGE_SIZE - 1));
    // uint8_t *buffer = (uint8_t *)EHDR_START_ADDR;
    // page_map_range_to_random(get_current_directory(), EHDR_START_ADDR, buf_len,
    //                          PTE_PRESENT | PTE_WRITEABLE | PTE_USER);
    // memcpy(buffer, buf, file_size);

    group->load_start = mapping_space->load_start;
    group->elf_file   = NULL;
    group->elf_size   = file_size;
    // VMA
    vma_t *ld_so_vma = vma_alloc();

    ld_so_vma->vm_start  = mapping_space->load_start;
    ld_so_vma->vm_end    = mapping_space->load_end;
    ld_so_vma->vm_flags |= VMA_READ | VMA_WRITE | VMA_EXEC;

    ld_so_vma->vm_type = VMA_TYPE_ANON;
    ld_so_vma->vm_name = strdup(group->name);
    vma_insert(&group->vma_manager, ld_so_vma);

    switch_page_directory(current_pagedir); // 恢复页表
    restore_runtime_state(was_scheduler_enabled, is_sti);

    free_frames((uint64_t)virt_to_phys((uint64_t)buf), (file_size + PAGE_SIZE - 1) / PAGE_SIZE);
    vfs_close(file);
    free(mapping_space);

    // 设置程序入口点
    return entry;
}

void getFileDirectory(const char *path, char *result)
{
    // 查找最后一个路径分隔符的位置
    int lastSlash = -1;
    for (int i = 0; path[i] != '\0'; i++)
    {
        if (path[i] == '/' || path[i] == '\\') { lastSlash = i; }
    }

    // 根据不同的情况处理
    if (lastSlash == -1)
    {
        // 没有分隔符，返回空字符串
        result[0] = '\0';
    }
    else if (lastSlash == 0)
    {
        // 只有根目录，如 "/file"
        result[0] = '/';
        result[1] = '\0';
    }
    else
    {
        // 正常路径，复制目录部分
        for (int i = 0; i < lastSlash; i++)
        {
            result[i] = path[i];
        }
        result[lastSlash] = '\0';
    }
}

char *getCwd(char *path)
{
    char result[strlen(path) + 1];
    memset(result, 0, strlen(path) + 1);
    getFileDirectory(path, result);
    return strdup(result);
}

void create_user_process_from_file(char *path, pcb_t pcb, char *argv[])
{
    if (path == NULL)
    {
        write_serial_fmt("Create User Process Failed. Reason: Empty Path.\n");
        return;
    }

    char  process_name[32];
    void *entry;
    int   argc = 0;

    get_thread_name_from_filepath(path, process_name);

    write_serial_fmt("Creating User Process. Process Name: %s\n", process_name);
    page_directory_t *new_directory = clone_page_directory(get_kernel_pagedir(), false);
    if (new_directory == NULL)
    {
        write_serial_fmt("Create User Process Failed. Reason: Cannot Clone Page Directory.\n");
        return;
    }
    pcb_t             group         = create_process_group(process_name, pcb, new_directory, (char *)"");
    if (group == NULL)
    {
        free_page_directory(new_directory);
        write_serial_fmt("Create User Process Failed. Reason: Cannot Create Process Group.\n");
        return;
    }

    entry = (void *)parse_elf_file(path, group);

    if (entry == NULL)
    {
        write_serial_fmt("Create User Process Failed. Reason: Cannot Open File.\n");
        kill_proc(group, 0, false);
        return;
    }

    write_serial_fmt("Creating User Thread. Thread Name: %s\n", process_name);
    char cmdline[PAGE_SIZE];
    memset(cmdline, 0, sizeof(cmdline));
    char *cmdline_ptr = cmdline;

    if (argv != NULL)
    {
        for (int i = 0; argv[i]; i++)
        {
            if (!append_cmdline_arg(cmdline, sizeof(cmdline), &cmdline_ptr, argv[i]))
            {
                write_serial_fmt("Create User Process Failed. Reason: Command Line Too Long.\n");
                kill_proc(group, 0, false);
                return;
            }
            argc++;
        }
    }

    char *old_cmdline = group->cmdline;
    group->cmdline    = strdup(cmdline);
    if (group->cmdline == NULL)
    {
        group->cmdline = old_cmdline;
        kill_proc(group, 0, false);
        return;
    }
    if (old_cmdline != NULL) free(old_cmdline);
    char *cwd = getCwd(path);
    if (cwd == NULL)
    {
        kill_proc(group, 0, false);
        return;
    }
    write_serial_fmt("CWD: %s\n", cwd);
    size_t tid = create_user_thread(entry, NULL, argc, process_name, group, cwd);
    if ((int64_t)tid < 0)
    {
        free(cwd);
        kill_proc(group, 0, false);
    }
}

void create_user_thread_from_file(char *path, pcb_t pcb)
{
    char  thread_name[50];
    void *entry = (void *)parse_elf_file(path, pcb);
    if (entry == NULL) return;
    get_thread_name_from_filepath(path, thread_name);
    write_serial_fmt("Creating User Thread. Thread Name: %s\n", thread_name);
    char *cwd = getCwd(path);
    if (cwd == NULL) return;
    size_t tid = create_user_thread(entry, NULL, NULL, thread_name, pcb, cwd);
    if ((int64_t)tid < 0) free(cwd);
}
