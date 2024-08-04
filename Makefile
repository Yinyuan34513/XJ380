MINCC = x86_64-w64-mingw32-gcc
ELFCC = ./tools/x86_64-elf-g++
ELFLD = ./tools/x86_64-elf-ld
QEMU = qemu-system-x86_64

default : mkvd run

#制作镜像
mkvd:
	dd if=/dev/zero of=XJ380.img bs=1M count=2048
	sudo losetup /dev/loop17 XJ380.img
	sudo mkfs.vfat -F 32 /dev/loop17
	sudo mount /dev/loop17 /mnt
	sudo mkdir -p /mnt/EFI/BOOT
	sudo cp out/BOOTX64.efi /mnt/EFI/BOOT/BOOTX64.EFI
	sudo cp out/kernel.elf /mnt/kernel.elf
	sudo umount /mnt

#将ESP模拟为fat32驱动器并运行
run:
	sudo mkdir -p esp/EFI/BOOT
	sudo cp BOOTX64.efi esp/EFI/BOOT/BOOTX64.EFI
	sudo cp out/kernel.elf esp/kernel.elf
	$(QEMU) -bios /usr/share/OVMF/OVMF_CODE.fd -net none -drive file=fat:rw:esp,index=0,format=vvfat

out/BOOTX64.efi : boot/bootx64.c
	$(MINCC) -Wall -Wextra -e efi_main -nostdinc -nostdlib -fno-builtin -Wl,--subsystem,10 -o BOOTX64.efi bootx64.c

out/main.o : kernel/main.c
	$(ELFCC) main.cpp -O2 -Wall -g -ffreestanding -fno-exceptions -fno-rtti -std=c++17 -c -o main.o

out/kernel.elf : out/main.o
	$(ELFLD) -e KernelMain -z norelro -Ttext-segment 0x100000 --static -o kernel.elf main.o
