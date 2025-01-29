#!/usr/bin/sh
rm -r ./out ./esp
mkdir -p ./out
mkdir -p ./esp/EFI/BOOT

ASM_FLAGS="-O0 -Wall -g -nostdlib -m64 -ffreestanding -fno-exceptions -fno-rtti"
C_FLAGS="-O0 -Wall -g -ffreestanding -fno-exceptions -fno-rtti -std=c11 -fshort-wchar"
CPP_FLAGS="-O0 -Wall -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -fshort-wchar"

ASM_FILES=$(find ./kernel ./driver ./graphics ./font ./lib -name "*.S")
C_FILES=$(find ./kernel ./driver ./graphics ./font ./lib -name "*.c")
CPP_FILES=$(find ./kernel ./driver ./graphics ./font ./lib -name "*.cpp")

x86_64-w64-mingw32-gcc -Wall -Wextra -e efi_main -nostdinc -nostdlib -fno-builtin -Wl,--subsystem,10 -fshort-wchar -o out/BOOTX64.efi boot/bootx64.c

counter=1
for file in $ASM_FILES; do
  gcc $file $ASM_FLAGS -c -o ./out/$counter.o
  counter=$((counter + 1))
done
for file in $C_FILES; do
  gcc $file $C_FLAGS -c -o ./out/$counter.o
  counter=$((counter + 1))
done
for file in $CPP_FILES; do
  g++ $file $CPP_FLAGS -c -o ./out/$counter.o
  counter=$((counter + 1))
done

ld -e KernelMain -z norelro -T linker.ld --static -o ./out/kernel.elf $(find ./out -name "*.o") ./font/hankaku.o

cp ./out/BOOTX64.efi ./esp/EFI/BOOT
cp ./out/kernel.elf ./esp

qemu-system-x86_64 -bios OVMF.fd -m 1024 -net none -drive file=fat:rw:esp,index=0,format=vvfat -serial file:serial.log -device usb-ehci,id=usb -device usb-tablet,bus=usb.0 -device usb-kbd,bus=usb.0
