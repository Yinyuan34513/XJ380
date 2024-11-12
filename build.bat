@echo off
chcp 65001
cls
del /Q /S out\*
del /Q /S esp\*
mkdir out
mkdir esp\EFI\BOOT
..\tools\mingw64\bin\x86_64-w64-mingw32-gcc.exe -Wall -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-unused-variable -Wextra -e efi_main -nostdinc -nostdlib -fno-builtin -Wl,--subsystem,10 -o out\BOOTX64.efi boot\bootx64.c
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-g++.exe kernel\main.cpp -O3 -Wall -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-unused-variable -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -c -o out\main.o
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-g++.exe kernel\pctable\gdt.cpp -O3 -Wall -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -c -o out\gdt.o
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-g++.exe kernel\pctable\idt.cpp -O3 -Wall -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -c -o out\idt.o
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-g++.exe kernel\asm\common.cpp -O3 -Wall -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -c -o out\common.o
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-g++.exe kernel\cpu\apic.cpp -O3 -Wall -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -c -o out\apic.o
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-g++.exe graphics\draw.cpp -O3 -Wall -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-unused-variable -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -c -o out\draw.o
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-g++.exe font\font.cpp -O3 -Wall -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-unused-variable -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -c -o out\font.o
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-g++.exe graphics\desktop.cpp -O3 -Wall -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-unused-variable -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -c -o out\desktop.o
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-g++.exe graphics\mouse.cpp -O3 -Wall -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-unused-variable -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -c -o out\mouse.o
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-g++.exe graphics\draw_sheet.cpp -O3 -Wall -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-unused-variable -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -c -o out\draw_sheet.o
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-g++.exe kernel\sheet.cpp -O3 -Wall -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-unused-variable -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -c -o out\sheet.o
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-g++.exe kernel\memorym.cpp -O3 -Wall -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -c -o out\memorym.o
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-g++.exe kernel\wsod.cpp -O3 -Wall -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -c -o out\wsod.o
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-gcc.exe kernel\alloc.c -O3 -Wall -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-unused-variable -g -ffreestanding -fno-exceptions -fno-rtti -std=c11 -c -o out\alloc.o
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-ld.exe -e KernelMain -z norelro -T linker.ld --static -o .\out\kernel.elf .\out\main.o .\out\draw.o .\out\font.o .\font\hankaku.o .\out\desktop.o .\out\memorym.o .\out\wsod.o .\out\alloc.o .\out\sheet.o .\out\draw_sheet.o .\out\mouse.o .\out\gdt.o .\out\common.o .\out\idt.o .\out\apic.o
copy out\BOOTX64.efi esp\EFI\BOOT
copy out\kernel.elf esp
..\tools\qemu\qemu-system-x86_64.exe -bios ..\tools\OVMF.fd -m 4096 -net none -drive file=fat:rw:esp,index=0,format=vvfat -serial file:serial.log -device usb-ehci,id=usb -device usb-tablet,bus=usb.0 -device usb-kbd,bus=usb.0