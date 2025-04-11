// 版权所有©XINGJI Studios 2017-2025 保留所有权利。
// XJ380 C++头文件头文件
#ifndef _INCLUDE_HPP_
#define _INCLUDE_HPP_

#include "../kernel/build_settings.h"
#include "./cppinc/GOP.hpp"
#include "./global/boot.h"
#include "./cppinc/sheet.h"
#include "./cppinc/window/window.h"
#include "./cppinc/proto.hpp"
#include "./global/stdint.h"
#include "./global/list.h"
#include "./global/spinlock.h"
#include "./global/graphics/global_color.h"
#include "./global/delay.h"
#include "./global/msr.h"
#include "./global/kernel/memory.h"
#include "./global/kernel/mmap.h"
#include "./global/kernel/pctable/gdt.h"
#include "./global/kernel/longm.h"
#include "./global/kernel/acpi.h"
#include "./global/kernel/apic/madt.h"
#include "./global/kernel/apic/hpet.h"
#include "./global/kernel/apic/apic.h"
#include "./global/kernel/smp/smp.h"
#include "./global/kernel/task/waitqueue.h"
#include "./global/kernel/task/task.h"
#include "./global/kernel/task/schedule.h"
#include "./global/kernel/ps2/keyboard.h"
#include "./global/kernel/ps2/mouse.h"
#include "./global/kernel/syscall/syscall.h"
// Drivers
#include "./global/kernel/driver/device.h"
#include "./global/kernel/driver/partition.h"
#include "./global/kernel/driver/rtc.h"
#include "./global/kernel/driver/pci.h"
#include "./global/kernel/driver/msi.h"
#include "./global/kernel/driver/ahci.h"
#include "./global/kernel/driver/nvme.h"
#include "./global/kernel/driver/usb.h"
#include "./global/kernel/driver/xhci.h"
#include "./global/kernel/driver/hid.h"
// FS
#include "./global/kernel/fs/vfs.h"
#include "./global/kernel/fs/fat.h"
extern "C"
{
#include "./global/kernel/pctable/idt.h"
#include "./efi.h"
#include "./errno.h"
}
#include "../kernel/ttf/ttf.h"
#endif
