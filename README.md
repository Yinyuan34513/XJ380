# XJ380(Code Singularity)操作系统
XINGJI工作室A级机密 | 保密编号XSEC202212011-11-A
## 开发者须知
1、严禁泄露源代码及非正式版镜像。<br>
2、Push代码请推送到自己的分支上。<br>
3、严禁擅自合并分支。<br>
4、main分支为主分支，每个主分支版本要新建一个分支专门储存。<br>
5、编译环境为Windows10\11。请严格按照规定配置开发环境。<br>
6、更新日志按照BV1作为规范（版本+【状态】 日期+用户名），状态分为 已实机测试 \ 已VM测试 \ 已虚拟机测试 \ 通过编译未测试 \ 未通过编译未测试 \ 暂未编译（如果实机运行不通过须使用VMWare进行测试）
## 环境配置
### 环境所用软件
- mingw-w64
- x86_64-elf-tools
- make
- qemu
- dd
### make
1、去这里下载并安装make(安装至默认路径)：<br>
https://sourceforge.net/projects/gnuwin32/files/make/3.81/make-3.81.exe/download?use_mirror=jaist&download=
### 其他
已经配置好放在tools里了。
### 环境变量
1、在搜索框搜索“编辑系统环境变量”<br>
2、选择“环境变量”<br>
3、双击“Path”一栏<br>
4、选择“新建”<br>
5、输入路径<br>
6、重复第4 5步
路径：
- C:\Program Files (x86)\GnuWin32\bin
## 编译须知
make：编译并运行。<br>
make mkrel：编译并输出至文件夹（需手动创建并复制文件至镜像）。<br>
make clear：清空编译出来的所有文件。<br>
<br>
Makefile里有一个变量“REL”，填版本。
## 版本命名规则
日常测试版：XJ380_DEBUG<br>
测试完成的构建基础功能版：XJ380_BUILD_V<填版本，例如BuildVersion2就是V2><br>
测试完成的开发版：XJ380_BETA_V<同样填版本><br>
正式版：XJ380_RELEASE_V<大版本号><br>
<br>
自己玩的话愿意叫啥叫啥。

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

### 21  ~   31	没用（算是保留）

### 32  ~   55	I/O APIC
    32	8259A
    33	keyboard
    34	HPET timer 0,8254 counter 0
    35	serial port A
    36	serial port B
    37	parallel port
    38	floppy
    39	parallel port
    40	RTC,HPET timer 1
    41	Generic
    42	Generic
    43	HPET timer 2
    44	HPET timer 3	/ mouse
    45	FERR#
    46	SATA primary
    47	SATA secondary
    48	PIRQA
    49	PIRQB
    50	PIRQC
    51	PIRQD
    52	PIRQE
    53	PIRQF
    54	PIRQG
    55	PIRQH

### 56 ~    149 暂时没用

### 150 ~   200	Local APIC
    150	CMCI
    151	Timer
    152	Thermal Monitor
    153	Performance Counter
    154	LINT0
    155	LINT1
    156	Error
    157 xhci_controller_0
    158 xhci_controller_1
    159 xhci_controller_2
    160 xhci_controller_3

### 200 ~   255	MP IPI
