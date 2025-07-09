NASM_FLAGS = -f elf64 -DX86_64_TARGET -DUEFI
ASM_FLAGS = -O0 -g -mno-red-zone -nostdlib -ffreestanding -fno-builtin -m64 -fno-stack-protector -fno-exceptions
C_FLAGS = -O0 -g -mno-red-zone -nostdlib -ffreestanding -fno-builtin -m64 -fno-stack-protector -fno-exceptions -std=gnu11 -fshort-wchar -nostdinc -I ./include
CPP_FLAGS = -O0 -g -mno-red-zone -nostdlib -ffreestanding -fno-builtin -m64 -fno-stack-protector -fno-exceptions -fno-rtti -std=gnu++17 -fshort-wchar -nostdinc -fno-use-cxa-atexit -fno-threadsafe-statics -I ./include

DEBUG ?= 1
SMP ?= 4
SUDO ?= 0
KVM ?= 0

ifeq ($(DEBUG), 1)
DEBUG_FLAG = -s -S
endif

ifeq ($(SUDO), 1)
SUDO_CMD = sudo
endif

ifeq ($(KVM), 1)
KVM_FLAG = --enable-kvm
endif

BUILD_DIR := out

QEMU_CMD = $(SUDO_CMD) qemu-system-x86_64 -d cpu_reset -M q35 -bios OVMF.fd -m 2048 -smp $(SMP) $(KVM_FLAG) -net none -drive if=none,id=harddisk,file=XJ380.img,index=0,format=raw -device ahci,id=ahci -device ide-hd,bus=ahci.0,drive=harddisk -device nec-usb-xhci,id=xhci -serial file:serial.log $(DEBUG_FLAG)

COMPILER_PREFIX ?= 

# FILES
NASM_FILES := $(shell find ./kernel ./driver -name "*.asm")
ASM_FILES := $(shell find ./kernel ./driver -name "*.S")
C_FILES := $(shell find ./kernel ./driver -name "*.c")
CPP_FILES := $(shell find ./kernel ./driver -name "*.cpp")

export CC = $(COMPILER_PREFIX)gcc
export CPP = $(COMPILER_PREFIX)g++
export NASM = $(COMPILER_PREFIX)nasm
export LD = $(COMPILER_PREFIX)ld
export NM = $(COMPILER_PREFIX)nm
export OBJC = $(COMPILER_PREFIX)objcopy
export OBJD = $(COMPILER_PREFIX)objdump

NASM_OBJS := $(patsubst ./%.asm, ./$(BUILD_DIR)/%.o, $(NASM_FILES))
ASM_OBJS := $(patsubst ./%.S, ./$(BUILD_DIR)/%.o, $(ASM_FILES))
C_OBJS := $(patsubst ./%.c, ./$(BUILD_DIR)/%.o, $(C_FILES))
CPP_OBJS := $(patsubst ./%.cpp, ./$(BUILD_DIR)/%.o, $(CPP_FILES))

$(BUILD_DIR)/%.o: %.asm
	@mkdir -p $(@D)
	@$(NASM) $(NASM_FLAGS) $< -o $@
	@echo "NA $< -> $@"

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(@D)
	@$(CC) -mcmodel=large $(ASM_FLAGS) -c $< -o $@
	@echo "AS $< -> $@"

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	@$(CC) -fno-pic -fno-pie -mcmodel=kernel $(C_FLAGS) -c $< -o $@
	@echo "CC $< -> $@"

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	@$(CPP) -fno-pic -fno-pie -mcmodel=kernel  $(CPP_FLAGS) -c $< -o $@
	@echo "CPP $< -> $@"

all: $(ASM_OBJS) $(C_OBJS) $(CPP_OBJS) $(NASM_OBJS)
# 针对BOOT和KERNEL进行特殊处理
	@x86_64-w64-mingw32-gcc -I ./include -Wextra -e efi_main -nostdinc -nostdlib -fno-builtin -Wl,--subsystem,10 -fshort-wchar -o $(BUILD_DIR)/BOOTX64.efi boot/bootx64.c

	@$(LD) -z muldefs -T linker.ld --static -o ./$(BUILD_DIR)/kernel.elf $(shell find ./out -name "*.o")

.PHONY: clean
clean:
	@rm -rf $(BUILD_DIR) ./XJ380.img

vdisk: all
	@rm -rf XJ380.img

	@dd if=/dev/zero of=XJ380.img bs=128M count=1
	@sgdisk XJ380.img -n 1:2048 -t 1:ef00

	@mkfs.vfat -F 32 --offset 2048 -S 512 XJ380.img $$((258048*512/1024))

	@mkdir -p $(BUILD_DIR)/XJ380/EFI/BOOT
	@mkdir -p $(BUILD_DIR)/XJ380/system
	@mkdir -p $(BUILD_DIR)/XJ380/apps
	@cp -a $(BUILD_DIR)/BOOTX64.efi $(BUILD_DIR)/XJ380/EFI/BOOT
	@cp -a $(BUILD_DIR)/kernel.elf $(BUILD_DIR)/XJ380/system

	@mcopy -s -i XJ380.img@@$$((2048*512)) $(BUILD_DIR)/XJ380/* ::

	@rm -rf $(BUILD_DIR)/XJ380

vmdk: vdisk
	@rm -rf XJ380.vmdk
	@qemu-img convert -O vmdk XJ380.img XJ380.vmdk

run: vdisk
	@$(QEMU_CMD)
