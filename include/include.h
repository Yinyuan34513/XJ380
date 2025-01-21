// 版权所有©XINGJI Studios 2017-2024 保留所有权利。
// XJ380 C++头文件头文件
#ifndef _INCLUDE_HPP_
#define _INCLUDE_HPP_

#include "./cppinc/GOP.hpp"
#include "./global/boot.h"
#include "./cppinc/sheet.h"
#include "./cppinc/proto.hpp"
#include "./global/stdint.h"
#include "./global/graphics/global_color.h"
#include "./global/kernel/memory.h"
#include "./global/kernel/pctable/gdt.h"
#include "./global/kernel/longm.h"
#include "./global/kernel/acpi.h"
#include "./global/kernel/apic/madt.h"
#include "./global/kernel/apic/hpet.h"
#include "./global/kernel/apic/apic.h"

extern "C" {
#include "./global/kernel/pctable/idt.h"
#include "./efi.h"
}

#endif 
