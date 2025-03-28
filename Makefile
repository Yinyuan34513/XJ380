ASM_FLAGS = -O0 -g -mno-sse2 -mno-sse -nostdlib -ffreestanding -fno-builtin -m64 -fno-stack-protector -fno-exceptions
C_FLAGS = -O0 -g -mno-sse2 -mno-sse -nostdlib -ffreestanding -fno-builtin -m64 -fno-stack-protector -fno-exceptions -std=c99 -fshort-wchar -nostdinc
CPP_FLAGS = -O0 -g -mno-sse2 -mno-sse -nostdlib -ffreestanding -fno-builtin -m64 -fno-stack-protector -fno-exceptions -fno-rtti -std=c++17 -fshort-wchar -nostdinc -fno-use-cxa-atexit -fno-threadsafe-statics

DEBUG ?= 0
SMP ?= 4
SUDO ?= 0
KVM ?= 0

ifeq ($(DEBUG), 1)
DEBUG_FLAG = -s -S
endif

ifeq ($(KVM), 1)
SUDO = 1
KVM_ENABLE = --enable-kvm
endif

ifeq ($(SUDO), 1)
SUDO_CMD = sudo
endif

QEMU_CMD = $(SUDO_CMD) qemu-system-x86_64 -M q35 -bios /usr/share/ovmf/OVMF.fd -m 8192 -smp $(SMP) $(KVM_ENABLE) -net none -drive if=none,id=harddisk,file=XJ380.img,index=0,format=raw -device ahci,id=ahci -device ide-hd,bus=ahci.0,drive=harddisk -serial file:serial.log $(DEBUG_FLAG)

COMPILER_PREFIX ?= x86_64-linux-gnu-

# FILES
ASM_FILES := $(shell find ./kernel ./graphics ./driver ./lib ./font -name "*.S")
C_FILES := $(shell find ./kernel ./graphics ./driver ./lib ./font -name "*.c")
CPP_FILES := $(shell find ./kernel ./graphics ./driver ./lib ./font -name "*.cpp")

export CC = $(COMPILER_PREFIX)gcc
export CPP = $(CC)
export LD = $(COMPILER_PREFIX)ld
export NM = $(COMPILER_PREFIX)nm
export OBJC = $(COMPILER_PREFIX)objcopy
export OBJD = $(COMPILER_PREFIX)objdump

ASM_OBJS := $(sort $(patsubst ./%.S, ./out/%.o, $(ASM_FILES)))
C_OBJS := $(sort $(patsubst ./%.c, ./out/%.o, $(C_FILES)))
CPP_OBJS := $(sort $(patsubst ./%.cpp, ./out/%.o, $(CPP_FILES)))

out/%.o: %.S
	@mkdir -p $(@D)
	@$(CC) -fno-pic -mcmodel=kernel $(ASM_FLAGS) -c $< -o $@
	@echo "AS $< -> $@"

out/%.o: %.c
	@mkdir -p $(@D)
	@$(CC) -fno-pic -mcmodel=kernel $(C_FLAGS) -c $< -o $@
	@echo "CC $< -> $@"

out/%.o: %.cpp
	@mkdir -p $(@D)
	@$(CPP) -fno-pic -mcmodel=kernel  $(CPP_FLAGS) -c $< -o $@
	@echo "CPP $< -> $@"

all: $(ASM_OBJS) $(C_OBJS) $(CPP_OBJS)
# 针对BOOT和KERNEL进行特殊处理
	@x86_64-w64-mingw32-gcc -Wextra -e efi_main -nostdinc -nostdlib -fno-builtin -Wl,--subsystem,10 -fshort-wchar -o out/BOOTX64.efi boot/bootx64.c

	@$(OBJC) -I binary -O elf64-x86-64 ./font/hankaku.bin ./font/hankaku.o

	@$(LD) -T linker.ld --static -o ./out/kernel.elf $(shell find ./out -name "*.o") ./font/hankaku.o
	@gcc kallsyms/kallsyms.c -o ./out/kallsyms
	@$(NM) -n ./out/kernel.elf | ./out/kallsyms >./out/kallsyms.S
	@$(CC) $(C_FLAGS) -c ./out/kallsyms.S -o ./out/kallsyms.o
	@$(LD) -z muldefs -T linker.ld --static -o ./out/kernel.elf $(shell find ./out -name "*.o") ./font/hankaku.o ./out/kallsyms.o
	@$(OBJD) -x -d -s ./out/kernel.elf > ./out/kernel.txt

	@$(MAKE) -C user build_xlibc
	@$(MAKE) -C user

.PHONY: clean
clean:
	@rm -rf ./out ./XJ380.img ./font/hankaku.o
	@$(MAKE) -C user clean

LOOP := $(shell sudo losetup --find)

vdisk: all
	@rm -rf XJ380.img

	@mkdir -p mnt

	@dd if=/dev/zero bs=1M count=0 seek=1024 of=XJ380.img
	@sgdisk XJ380.img -n 1:2048 -t 1:ef00

	@sudo losetup $(LOOP) --partscan XJ380.img

	@sudo mkfs.vfat -F 32 $(LOOP)p1

	@sudo mount $(LOOP)p1 ./mnt

	@sudo mkdir -p ./mnt/EFI/BOOT
	@sudo mkdir -p ./mnt/usr

	@sudo cp out/kernel.elf ./mnt
	@sudo cp out/BOOTX64.efi ./mnt/EFI/BOOT

	@sudo cp out/init.elf ./mnt/usr
	@sudo cp out/shell.elf ./mnt/usr

	@sudo umount ./mnt

	@sudo losetup -d $(LOOP)

	@qemu-img convert -O vmdk XJ380.img XJ380.vmdk

run: vdisk
	@$(QEMU_CMD)
