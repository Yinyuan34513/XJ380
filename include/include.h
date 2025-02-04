// 版权所有©XINGJI Studios 2017-2025 保留所有权利。
// XJ380 C++头文件头文件
#ifndef _INCLUDE_HPP_
#define _INCLUDE_HPP_

#include "./cppinc/GOP.hpp"
#include "./global/boot.h"
#include "./cppinc/sheet.h"
#include "./cppinc/proto.hpp"
#include "./global/stdint.h"
#include "./global/list.h"
#include "./global/fifo8.h"
#include "./global/spinlock.h"
#include "./global/graphics/global_color.h"
#include "./global/delay.h"
#include "./global/kernel/memory.h"
#include "./global/kernel/mmap.h"
#include "./global/kernel/pctable/gdt.h"
#include "./global/kernel/longm.h"
#include "./global/kernel/acpi.h"
#include "./global/kernel/apic/madt.h"
#include "./global/kernel/apic/hpet.h"
#include "./global/kernel/apic/apic.h"
#include "./global/kernel/smp/smp.h"
#include "./global/kernel/task/task.h"
#include "./global/kernel/task/schedule.h"
#include "./global/kernel/ps2/keyboard.h"
#include "./global/kernel/ps2/mouse.h"
// Drivers
#include "./global/kernel/driver/device.h"
#include "./global/kernel/driver/pci.h"
#include "./global/kernel/driver/ahci.h"
#include "./global/kernel/driver/nvme.h"
// FS
#include "./global/kernel/fs/vfs.h"
extern "C"
{
#include "./global/kernel/pctable/idt.h"
#include "./efi.h"
}

#endif
