@echo off
cls
del /Q /S out\*
del /Q /S esp\*
mkdir out
mkdir esp\EFI\BOOT

..\tools\mingw64\bin\x86_64-w64-mingw32-gcc.exe -Wall -Wextra -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-unused-variable -e efi_main -nostdinc -nostdlib -fno-builtin -Wl,--subsystem,10 -o out\BOOTX64.efi boot\bootx64.c
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-g++.exe kernel\main.cpp -O2 -Wall -Wno-unused-parameter -Wno-unused-variable -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -c -o out\main.o
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-g++.exe graphics\draw.cpp -O2 -Wall -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-unused-variable -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -c -o out\draw.o
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-g++.exe font\font.cpp -O2 -Wall -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-unused-variable -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -c -o out\font.o
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-g++.exe graphics\desktop.cpp -O2 -Wall -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-unused-variable -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -c -o out\desktop.o
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-g++.exe graphics\heap_malloc.cpp -O2 -Wall -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-unused-variable -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -c -o out\heap_malloc.o
..\tools\x86_64-elf-tools-windows\bin\x86_64-elf-ld.exe -e KernelMain -z norelro -T linker.ld --static -o .\out\kernel.elf .\out\main.o .\out\draw.o .\out\font.o .\font\hankaku.o .\out\desktop.o .\out\heap_malloc.o

copy out\BOOTX64.efi esp\EFI\BOOT
copy out\kernel.elf esp
..\tools\qemu\qemu-system-x86_64.exe -bios ..\tools\OVMF.fd -m 1024 -net none -drive file=fat:rw:esp,index=0,format=vvfat