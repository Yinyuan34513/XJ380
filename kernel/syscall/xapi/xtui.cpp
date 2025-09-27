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
#include <graphics/window/window.h>
#include <krlibc.h>
#include <mm/frame.h>
#include <pci/pci.h>
#include <proto.hpp>
#include <rtc.h>
#include <syscall/pxapi.h>
#include <task/pcb.h>
#include <ttf.h>
#include <syscall/syscall.h>
#include "../../build_settings.h"

extern UserInfo *current_user;

void do_xapi_GetSystemVersion(uint64_t str)
{
    char *dst = (char *)str;
    if (dst != NULL)
        memcpy(dst, OS_VERSION, strlen(OS_VERSION));
}

uint64_t do_xapi_GetTime()
{
    // TODO: Get Time
}

void do_xapi_GetCurrentUser(uint64_t dst)
{
    xapi_type_UserInfo *dest = (xapi_type_UserInfo *)dst;
    memcpy(dest->name, current_user->name, strlen(current_user->name));
    dest->user_type = current_user->user_type;
}