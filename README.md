# XJ380(Code Singularity)操作系统
XINGJI工作室A级机密 | 保密编号XSEC202212011-11-A
## 开发者须知
1、严禁泄露源代码及非正式版镜像。<br>
2、Push代码请推送到自己的分支上。<br>
3、严禁擅自合并分支。<br>
4、编译环境为Linux（或WSL）。请严格按照规定配置开发环境。<br>
5、更新日志按照BV1作为规范（版本+【状态】 日期+用户名），状态分为 已实机测试 \ 已VM测试 \ 已虚拟机测试 \ 通过编译未测试 \ 未通过编译未测试 \ 暂未编译（如果实机运行不通过须使用VMWare进行测试）
## 环境配置
### 环境所用软件
- mingw-w64
- x86_64-elf-tools
- make
- qemu
- 剩下对着makefile装吧反正linux简单（bushi

## 更新日志

### BuildVersion 1 【已实机测试】
2024年8月3日 Guoqiyu1115
- 可以通过UEFI进行引导
- 可以输出固定字符串。

### BuildVersion 2 【已实机测试】
2024年8月4日 Guoqiyu1115
- 可以加载内核
- 写了个内核“kernel.elf”（kernel/main.c）
- 更新Makefile，可以直接通过虚拟机启动（需OVMF.fd，在tools里已经配置好）。
- 删除Makefile，可以直接通过脚本“build.bat”直接编译运行一条龙。

### BuildVersion 3 【已实机测试】
2024年8月8日 Guoqiyu1115
- 全彩画图搞定

### BuildVersion 4 【已实机测试】
2024年8月8日 Guoqiyu1115
- 优化画图算法

### BuildVersion 5 【已虚拟机测试】
2024年8月9日 wcjbr
- 拆代码

### BuildVersion 6 【已实机测试】
2024年8月9日 wcjbr & Guoqiyu1115
- 绘图函数
- 半个任务栏

### BuildVersion 7 【已实机测试】
2024年8月9日 wcjbr & Guoqiyu1115
- ASCII字符显示
- 任务栏及快捷栏

### BuildVersion 7 Upgrade 1
2024年8月10日 Guoqiyu1115
- 完善快捷栏

### BuildVersion 8 【已虚拟机测试】
2024年8月15日 copi143 & min0911Y & Guoqiyu1115 & wcjbr
- 将SystemTable传入内核
- 键盘输入
- 输入换行
- 整理头文件
- 整理史山代码
- 整理类型定义（详见stdint.h）
- 整理目录

### BuildVersion 9 【已实机测试】
2024年8月16日 Guoqiyu1115 & HlHill
- 成功获取鼠标输入
- 弄清楚了导致键盘输入卡住的元凶

### BuildVersion 10 【已实机测试】
2024年8月17日 Guoqiyu1115
- 鼠标数据输出

### BuildVersion 11 【已实机测试】
2024年8月22日 Guoqiyu1115
- 整理史山头文件
- 获取MemoryMap（为内存管理做准备）
- 输出MemoryMap（有BUG，给注释掉了）
- 修复ld脚本和编译脚本的隐藏bug

### BuildVersion 12 【已虚拟机测试】
2024年8月23日 Guoqiyu1115 & 绿灯
- 输出MemoryMap

### BuildVersion 13 【已实机测试】
2024年9月8日 Guoqiyu1115 & chezixiang
- 修复实机无法输出MemoryMap的问题

### BuildVersion 14 【已实机测试】
2024年9月15日 jje & Guoqiyu1115
- 内存合并
- 修复了内存合并的BUG
- 输出内存容量

### BuildVersion 15 【已实机测试】
2024年9月16日 Guoqiyu1115
- 修复了把BS也分类为free的BUG
- 整理debug数据布局
- 增加十进制输出函数

### BuildVersion 16 【已实机测试】
2024年9月21日 copi143
- 内存管理

### BuildVersion 17 【已虚拟机测试】
2024年9月28日 Guoqiyu1115
- 可以调用Runtime的东东了
- 电池信息

### BuildVersion 18 【已虚拟机测试】
2024年9月30日 Guoqiyu1115
- 图层的分配刷新与释放

### BuildVersion 19 【已虚拟机测试】
2024年10月6日 Guoqiyu1115
- 调整分辨率

### BuildVersion 20 【已虚拟机测试】
2024年10月19日 Guoqiyu1115
- 图层及透明

### BuildVersion 21 【已实机测试】
2024年10月19日 Guoqiyu1115
- 鼠标指针

### BuildVersion 22 【已实机测试】
2024年10月22日 Guoqiyu1115
- 修复了鼠标指针溢出的问题

### BuildVersion 23 【已实机测试】
2024年10月25日 Guoqiyu1115
- 软关机.exe

### BuildVersion 24 【已虚拟机测试】
2024年11月9日 Guoqiyu1115
- 除了国企鱼没人想你，BootServices

### BuildVersion 25 【已虚拟机测试】
2024年11月10日 Guoqiyu1115
- GDT搞定（有bug）
- 国企鱼也不想你，BootServices

### BuildVersion 26 【已实机测试】
2024年11月10日 Guoqiyu1115
- IDT搞定

### BuildVersion 27 【已虚拟机测试】
2024年11月23日 Guoqiyu1115
- 修好了GDT
- 新的WSOD（白屏死机）界面

### BuildVersion 28 【已VM测试】
2024年11月30日 Guoqiyu1115
- 修好了GDT
- 8259A（临时用）
- 整理文件

### BuildVersion 29 【已虚拟机测试】
2024年12月7日 Guoqiyu1115
- 成功从ACPI获取MADT（VMware无法引导）

### BuildVersion 29.1 【已实机测试】
2024年12月7日 Guoqiyu1115
- 成功从ACPI获取MADT（国企鱼事傻逼）

### BuildVersion 30 【已虚拟机测试】
2024年12月28日 Guoqiyu1115
- 成功从ACPI获取MADT（VMware无法引导，炸PF）
- IO APIC设置成功（大抵是吧）

### BuildVersion 31 【已虚拟机测试】
2025年1月20日 XIAOYI12
- 增加Hpet高精计时器解析,并成功获取 nanoTime

### BuildVersion 32 【已虚拟机测试】
2025年1月21日 XIAOYI12 & CodeCube
- 支持x2APIC (LocalAPIC/IO APIC)
- 启用APIC时钟中断

### BuildVersion 33 【已虚拟机测试】
2025年1月22日 Guoqiyu1115 & CodeCube
- 键盘

### BuildVersion 34 【已虚拟机测试】
2025年1月23日 Guoqiyu1115 & CodeCube
- 鼠标
- 新的内存管理

### BuildVersion 35 【已虚拟机测试】
2025年1月25日 Guoqiyu1115
- 修复了图层管理在重启后卡死循环的bug

### BuildVersion 36 【已虚拟机测试】
2025年1月31日 VOID
- SMP

### BuildVersion 37 【已虚拟机测试】
2025年1月31日 VOID
- 重新映射页表

### BuildVersion 38 【已虚拟机测试】
2025年2月1日 VOID
- 异常回溯

### BuildVersion 39 【已虚拟机测试】
2025年2月1日 VOID
- 进程调度

### BuildVersion 40 【已虚拟机测试】
2025年2月1日 VOID
- 多任务
- 内核提升至高地址

### BuildVersion 41 【已虚拟机测试】
2025年2月3日 VOID & Code-Cube
- 修复了亿点点bug
- 更换为makefile

### BuildVersion 42 【已虚拟机测试】
2025年2月3日 VOID
- PCI驱动
- AHCI驱动

### BuildVersion 43 【已虚拟机测试】
2025年2月3日 VOID
- PCIe驱动
- NVMe驱动

### BuildVersion 44 【已虚拟机测试】
2025年2月4日 VOID & Code-Cube
- 修复了亿些warning
- 封装了通用中断处理函数

### BuildVersion 45 【已虚拟机测试】
2025年2月4日 VOID & Code-Cube
- 修复了亿些warning
- 封装了通用中断处理函数

### BuildVersion 46 【已虚拟机测试】
2025年2月4日 VOID
- VFS
- 文件系统（FAT32）

### BuildVersion 47 【已虚拟机测试】
2025年2月6日 VOID
- syscall

### BuildVersion 48 【已虚拟机测试】
2025年3月16日 VOID
- 修复硬盘驱动（execve死循环bug）

### BuildVersion 49 【已虚拟机测试】
2025年4月4日 人造人
- 修复一堆bug

### BuildVersion 50 【已虚拟机测试】
2025年4月5日 Guoqiyu1115
- 修复图层的bug
- 创建窗口

### BuildVersion 51 【已虚拟机测试】
2025年4月6日 Guoqiyu1115
- 移动窗口

### BuildVersion 52 【已虚拟机测试】
2025年4月7日 VOID & Guoqiyu1115
- 更好的内存管理
- 修复了一些bug
- 加了个字符串防止有人以为XJ380卡在哪了

### BuildVersion 53 【已虚拟机测试】
2025年5月5日 Guoqiyu1115
- 矢量字体（半透明背景时，混色有问题）

## 中断号分配表
### 0   ~   31	异常
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
    