# XJ380(Code Singularity)操作系统

XINGJI 工作室 A 级机密 | 保密编号 XSEC202212011-11-A

> 本文档用于开发者快速搭建环境、配置内核与编译镜像。配置系统已迁移到 Kconfig，
> 旧的 build_settings.h 手写宏仅作为兜底默认值，正式配置请以 Kconfig / .config 为准。

## 开发者须知

1. 严禁泄露源代码及非正式版镜像。
2. Push 代码请推送到自己的分支上。
3. 严禁擅自合并分支。
4. 编译环境为 Linux（或 WSL）。请严格按照规定配置开发环境。
5. 更新日志按照 BV1 作为规范（版本 +【状态】+ 日期 + 用户名），状态分为：
   已实机测试 / 已 VM 测试 / 已虚拟机测试 / 通过编译未测试 / 未通过编译未测试 / 暂未编译
   （如果实机运行不通过须使用 VMWare 进行测试）。

## 1. 环境配置

### 1.1 软件依赖

```bash
sudo apt update -y
# 交叉编译工具链（内核 / 驱动 / UEFI 引导均用 clang + lld）
sudo apt install clang lld -y
# UEFI 引导程序 mingw 目标（也可用 clang --target=x86_64-w64-mingw32 替代）
sudo apt install gcc-mingw-w64-x86-64 -y
# ESP 分区（FAT）镜像：用 mtools 的 mformat/mmd/mcopy 写入，无需 dosfstools 的 mkfs.vfat
sudo apt install mtools -y
# 根文件系统（ext4）
sudo apt install e2fsprogs -y
# 磁盘分区表（sgdisk）
sudo apt install gdisk -y
# 运行 / 调试
sudo apt install qemu-system-x86_64 -y
# Kconfig 前端（menuconfig 需要 ncurses；conf/mconf 构建需要 bison/flex）
sudo apt install bison flex libncursesw-dev libtinfo-dev -y
# 准备根文件系统（按需：prepare/stage 阶段从 xbps 拉取，可省略用于纯内核验证）
sudo apt install xbps -y
```

### 1.2 子模块

```bash
git submodule update --init frameworks/StardustUI third_party/lwext4
```

> 若 StardustUI 锁定提交在远端已不存在，请先将其更新到可用提交后再编译。

## 2. 配置系统（Kconfig）

配置由 `Kconfig` 描述，使用从 **Linux v6.12** 提取的 kconfig 工具链
（`tools/kconfig/`，许可证 GPL-2.0，见仓库顶层 `LICENSE`）生成配置。

- `.config`：人类可读的内核配置（由 Kconfig 生成）。
- `kernel/build_config.h`：**自动生成**，根据 `.config` 导出所有 `CONFIG_*`
  宏，由内核编译消费（该文件已被 git 忽略，请勿手工修改）。

常用目标：

| 命令 | 作用 |
| --- | --- |
| `make defconfig` | 套用 `configs/xj380_defconfig`，生成 `.config` 与 `build_config.h` |
| `make menuconfig` | 基于 ncurses 的交互式配置（需 libncursesw） |
| `make config` | 基于文本行的交互式配置 |
| `make oldconfig` | 在已有 `.config` 基础上套用新 `Kconfig` 的默认值 |
| `make savedefconfig` | 将当前 `.config` 回写到 `configs/xj380_defconfig` |

修改配置项的方式：

- 新增 / 调整可配置选项：编辑 `Kconfig`（选项定义、菜单、类型、默认值）。
- 调整默认配置：编辑 `configs/xj380_defconfig`，然后 `make defconfig` 应用，
  或改完 `menuconfig` 后 `make savedefconfig` 固化到 defconfig。

版本号、栈 / 堆大小、各类内核特性开关（如 `CONFIG_KERNEL_PROFILER`、
`CONFIG_KERNEL_BUSYBOX_ALIASES` 等）现在都属于 Kconfig 选项，
不再需要手改 `build_settings.h` 中的宏。

## 3. 编译

```bash
make all -j"$(nproc)"
```

产物为 `XJ380.img`，是一个**双分区**磁盘镜像：

- **分区 1（`ef00` ESP，FAT）**：UEFI 固件加载项所在分区。包含
  `EFI/BOOT/BOOTX64.efi`（引导程序）与 `system/kernel.elf`（内核），
  由 `mtools`（`mformat`/`mmd`/`mcopy`）写入。
- **分区 2（`8300` rootfs，ext4）**：系统根文件系统，由 `mkfs.ext4` 生成。

> 标准 UEFI 固件（如 OVMF）只能从 FAT ESP 加载启动项，因此引导程序必须放在 FAT
> 分区；而系统根文件系统使用 ext4，避免依赖 vfat / dosfstools 的 mkfs.vfat。

## 4. 运行

```bash
make run          # 用 qemu-system-x86_64 启动 XJ380.img
```

其它镜像目标：`make vdisk`（生成 raw 镜像）、`make vmdk`（转 vmdk）。

## 5. 更新版本信息

版本相关字符串（发行代号、内核版本、完整 OS 版本）已是 Kconfig 选项：

- `CONFIG_OS_EDITION`
- `CONFIG_KN_VERSION`
- `CONFIG_OS_VERSION`

通过 `make menuconfig` 修改后 `make savedefconfig` 固化即可，无需再手改
`build_settings.h` 或 `stage_image_base.sh`。

## 6. 许可证

- 仓库整体以 **GNU GPL v2** 分发，见顶层 `LICENSE`。
- `tools/kconfig/` 逐字取自 Linux v6.12 `scripts/kconfig`，许可证为
  `GPL-2.0-only`，详见 `tools/kconfig/README` 与 `THIRD_PARTY_NOTICES.md`。
- 第三方组件（lwIP、mbedTLS、FatFs、lwext4 等）保持各自上游许可证，
  见 `THIRD_PARTY_NOTICES.md`。
