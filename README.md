# XJ380(Code Singularity)操作系统

XINGJI 工作室 A 级机密 | 保密编号 XSEC202212011-11-A

## 开发者须知

1、严禁泄露源代码及非正式版镜像。<br>
2、Push 代码请推送到自己的分支上。<br>
3、严禁擅自合并分支。<br>
4、编译环境为 Linux（或 WSL）。请严格按照规定配置开发环境。<br>
5、更新日志按照 BV1 作为规范（版本+【状态】 日期+用户名），状态分为 已实机测试 \ 已 VM 测试 \ 已虚拟机测试 \ 通过编译未测试 \ 未通过编译未测试 \ 暂未编译（如果实机运行不通过须使用 VMWare 进行测试）

## 环境配置

### 环境所用软件

- mingw-w64 （为了编译 BOOTX64.efi<!--So microsoft, fuck you!-->）
- x86_64-elf-tools
- make
- qemu
- 剩下对着 makefile 装吧反正 linux 简单（bushi

## Source Layout

- `boot/`: UEFI bootloader code
- `kernel/`: core kernel subsystems
- `driver/`: built-in drivers linked into `kernel.elf`
- `kmod/`: loadable kernel modules built as `.km`
- `user/`: user-space programs and XAPI runtime

Notes:

- The build still installs kernel modules into `/lib/modules` inside the image.
- `mod/Makefile` is kept as a compatibility wrapper for older build habits.

## 更新日志

### BuildVersion 1 【已实机测试】

2024 年 8 月 3 日 Guoqiyu1115

- 可以通过 UEFI 进行引导
- 可以输出固定字符串。

### BuildVersion 2 【已实机测试】

2024 年 8 月 4 日 Guoqiyu1115

- 可以加载内核
- 写了个内核“kernel.elf”（kernel/main.c）
- 更新 Makefile，可以直接通过虚拟机启动（需 OVMF.fd，在 tools 里已经配置好）。
- 删除 Makefile，可以直接通过脚本“build.bat”直接编译运行一条龙。

### BuildVersion 3 【已实机测试】

2024 年 8 月 8 日 Guoqiyu1115

- 全彩画图搞定

### BuildVersion 4 【已实机测试】

2024 年 8 月 8 日 Guoqiyu1115

- 优化画图算法

### BuildVersion 5 【已虚拟机测试】

2024 年 8 月 9 日 wcjbr

- 拆代码

### BuildVersion 6 【已实机测试】

2024 年 8 月 9 日 wcjbr & Guoqiyu1115

- 绘图函数
- 半个任务栏

### BuildVersion 7 【已实机测试】

2024 年 8 月 9 日 wcjbr & Guoqiyu1115

- ASCII 字符显示
- 任务栏及快捷栏

### BuildVersion 7 Upgrade 1

2024 年 8 月 10 日 Guoqiyu1115

- 完善快捷栏

### BuildVersion 8 【已虚拟机测试】

2024 年 8 月 15 日 copi143 & min0911Y & Guoqiyu1115 & wcjbr

- 将 SystemTable 传入内核
- 键盘输入
- 输入换行
- 整理头文件
- 整理史山代码
- 整理类型定义（详见 stdint.h）
- 整理目录

### BuildVersion 9 【已实机测试】

2024 年 8 月 16 日 Guoqiyu1115 & HlHill

- 成功获取鼠标输入
- 弄清楚了导致键盘输入卡住的元凶

### BuildVersion 10 【已实机测试】

2024 年 8 月 17 日 Guoqiyu1115

- 鼠标数据输出

### BuildVersion 11 【已实机测试】

2024 年 8 月 22 日 Guoqiyu1115

- 整理史山头文件
- 获取 MemoryMap（为内存管理做准备）
- 输出 MemoryMap（有 BUG，给注释掉了）
- 修复 ld 脚本和编译脚本的隐藏 bug

### BuildVersion 12 【已虚拟机测试】

2024 年 8 月 23 日 Guoqiyu1115 & 绿灯

- 输出 MemoryMap

### BuildVersion 13 【已实机测试】

2024 年 9 月 8 日 Guoqiyu1115 & chezixiang

- 修复实机无法输出 MemoryMap 的问题

### BuildVersion 14 【已实机测试】

2024 年 9 月 15 日 jje & Guoqiyu1115

- 内存合并
- 修复了内存合并的 BUG
- 输出内存容量

### BuildVersion 15 【已实机测试】

2024 年 9 月 16 日 Guoqiyu1115

- 修复了把 BS 也分类为 free 的 BUG
- 整理 debug 数据布局
- 增加十进制输出函数

### BuildVersion 16 【已实机测试】

2024 年 9 月 21 日 copi143

- 内存管理

### BuildVersion 17 【已虚拟机测试】

2024 年 9 月 28 日 Guoqiyu1115

- 可以调用 Runtime 的东东了
- 电池信息

### BuildVersion 18 【已虚拟机测试】

2024 年 9 月 30 日 Guoqiyu1115

- 图层的分配刷新与释放

### BuildVersion 19 【已虚拟机测试】

2024 年 10 月 6 日 Guoqiyu1115

- 调整分辨率

### BuildVersion 20 【已虚拟机测试】

2024 年 10 月 19 日 Guoqiyu1115

- 图层及透明

### BuildVersion 21 【已实机测试】

2024 年 10 月 19 日 Guoqiyu1115

- 鼠标指针

### BuildVersion 22 【已实机测试】

2024 年 10 月 22 日 Guoqiyu1115

- 修复了鼠标指针溢出的问题

### BuildVersion 23 【已实机测试】

2024 年 10 月 25 日 Guoqiyu1115

- 软关机.exe

### BuildVersion 24 【已虚拟机测试】

2024 年 11 月 9 日 Guoqiyu1115

- 除了国企鱼没人想你，BootServices

### BuildVersion 25 【已虚拟机测试】

2024 年 11 月 10 日 Guoqiyu1115

- GDT 搞定（有 bug）
- 国企鱼也不想你，BootServices

### BuildVersion 26 【已实机测试】

2024 年 11 月 10 日 Guoqiyu1115

- IDT 搞定

### BuildVersion 27 【已虚拟机测试】

2024 年 11 月 23 日 Guoqiyu1115

- 修好了 GDT
- 新的 WSOD（白屏死机）界面

### BuildVersion 28 【已 VM 测试】

2024 年 11 月 30 日 Guoqiyu1115

- 修好了 GDT
- 8259A（临时用）
- 整理文件

### BuildVersion 29 【已虚拟机测试】

2024 年 12 月 7 日 Guoqiyu1115

- 成功从 ACPI 获取 MADT（VMware 无法引导）

### BuildVersion 29.1 【已实机测试】

2024 年 12 月 7 日 Guoqiyu1115

- 成功从 ACPI 获取 MADT（国企鱼事傻逼）

### BuildVersion 30 【已虚拟机测试】

2024 年 12 月 28 日 Guoqiyu1115

- 成功从 ACPI 获取 MADT（VMware 无法引导，炸 PF）
- IO APIC 设置成功（大抵是吧）

### BuildVersion 31 【已虚拟机测试】

2025 年 1 月 20 日 XIAOYI12

- 增加 Hpet 高精计时器解析,并成功获取 nanoTime

### BuildVersion 32 【已虚拟机测试】

2025 年 1 月 21 日 XIAOYI12 & CodeCube

- 支持 x2APIC (LocalAPIC/IO APIC)
- 启用 APIC 时钟中断

### BuildVersion 33 【已虚拟机测试】

2025 年 1 月 22 日 Guoqiyu1115 & CodeCube

- 键盘

### BuildVersion 34 【已虚拟机测试】

2025 年 1 月 23 日 Guoqiyu1115 & CodeCube

- 鼠标
- 新的内存管理

### BuildVersion 35 【已虚拟机测试】

2025 年 1 月 25 日 Guoqiyu1115

- 修复了图层管理在重启后卡死循环的 bug

### BuildVersion 36 【已虚拟机测试】

2025 年 1 月 31 日 VOID

- SMP

### BuildVersion 37 【已虚拟机测试】

2025 年 1 月 31 日 VOID

- 重新映射页表

### BuildVersion 38 【已虚拟机测试】

2025 年 2 月 1 日 VOID

- 异常回溯

### BuildVersion 39 【已虚拟机测试】

2025 年 2 月 1 日 VOID

- 进程调度

### BuildVersion 40 【已虚拟机测试】

2025 年 2 月 1 日 VOID

- 多任务
- 内核提升至高地址

### BuildVersion 41 【已虚拟机测试】

2025 年 2 月 3 日 VOID & Code-Cube

- 修复了亿点点 bug
- 更换为 makefile

### BuildVersion 42 【已虚拟机测试】

2025 年 2 月 3 日 VOID

- PCI 驱动
- AHCI 驱动

### BuildVersion 43 【已虚拟机测试】

2025 年 2 月 3 日 VOID

- PCIe 驱动
- NVMe 驱动

### BuildVersion 44 【已虚拟机测试】

2025 年 2 月 4 日 VOID & Code-Cube

- 修复了亿些 warning
- 封装了通用中断处理函数

### BuildVersion 45 【已虚拟机测试】

2025 年 2 月 4 日 VOID & Code-Cube

- 修复了亿些 warning
- 封装了通用中断处理函数

### BuildVersion 46 【已虚拟机测试】

2025 年 2 月 4 日 VOID

- VFS
- 文件系统（FAT32）

### BuildVersion 47 【已虚拟机测试】

2025 年 2 月 6 日 VOID

- syscall

### BuildVersion 48 【已虚拟机测试】

2025 年 3 月 16 日 VOID

- 修复硬盘驱动（execve 死循环 bug）

### BuildVersion 49 【已虚拟机测试】

2025 年 4 月 4 日 人造人

- 修复一堆 bug

### BuildVersion 50 【已虚拟机测试】

2025 年 4 月 5 日 Guoqiyu1115

- 修复图层的 bug
- 创建窗口

### BuildVersion 51 【已虚拟机测试】

2025 年 4 月 6 日 Guoqiyu1115

- 移动窗口

### BuildVersion 52 【已虚拟机测试】

2025 年 4 月 7 日 VOID & Guoqiyu1115

- 更好的内存管理
- 修复了一些 bug
- 加了个字符串防止有人以为 XJ380 卡在哪了

### BuildVersion 53 【已虚拟机测试】

2025 年 5 月 5 日 Guoqiyu1115

- 矢量字体

### BuildVersion 54 【已虚拟机测试】

2025 年 5 月 10 日 Guoqiyu1115

- XAPI 准备完毕（可以开始编写实现）

### BuildVersion 55 【已虚拟机测试】

2025 年 5 月 20 日 Guoqiyu1115

- XAPI GUI 大部分完成
- 修复了读写文件有概率失败或 PF 的 bug
- 修改开机动画

### BuildVersion 56 【已虚拟机测试】

2025 年 5 月 20 日 Guoqiyu1115

- 在 debug 模式下将串口打印至屏幕

### BuildVersion 57 【已虚拟机测试】

2025 年 6 月 14 日 Guoqiyu1115

- stbi（未测试，可过编译）
- 及其生草未经测试的 stdio（文件）
- 修复了莫名其妙卡住的 bug
- 优化了一丢丢内存管理的初始化速度

### BuildVersion 58 【已虚拟机测试】

2025 年 6 月 22 日 Guoqiyu1115

- stbir（未测试，可过编译）

## 中断号分配表

### 0 ~ 31 异常

    0	devide error
    1	debug
    2	NMI
    3	breakpoint
    4	overflow
    5	bound range
    6	undefined opcode
    7	device	not available
    8	double fault
    9	coprocessor segment overrun
    10	invalid TSS
    11	segment not present
    12	stack segment fault
    13	general protection
    14	page fault
    15
    16	x87 FPU error
    17	alignment check
    18	machine check
    19	SIMD exception
    20	virtualization exception

    32 	apic timer
    33 	keyboard
    34 	mouse
