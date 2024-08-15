# XJ380(Code Singularity)操作系统
## 开发者须知
1、严禁泄露源代码及镜像。<br>
2、Push代码请推送到自己的分支上。<br>
3、严禁擅自合并分支。<br>
4、main分支为主分支，每个主分支版本要新建一个分支专门储存。<br>
5、编译环境为Windows10\11。请严格按照规定配置开发环境。<br>
6、更新日志按照BV1作为规范（版本+【状态】 日期+用户名），状态分为 已实机测试 \ 已虚拟机测试 \ 通过编译未测试 \ 未通过编译未测试 \ 暂未编译
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
