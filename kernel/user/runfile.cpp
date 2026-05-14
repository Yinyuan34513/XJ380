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
#include <user/runfile.h>

extern UserInfo *current_user;

// 这个应该挪到OOBE里，只在第一次启动XJ380时创建，这里只是为了方便测试
void init_runfile()
{
    char runfile_setfile_path[256];
    memset(runfile_setfile_path, 0, 256);
    strcat(runfile_setfile_path, "/users/");
    strcat(runfile_setfile_path, "GuoqiFish");
    strcat(runfile_setfile_path, "/runfile.dat");
    vfs_mkfile(runfile_setfile_path);

    char  settings_file_buffer[sizeof(RunfileSettings_Item) * 1024];
    vfs_node_t settings_file_v = vfs_open(runfile_setfile_path);
    if (!settings_file_v) return;

    RunfileSettings_Format *file_format = (RunfileSettings_Format *)settings_file_buffer;

    strcpy(file_format->items[0].exname,   "txt");
    strcpy(file_format->items[0].describe, "文本文件");
    strcpy(file_format->items[0].runpath,  "/apps/builtin/texter.elf");

    strcpy(file_format->items[1].exname,   "inf");
    strcpy(file_format->items[1].describe, "配置文件");
    strcpy(file_format->items[1].runpath,  "/apps/builtin/texter.elf");

    strcpy(file_format->items[2].exname,   "png");
    strcpy(file_format->items[2].describe, "PNG 图片文件");
    strcpy(file_format->items[2].runpath,  "/apps/builtin/picturer.elf");

    strcpy(file_format->items[3].exname,   "jpg");
    strcpy(file_format->items[3].describe, "JPEG 图片文件");
    strcpy(file_format->items[3].runpath,  "/apps/builtin/picturer.elf");

    strcpy(file_format->items[4].exname,   "bmp");
    strcpy(file_format->items[4].describe, "位图文件");
    strcpy(file_format->items[4].runpath,  "/apps/builtin/picturer.elf");

    strcpy(file_format->items[5].exname,   "xtb");
    strcpy(file_format->items[5].describe, "XJ380 块状文本标记文本文件");
    strcpy(file_format->items[5].runpath,  "/apps/builtin/texter.elf");

    strcpy(file_format->items[6].exname,   "");

    vfs_write(settings_file_v, settings_file_buffer, 0, sizeof(RunfileSettings_Item) * 1024);

    vfs_close(settings_file_v);



    // 回收再利用
    memset(runfile_setfile_path, 0, 256);
    strcat(runfile_setfile_path, "/users/");
    strcat(runfile_setfile_path, "GuoqiFish");
    strcat(runfile_setfile_path, "/settings.dat");
    vfs_mkfile(runfile_setfile_path);

    char  settings_data_buffer[sizeof(SettingsDataFileFormat)];
    settings_file_v = vfs_open(runfile_setfile_path);
    if (!settings_file_v) return;

    SettingsDataFileFormat *file_data = (SettingsDataFileFormat *)settings_data_buffer;

    strcpy(file_data->BackgroundFilePath, "/system/resources/image/background2.png");
    file_data->ClockHourOffset = 8;

    vfs_write(settings_file_v, settings_data_buffer, 0, sizeof(SettingsDataFileFormat));

    vfs_close(settings_file_v);



    // 回收再利用
    memset(runfile_setfile_path, 0, 256);
    strcat(runfile_setfile_path, "/system/config/usereg.dat");
    vfs_mkfile(runfile_setfile_path);

    char  urf_buffer[sizeof(UserRegisterList)];
    settings_file_v = vfs_open(runfile_setfile_path);
    if (!settings_file_v) return;

    UserRegisterList *urf_data = (UserRegisterList *)urf_buffer;

    urf_data->user_count = 3;

    strcpy(urf_data->uinf[0].password, "114514");

    strcpy(urf_data->uinf[1].password, "114514");
    strcpy(urf_data->uinf[1].name, "GuoqiFish");
    urf_data->uinf[1].user_type = XUT_Admin;
    vfs_mkdir("/users");
    char user_folder_path[265] = "/users/";
    strcat(user_folder_path, "GuoqiFish");
    vfs_mkdir(user_folder_path);
    
    strcpy(urf_data->uinf[2].password, "114514");
    strcpy(urf_data->uinf[2].name, "Another User");
    urf_data->uinf[2].user_type = XUT_Admin;
    vfs_mkdir("/users");
    char user_folder_path1[265] = "/users/";
    strcat(user_folder_path1, "Another User");
    vfs_mkdir(user_folder_path1);

    vfs_write(settings_file_v, urf_buffer, 0, sizeof(UserRegisterList));

    vfs_close(settings_file_v);
}

void runfile(char *path)
{
    // 获取扩展名
    char exname[10];
    memset(exname, 0, 10);
    if (!get_file_exname(path, exname))
    {
        return;
    }

    if (strcmp("elf", exname) == 0 || strcmp("epf", exname) == 0)
    {
        // 是可执行文件
        create_user_process_from_file(path, NULL, NULL);
        return;
    }

    // 搜索扩展名
    char runfile_setfile_path[256];
    memset(runfile_setfile_path, 0, 256);
    strcat(runfile_setfile_path, "/users/");
    strcat(runfile_setfile_path, current_user->name);
    strcat(runfile_setfile_path, "/runfile.dat");
    vfs_mkfile(runfile_setfile_path);

    char  settings_file_buffer[sizeof(RunfileSettings_Item) * 1024];
    vfs_node_t settings_file_v = vfs_open(runfile_setfile_path);
    if (!settings_file_v) return;
    vfs_read(settings_file_v, settings_file_buffer, 0, settings_file_v->size);

    RunfileSettings_Format *file_format = (RunfileSettings_Format *)settings_file_buffer;
    for (int i = 0; i < 1024; i++)
    {
        if (strcmp(exname, file_format->items[i].exname) == 0)
        {
            // 是可执行文件
            char *argv[2]    = {path, NULL};
            create_user_process_from_file(file_format->items[i].runpath, NULL, argv);
            return;
        }
    }

    vfs_close(settings_file_v);
}

bool get_file_exname(char *name, char *exname)
{
    if (name == NULL || exname == NULL) return false;

    char *p = name;
    char *q = exname;
    bool have_type = false;
    while (*p)
    {
        p++;
        if (*p == '.')
            have_type = true;
    }

    if (!have_type)
        return false;
    
    while (*p != '.')
        p--;

    p++;

    size_t copied = 0;
    while (*p)
    {
        if (copied >= 9) break;
        *q = *p;
        p++; q++;
        copied++;
    }
    *q = '\0';
    
    return true;
}
