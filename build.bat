@echo off
chcp 65001
cls
del /Q /S out\*
del /Q /S esp\*
mkdir out
mkdir esp\EFI\BOOT
echo 开始编译boot
..\tools\mingw64\bin\x86_64-w64-mingw32-gcc.exe -Wall -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-unused-variable -Wextra -e efi_main -nostdinc -nostdlib -fno-builtin -Wl,--subsystem,10 -o out\BOOTX64.efi boot\bootx64.c
echo boot编译完毕
echo main.cpp开始编译
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-g++.exe kernel\main.cpp -O2 -Wall -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-unused-variable -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -c -o out\main.o
echo main.cpp编译结束
echo draw.cpp开始编译
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-g++.exe graphics\draw.cpp -O2 -Wall -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-unused-variable -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -c -o out\draw.o
echo draw.cpp编译结束
echo font.cpp开始编译
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-g++.exe font\font.cpp -O2 -Wall -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-unused-variable -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -c -o out\font.o
echo font.cpp编译结束
echo desktop.cpp开始编译
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-g++.exe graphics\desktop.cpp -O2 -Wall -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-unused-variable -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -c -o out\desktop.o
echo desktop.cpp编译结束
echo memory.cpp开始编译
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-g++.exe kernel\memory.cpp -O2 -Wall -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -c -o out\memory.o
echo memory.cpp编译结束
echo wsod.cpp开始编译
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-g++.exe kernel\wsod.cpp -O2 -Wall -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -c -o out\wsod.o
echo wsod.cpp编译结束
echo 开始链接内核
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-ld.exe -e KernelMain -z norelro -T linker.ld --static -o .\out\kernel.elf .\out\main.o .\out\draw.o .\out\font.o .\font\hankaku.o .\out\desktop.o .\out\memory.o .\out\wsod.o
echo 内核链接结束
copy out\BOOTX64.efi esp\EFI\BOOT
copy test.bmp esp\EFI\BOOT
copy out\kernel.elf esp
..\tools\qemu\qemu-system-x86_64.exe -bios ..\tools\OVMF.fd -m 1024 -net none -drive file=fat:rw:esp,index=0,format=vvfat -serial file:serial.log -device usb-ehci,id=usb -device usb-tablet,bus=usb.0 -device usb-kbd,bus=usb.0