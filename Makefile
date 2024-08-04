default : mkvd

mkvd:
	dd if=/dev/zero of=XJ380.img bs=1M count=2048
	sudo losetup /dev/loop17 XJ380.img
	sudo mkfs.vfat -F 32 /dev/loop17
	sudo mount /dev/loop17 /mnt
	sudo mkdir -p /mnt/EFI/BOOT
	sudo cp BOOTX64.efi /mnt/EFI/BOOT/BOOTX64.EFI
	sudo umount /mnt

out/BOOTX64.efi : boot/bootx64.c
	x86_64-w64-mingw32-gcc -Wall -Wextra -e efi_main -nostdinc -nostdlib -fno-builtin -Wl,--subsystem,10 -o BOOTX64.efi bootx64.c
