@echo off
cls
mkdir out
mkdir esp\EFI\BOOT
echo BOOTX64编译开始
..\tools\mingw64\bin\x86_64-w64-mingw32-gcc.exe -Wall -Wextra -e efi_main -nostdinc -nostdlib -fno-builtin -Wl,--subsystem,10 -o out\BOOTX64.efi boot\bootx64.c
echo BOOTX64编译结束
echo main.cpp编译开始
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-g++.exe kernel\main.cpp -O2 -Wall -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -c -o out\main.o
echo main.cpp编译结束
echo 链接内核文件开始
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-ld.exe -e KernelMain -z norelro -T linker.ld --static -o .\out\kernel.elf .\out\main.o
echo 链接内核文件结束
copy out\BOOTX64.efi esp\EFI\BOOT
copy out\kernel.elf esp
..\tools\qemu\qemu-system-x86_64.exe -bios ..\tools\OVMF.fd -m 512 -net none -drive file=fat:rw:esp,index=0,format=vvfat