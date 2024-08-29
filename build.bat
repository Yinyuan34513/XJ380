@echo off
cls
del /Q /S out\*
del /Q /S esp\*
mkdir out
mkdir esp\EFI\BOOT
..\tools\mingw64\bin\x86_64-w64-mingw32-gcc.exe -Wall -Wextra -e efi_main -nostdinc -nostdlib -fno-builtin -Wl,--subsystem,10 -o out\BOOTX64.efi boot\bootx64.c
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-g++.exe kernel\main.cpp -O2 -Wall -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -c -o out\main.o
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-g++.exe kernel\memorym.cpp -O2 -Wall -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -c -o out\memorym.o
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-g++.exe kernel\wsod.cpp -O2 -Wall -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -c -o out\wsod.o
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-g++.exe graphics\draw.cpp -O2 -Wall -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -c -o out\draw.o
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-g++.exe graphics\desktop.cpp -O2 -Wall -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -c -o out\desktop.o
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-g++.exe font\font.cpp -O2 -Wall -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -c -o out\font.o
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-ld.exe -z norelro -T linker.ld --static -o .\out\kernel.elf .\out\main.o .\out\draw.o .\out\font.o .\font\hankaku.o .\out\desktop.o .\out\memorym.o .\out\wsod.o
copy out\BOOTX64.efi esp\EFI\BOOT
copy out\kernel.elf esp
..\tools\qemu\qemu-system-x86_64.exe -bios ..\tools\OVMF.fd -m 1024 -net none -drive file=fat:rw:esp,index=0,format=vvfat