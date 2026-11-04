# 保密标记清查记录（Security / Confidentiality Markers Audit）

> 本文件记录 XJ380(Code Singularity) 代码仓库中所有“保密 / 机密 / 严禁泄露”类标记的扫描结果、位置与处理结论，供后人查阅。

## 背景与结论

本项目已采用 **GNU GPL v2**（见 `LICENSE`）作为开源许可证。GPLv2 是强Copyleft许可证，
其核心是向所有人授予源代码与镜像的复制、分发、修改权利。这与“项目保密 / 严禁泄露源代码”
之类的声明**直接冲突**——在 GPLv2 下不能再对源代码和镜像设置保密限制。

经决策：**放弃一切保密限制**，源码与镜像按 GPLv2 许可证公开分发。
因此，仓库内所有表达“保密 / 机密 / 不对外开放 / 严禁泄露”的项目自有标记均已移除或改写。
下述第三方代码中的同类字眼属于上游许可证或密码学术语，**不在移除范围内**，须保留。

---

## 一、已清除的项目自有保密标记（Genuine project markers — REMOVED）

| 原位置（文件:行） | 原文 | 处理方式 | 现状 |
|---|---|---|---|
| `README.md:3` | `XINGJI 工作室 A 级机密 | 保密编号 XSEC202212011-11-A` | 删除标题中的机密/保密编号 | 已移除（提交 `51c0224c`） |
| `README.md:10`（开发者须知 第1条） | `1. 严禁泄露源代码及非正式版镜像。` | 删除该条；其余须知保留 | 已移除（提交 `51c0224c`） |
| `AGENTS.md:74`（原 ANTI-PATTERNS 首条） | `Do not leak source or non-official images; \`README.md\` marks the project confidential.` | 改写为：`源码与镜像按 GPLv2 许可证分发（见 LICENSE），不再设保密限制。` | 已改写（工作区已改，待提交） |
| `include/syscall/pxapi.h:40` | `// 一些隐藏的API，不对外开放，严禁泄露！` | 整行删除 | 已移除（工作区已改，待提交） |
| `kernel/syscall/syscall.cpp:916` | `/* XJ380API 隐藏版（严禁泄露） */` | 整行删除 | 已移除（工作区已改，待提交） |
| `user/bcms-sp.cpp:5` | `xapi_Printf("%s 哈机密\n","gulu");` | 玩笑串，改为 `xapi_Printf("%s\n","gulu");` | 已移除（工作区已改，待提交） |

> 说明：`pxapi.h:32` 的 `// 以下成员隐藏` 与 `syscall.cpp` 中的“隐藏版”不同，前者是描述
> 结构体成员对应用层不可见的**技术注释**，非保密声明，予以保留。

---

## 二、保留的第三方声明（Third-party — MUST KEEP）

以下命中项**不移除**，理由：它们是上游开源组件的许可证头或密码学文档术语，与“项目保密”无关。
移除会违反相应组件的许可证要求。

- `third_party/mbedtls-src/**`：大量 `confidential` 出现在密码学文档/注释中（如“data confidentiality”
  “wipe confidential data”），是安全属性术语，非项目保密声明。
- `third_party/lwext4/**`、`kmod/netserver/lwip/**`：`* All rights reserved.` 为上游 BSD 类许可证头。
- `user/browser/third_party/libwebp/**`：`// Copyright ... Google Inc. All Rights Reserved.` 为上游许可证头。
- `third_party/mbedtls-src/README.md` / `BUGS.md`：提及“report confidentially”指安全漏洞上报渠道，保留。

---

## 三、项目自有版权头 “All rights reserved”（保留，非保密标记）

以下文件含有 `XINGJI Interactive Software ... All rights reserved.` 之类的版权头：
`user/xapi/*`、`user/shell/shell_main.cpp`、`user/shell/shell_cmd.cpp`、`user/installer/installer.cpp`、
`boot/bootx64.c`、`include/user/x3tp.h` 等。

这些是**版权归属声明**，并非“保密/不得分发”的标记。在 GPLv2 下保留版权头没有问题，
实际的授权与分发条款以根目录 `LICENSE`（GPLv2）为准。故予以保留，仅作记录。

---

## 四、技术性 “hidden / secret” 用法（非保密标记，保留）

扫描中还命中若干 `hidden` / `secret`，均属技术语义，与项目保密无关：

- `include/elf.h`、`kernel/dlinker.cpp`、`kmod/AGENTS.md`：ELF 符号可见性 `STV_HIDDEN` / `-fvisibility=hidden`。
- `kmod/netserver/lwip/netif/ppp/**`：`secret` 是 PPP/CHAP/L2TP 的**密码字**变量名。
- `user/installer/installer.cpp`：`#include "installer_hidden.h"` 为安装器内部实现文件名。
- `include/fs/fatfs/ff.h`：`AM_HID 0x02 /* Hidden */` 是 FAT 文件属性宏。

以上一律保留。

---

## 五、扫描命令与复查方法

如需重新扫描，可运行：

```bash
# 项目保密类断言
grep -rniE '严禁泄露|不对外开放|机密|保密编号|隐藏版|泄露源代码' --include='*.md' --include='*.cpp' --include='*.h' --include='*.c' .

# 第三方许可证头（应保留）
grep -rniE 'All rights reserved|confidential' third_party/ user/browser/third_party/
```

结论：当前工作区中，所有“项目级保密/机密/严禁泄露”标记已清除；第三方许可证与密码学术语原样保留。
源码与镜像自本提交起按 GPLv2 公开分发。
