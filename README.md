# XJ380(Code Singularity)操作系统

XINGJI 工作室 A 级机密 | 保密编号 XSEC202212011-11-A

## 开发者须知

1. 严禁泄露源代码及非正式版镜像。
2. Push 代码请推送到自己的分支上。
3. 严禁擅自合并分支。
4. 编译环境为 Linux（或 WSL）。请严格按照规定配置开发环境。
5. 更新日志按照 BV1 作为规范（版本+【状态】 日期+用户名），状态分为 已实机测试 \ 已 VM 测试 \ 已虚拟机测试 \ 通过编译未测试 \ 未通过编译未测试 \ 暂未编译（如果实机运行不通过须使用 VMWare 进行测试）

## 环境配置

### 环境所用软件

```bash
sudo apt update -y
sudo apt install clang -y
sudo apt install g++ -y
sudo apt install gcc-mingw-w64-x86-64 -y
sudo apt install mtools dosfstools -y
sudo apt install qemu-system -y
```

## 编译

```bash
make all -j"$(nproc)"
```

## 更新版本信息

需要手动更改 ``stage_image_base.sh`` 和 ``build_settings.h`` 。
