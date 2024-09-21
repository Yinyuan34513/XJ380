#!/usr/bin/sh
rm -r out esp
mkdir out
mkdir -p esp/EFI/BOOT
x86_64-w64-mingw32-gcc -Wall -Wextra -e efi_main -nostdinc -nostdlib -fno-builtin -Wl,--subsystem,10 -fshort-wchar -o out/BOOTX64.efi boot/bootx64.c
g++ kernel/main.cpp -O2 -Wall -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -fshort-wchar -c -o out/main.o
g++ graphics/draw.cpp -O2 -Wall -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -fshort-wchar -c -o out/draw.o
g++ font/font.cpp -O2 -Wall -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -fshort-wchar -c -o out/font.o
g++ graphics/desktop.cpp -O2 -Wall -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -fshort-wchar -c -o out/desktop.o
g++ kernel/memorym.cpp -O2 -Wall -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -fshort-wchar -c -o out/memorym.o
g++ kernel/wsod.cpp -O2 -Wall -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -fshort-wchar -c -o out/wsod.o
gcc kernel/alloc.c -O2 -Wall -g -ffreestanding -fno-exceptions -fno-rtti -std=c11 -fshort-wchar -c -o out/alloc.o
ld -e KernelMain -z norelro -T linker.ld --static -o ./out/kernel.elf ./out/main.o ./out/draw.o ./out/font.o ./font/hankaku.o ./out/desktop.o ./out/memorym.o ./out/alloc.o
cp out/BOOTX64.efi esp/EFI/BOOT
cp out/kernel.elf esp
qemu-system-x86_64 -bios OVMF.fd -m 1024 -net none -drive file=fat:rw:esp,index=0,format=vvfat -serial file:serial.log -device usb-ehci,id=usb -device usb-tablet,bus=usb.0 -device usb-kbd,bus=usb.0
