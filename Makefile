#工具路径
TOOLS = ../tools

#工具
MINCC = $(TOOLS)/mingw64/bin/x86_64-w64-mingw32-gcc.exe
ELFPP = $(TOOLS)/x86_64-elf-tools-windows/bin/x86_64-elf-g++.exe
ELFLD = $(TOOLS)/x86_64-elf-tools-windows/bin/x86_64-elf-ld.exe
QEMU = $(TOOLS)/qemu/qemu-system-x86_64.exe
QEMU_IMG = $(TOOLS)/qemu/qemu-img.exe
DD = $(TOOLS)/dd-0.6beta3/dd.exe

#系统指令
CPY = copy
MKDIR = mkdir
RMDIR = rmdir

#编译版本及编译平台
REL = XJ380_BUILD_V2

default : run clear

mkrel : 
	$(MKDIR) $(REL)\EFI\BOOT
	$(CPY) out\BOOTX64.efi $(REL)\EFI\BOOT
	$(CPY) out\kernel.elf $(REL)

clear :
	$(RMDIR) /Q /S esp
	$(RMDIR) /Q /S out
	$(MKDIR) out

#将ESP模拟为fat32驱动器并运行
run :
	$(MKDIR) esp\EFI\BOOT
	$(CPY) out\BOOTX64.efi esp\EFI\BOOT
	$(CPY) out\kernel.elf esp
	$(QEMU) -bios $(TOOLS)/OVMF.fd -m 512 -net none -drive file=fat:rw:esp,index=0,format=vvfat

out/BOOTX64.efi : boot/bootx64.c
	$(MINCC) -Wall -Wextra -e efi_main -nostdinc -nostdlib -fno-builtin -W -isystem -o ./out/BOOTX64.efi ./boot/bootx64.c

out/main.o : kernel/main.c
	$(ELFPP) kernel/main.cpp -O2 -Wall -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -c -o ./out/main.o

out/kernel.elf : out/main.o
	$(ELFLD) -e KernelMain -z norelro -Ttext-segment 0x100000 --static -o ./out/kernel.elf ./out/main.o
