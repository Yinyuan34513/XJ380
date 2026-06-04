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

void write_ulog(const char *str)
{
    if (str == NULL) return;
    vfs_node_t v = vfs_open("/system/oslog.log");
    if (v == NULL) return;
    vfs_write(v, (void *)str, v->size, strlen(str) * sizeof(char));
    vfs_close(v);
}

void ulog_err(const char *err_type)
{
    tcb_t task = get_current_task();
    write_ulog("[XULS][Error] Task \"");
    write_ulog(task != NULL ? task->name : "(null)");
    write_ulog("\" Was Broken. Error Type: ");
    write_ulog(err_type != NULL ? err_type : "(null)");
    write_ulog("\n");

}

void ulog_err(char *err_type)
{
    ulog_err((const char *)err_type);
}

// 启动日志系统
void init_xuls()
{
    vfs_mkfile("/system/oslog.log");
    write_ulog("[XULS][Notice] XJ380 User Log System Initialize Success.\n");
}
