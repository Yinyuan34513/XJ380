Add-Type -AssemblyName System.IO  
[System.IO.File]::WriteAllText("out/BOOTX64.efi", "")
../tools/mingw64/bin/x86_64-w64-mingw32-gcc.exe -Wall -Wextra -e efi_main -nostdinc -nostdlib -fno-builtin -W -isystem 10 -o out/BOOTX64.efi ./boot/bootx64.c
../tools/x86_64-elf-tools-windows/bin/x86_64-elf-g++.exe kernel/main.cpp -O2 -Wall -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -c -o out/main.o
../tools/x86_64-elf-tools-windows/bin/x86_64-elf-ld.exe -e KernelMain -z norelro -Ttext-segment 0x100000 --static -o out/kernel.elf out/main.o
#mkdir esp\EFI\BOOT
Copy-Item out\BOOTX64.efi esp\EFI\BOOT
Copy-Item out\kernel.elf esp
../tools/qemu/qemu-system-x86_64.exe -bios ../tools/OVMF.fd -m 512 -net none -drive file=fat:rw:esp,index=0,format=vvfat