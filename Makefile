PYTHON ?= python3
KCONFIG_CONFIG ?= .config
KCONFIG_DEFCONFIG ?= configs/xj380_defconfig
KCONFIG_SCRIPT := tools/kconfig.py
KCONFIG_AUTOCONF := include/generated/autoconf.h
KCONFIG_AUTO_CONF := include/generated/auto.conf
NO_KCONFIG_GOALS := clean clean.clangd gen.clangd defconfig olddefconfig menuconfig savedefconfig

ifeq ($(filter $(NO_KCONFIG_GOALS),$(MAKECMDGOALS)),)
-include $(KCONFIG_AUTO_CONF)
endif

.DEFAULT_GOAL := all

KCONFIG_CPPFLAGS := -include generated/autoconf.h

NASM_FLAGS = -f elf64 -DX86_64_TARGET -DUEFI
ASM_FLAGS = -O0 -g -mno-red-zone -mstackrealign -nostdlib -ffreestanding -fno-builtin -m64 -fno-stack-protector -fno-exceptions -mno-80387
C_FLAGS = -O0 -g -mno-red-zone -mstackrealign -nostdlib -ffreestanding -fno-builtin -m64 -fno-stack-protector -fno-exceptions -std=c11 -fshort-wchar -nostdinc -mno-80387 -I./include $(KCONFIG_CPPFLAGS)
CPP_FLAGS = -O0 -g -mno-red-zone -mstackrealign -nostdlib -ffreestanding -fno-builtin -m64 -fno-stack-protector -fno-exceptions -fno-rtti -std=gnu++17 -fshort-wchar -nostdinc -fno-use-cxa-atexit -fno-threadsafe-statics -mno-80387 -Wno-int-to-pointer-cast -Wno-macro-redefined -Wno-c11-extensions -Wno-c99-extensions -Wno-gnu-statement-expression-from-macro-expansion -I./include $(KCONFIG_CPPFLAGS) -Wno-writable-strings -Wno-c++11-narrowing
BOOT_C_FLAGS = -g -I ./boot/include -I ./include $(KCONFIG_CPPFLAGS) -Wextra -e efi_main -nostdinc -nostdlib -fno-builtin -Wl,--subsystem,10 -fshort-wchar

DEBUG ?= $(if $(CONFIG_QEMU_DEBUG),1,0)
SMP ?= $(or $(CONFIG_QEMU_SMP_CPUS),4)
SUDO ?= $(if $(CONFIG_QEMU_USE_SUDO),1,0)
KVM ?= $(if $(CONFIG_QEMU_ENABLE_KVM),1,0)

ifeq ($(DEBUG), 1)
DEBUG_FLAG = -s -S
endif

ifeq ($(SUDO), 1)
SUDO_CMD = sudo -E env DISPLAY=$(DISPLAY) XAUTHORITY=$(XAUTHORITY)
endif

ifeq ($(KVM), 1)
KVM_FLAG = --enable-kvm -cpu $(or $(CONFIG_QEMU_CPU_KVM),host)
else
KVM_FLAG = -cpu $(or $(CONFIG_QEMU_CPU_TCG),max)
endif

BUILD_DIR := out
KMOD_DIR := kmod
IMAGE_SIZE_MB ?= $(or $(CONFIG_IMAGE_SIZE_MB),128)
IMAGE_EFI_PARTITION_START_SECTOR ?= $(or $(CONFIG_IMAGE_EFI_PARTITION_START_SECTOR),2048)
IMAGE_FAT_SECTORS ?= $(or $(CONFIG_IMAGE_FAT_SECTORS),0)
ifeq ($(IMAGE_FAT_SECTORS),0)
MKFS_SIZE_ARG = $$((($(IMAGE_SIZE_MB) * 1024 * 1024 / 512) - $(IMAGE_EFI_PARTITION_START_SECTOR)))
else
MKFS_SIZE_ARG = $(IMAGE_FAT_SECTORS)
endif
VMDK_OUTPUT_FORMAT ?= $(or $(CONFIG_VMDK_OUTPUT_FORMAT),vmdk)

ifeq ($(CONFIG_IMAGE_INCLUDE_STARDUST_EXAMPLES),y)
STARDUST_EXAMPLES := \
	frameworks/StardustUI/examples/helloworld \
	frameworks/StardustUI/examples/layout
endif
STARDUSTUI_EXAMPLE_DIRS := $(STARDUST_EXAMPLES)

# QEMU "--display" expects a display backend type (gtk/sdl/curses/none/...),
# not an X11 DISPLAY string like ":0".
# Use "DISPLAY_BACKEND" to select the backend; keep exporting X11 DISPLAY
# via SUDO_CMD when needed.
DISPLAY_BACKEND ?= $(or $(CONFIG_QEMU_DISPLAY_BACKEND),gtk)
QEMU_MEMORY_MB ?= $(or $(CONFIG_QEMU_MEMORY_MB),8192)
QEMU_MACHINE ?= $(or $(CONFIG_QEMU_MACHINE),q35)
QEMU_BIOS ?= $(or $(CONFIG_QEMU_BIOS),OVMF.fd)
USB_XHCI_DEVICES ?= -device usb-kbd,bus=xhci.0,port=1 -device usb-mouse,bus=xhci.0,port=2
ifeq ($(CONFIG_QEMU_ENABLE_USB_INPUT),y)
QEMU_USB_INPUT_FLAGS := $(USB_XHCI_DEVICES)
endif
AUDIO_BACKEND ?= $(or $(CONFIG_QEMU_AUDIO_BACKEND),sdl)
HDA_CODEC ?= $(or $(CONFIG_QEMU_HDA_CODEC),hda-output)
SERIAL_LOG ?= $(or $(CONFIG_QEMU_SERIAL_LOG),serial.log)
ifeq ($(CONFIG_QEMU_HOSTFWD_TELNET),y)
QEMU_HOSTFWD_FLAGS := ,hostfwd=tcp::2323-:2323
endif
ifeq ($(CONFIG_QEMU_ENABLE_E1000),y)
QEMU_NET_FLAGS := -netdev user,id=net0$(QEMU_HOSTFWD_FLAGS) -device e1000,netdev=net0
endif
QEMU_CMD = $(SUDO_CMD) qemu-system-x86_64 --display $(DISPLAY_BACKEND) -M $(QEMU_MACHINE) -bios $(QEMU_BIOS) -m $(QEMU_MEMORY_MB) -smp $(SMP) $(KVM_FLAG) -drive if=none,id=harddisk,file=XJ380.img,index=0,format=raw -device ahci,id=ahci -device ide-hd,bus=ahci.0,drive=harddisk -device qemu-xhci,id=xhci $(QEMU_USB_INPUT_FLAGS) -serial file:$(SERIAL_LOG) -audiodev $(AUDIO_BACKEND),id=snd0 -device ich9-intel-hda -device $(HDA_CODEC),audiodev=snd0 $(QEMU_NET_FLAGS) $(DEBUG_FLAG) -monitor stdio

COMPILER_PREFIX ?= 

# FILES
NASM_FILES := $(shell find ./kernel ./driver ./graphics ./font ./lib -name "*.asm")
ASM_FILES := $(shell find ./kernel ./driver ./graphics ./font ./lib -name "*.S")
C_FILES := $(shell find ./kernel ./driver ./graphics ./font ./lib -name "*.c")
CPP_FILES := $(shell find ./kernel ./driver ./graphics ./font ./lib -name "*.cpp")
HEADER_FILES := $(shell find ./include -name "*.h" -o -name "*.hpp")

export CC = $(COMPILER_PREFIX)clang
export CPP = $(COMPILER_PREFIX)clang++
export NASM = $(COMPILER_PREFIX)nasm
export LD = $(COMPILER_PREFIX)ld
export NM = $(COMPILER_PREFIX)nm
export OBJC = $(COMPILER_PREFIX)objcopy
export OBJD = $(COMPILER_PREFIX)objdump

NASM_OBJS := $(patsubst ./%.asm, ./$(BUILD_DIR)/%.o, $(NASM_FILES))
ASM_OBJS := $(patsubst ./%.S, ./$(BUILD_DIR)/%.o, $(ASM_FILES))
C_OBJS := $(patsubst ./%.c, ./$(BUILD_DIR)/%.o, $(C_FILES))
CPP_OBJS := $(patsubst ./%.cpp, ./$(BUILD_DIR)/%.o, $(CPP_FILES))
KERNEL_OBJS := $(ASM_OBJS) $(C_OBJS) $(CPP_OBJS) $(NASM_OBJS)
BUILD_SETTINGS_HEADER := kernel/build_settings.h
FMT_FILES := $(C_FILES:%=%.fmt) $(CPP_FILES:%=%.fmt) $(HEADER_FILES:%=%.fmt)
CHECK_FILES := $(C_FILES:%.c=%.tidy) $(CPP_FILES:%.cpp=%.tidy) $(HEADER_FILES:%.h=%.tidy)
CLANGD_TEMPLATE_CONFS := $(wildcard ./**/.clangd_template ./.clangd_template)
PWD := $(CURDIR)

$(KCONFIG_CONFIG): Kconfig $(KCONFIG_SCRIPT) $(KCONFIG_DEFCONFIG)
	@$(PYTHON) $(KCONFIG_SCRIPT) olddefconfig

$(KCONFIG_AUTOCONF) $(KCONFIG_AUTO_CONF): Kconfig $(KCONFIG_SCRIPT) $(KCONFIG_DEFCONFIG) $(KCONFIG_CONFIG)
	@$(PYTHON) $(KCONFIG_SCRIPT) syncconfig

$(KERNEL_OBJS): $(BUILD_SETTINGS_HEADER) $(KCONFIG_AUTOCONF)

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
	@$(CC) -fno-pic -fno-pie -mcmodel=large $(C_FLAGS) -c $< -o $@
	@echo "CC $< -> $@"

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	@$(CPP) -fno-pic -fno-pie -mcmodel=large  $(CPP_FLAGS) -c $< -o $@
	@echo "CPP $< -> $@"

%.fmt: %
	@echo "Formatting $<"
	@clang-format -i $<

# 暂时不 Check ASM 文件
%.tidy: %.c
	@echo "Checking C $<"
	@clang-tidy $< -- $(C_FLAGS)
%.tidy: %.h
	@echo "Checking Header $<"
	@clang-tidy $< -- $(C_FLAGS)

%.tidy: %.cpp
	@echo "Checking CPP $<"
	@clang-tidy $< -- $(CPP_FLAGS)

.clangd:
	@echo "# Generated by Makefile" > $<.clangd
	@sed "s/\$${workspaceFolder}/$(subst /,\/,${PWD})/g" $<.clangd_template >> $<.clangd
	@printf "\033[1;32m[Done]\033[0m $<.clangd was generated.\n"
%.clangd: %
	@echo "# Generated by Makefile" > $<.clangd
	@sed "s/\$${workspaceFolder}/$(subst /,\/,${PWD})/g" $<.clangd_template >> $<.clangd
	@printf "\033[1;32m[Done]\033[0m $<.clangd was generated.\n"

all: $(KCONFIG_AUTO_CONF) $(KERNEL_OBJS) liballoc-x86_64.a
# 针对BOOT和KERNEL进行特殊处理
	@x86_64-w64-mingw32-gcc $(BOOT_C_FLAGS) -o $(BUILD_DIR)/BOOTX64.efi boot/bootx64.c boot/bootlib.c

	@$(OBJC) -I binary -O elf64-x86-64 ./font/hankaku.bin ./font/hankaku.o
	@$(OBJC) -I binary -O elf64-x86-64 ./graphics/logo/xj380_2.png ./graphics/logo/xj380_2.o

	@$(LD) -z muldefs -T linker.ld --static -o ./$(BUILD_DIR)/kernel.elf $(KERNEL_OBJS) ./font/hankaku.o ./graphics/logo/xj380_2.o liballoc-x86_64.a

	@$(MAKE) -C user KCONFIG_AUTOCONF=$(PWD)/$(KCONFIG_AUTOCONF) build_xapi
	@$(MAKE) -C user KCONFIG_AUTOCONF=$(PWD)/$(KCONFIG_AUTOCONF)
	@$(MAKE) -C $(KMOD_DIR)
	@if [ -n "$(strip $(STARDUSTUI_EXAMPLE_DIRS))" ]; then \
		for dir in $(STARDUSTUI_EXAMPLE_DIRS); do \
			$(MAKE) -C $$dir copy PLATFORM=xj380 KCONFIG_AUTOCONF=$(PWD)/$(KCONFIG_AUTOCONF) || exit $$?; \
		done; \
	fi

liballoc-x86_64.a:
	curl -Lo liballoc-x86_64.a https://github.com/plos-clan/liballoc/releases/download/release/liballoc-x86_64.a

.PHONY: clean format check gen.clangd defconfig olddefconfig menuconfig savedefconfig stardustui.examples.copy

defconfig:
	@$(PYTHON) $(KCONFIG_SCRIPT) defconfig --defconfig $(KCONFIG_DEFCONFIG)
	@echo "Loaded $(KCONFIG_DEFCONFIG)"

olddefconfig:
	@$(PYTHON) $(KCONFIG_SCRIPT) olddefconfig
	@echo "Updated $(KCONFIG_CONFIG)"

menuconfig:
	@$(PYTHON) $(KCONFIG_SCRIPT) menuconfig

savedefconfig:
	@$(PYTHON) $(KCONFIG_SCRIPT) savedefconfig --output $(KCONFIG_DEFCONFIG)
	@echo "Saved $(KCONFIG_DEFCONFIG)"

clean:
	@rm -rf $(BUILD_DIR) ./XJ380.img include/generated
	@$(MAKE) -C user clean

stardustui.examples.copy:
	@if [ -n "$(strip $(STARDUSTUI_EXAMPLE_DIRS))" ]; then \
		for dir in $(STARDUSTUI_EXAMPLE_DIRS); do \
			$(MAKE) -C $$dir copy PLATFORM=xj380 KCONFIG_AUTOCONF=$(PWD)/$(KCONFIG_AUTOCONF) || exit $$?; \
		done; \
	fi

format: $(FMT_FILES)
	@echo "Formatted all the code ..."

check: $(CHECK_FILES)
	@echo "Checked all the code ..."

clean.clangd:
	@rm -f $(CLANGD_TEMPLATE_CONFS:%.clangd_template=%.clangd)

gen.clangd: clean.clangd $(CLANGD_TEMPLATE_CONFS:%.clangd_template=%.clangd)
	@echo "\033[1;32m[Done]\033[0m .clangd configurations was generated.\n"

vdisk: all
	@rm -rf XJ380.img

	@dd if=/dev/zero of=XJ380.img bs=1M count=$(IMAGE_SIZE_MB)
# You must execute the following command with sudo privileges, that's not good...
	@sudo sgdisk XJ380.img -n 1:$(IMAGE_EFI_PARTITION_START_SECTOR) -t 1:ef00

	@sudo mkfs.vfat -F 32 --offset $(IMAGE_EFI_PARTITION_START_SECTOR) -S 512 XJ380.img $(MKFS_SIZE_ARG)

	@mkdir -p $(BUILD_DIR)/XJ380/EFI/BOOT
	@mkdir -p $(BUILD_DIR)/XJ380/system
	@mkdir -p $(BUILD_DIR)/XJ380/system/font
	@mkdir -p $(BUILD_DIR)/XJ380/system/theme/skyglass
	@mkdir -p $(BUILD_DIR)/XJ380/system/foxland
	@mkdir -p $(BUILD_DIR)/XJ380/system/icon
	@mkdir -p $(BUILD_DIR)/XJ380/apps
	@mkdir -p $(BUILD_DIR)/XJ380/system/resources/image

	@cp -a $(BUILD_DIR)/BOOTX64.efi $(BUILD_DIR)/XJ380/EFI/BOOT
	@cp -a $(BUILD_DIR)/kernel.elf $(BUILD_DIR)/XJ380/system

	@cp -a $(BUILD_DIR)/shell.elf $(BUILD_DIR)/XJ380/system

	@cp -a $(BUILD_DIR)/fmanager.elf $(BUILD_DIR)/XJ380/system
	@cp -a ./user/fmanager/fm_bg.png $(BUILD_DIR)/XJ380/system/resources/image

	@cp -a $(BUILD_DIR)/ctrlmenu.elf $(BUILD_DIR)/XJ380/system

	@cp -a $(BUILD_DIR)/calc.elf $(BUILD_DIR)/XJ380/system

	@cp -a $(BUILD_DIR)/texter.elf $(BUILD_DIR)/XJ380/system

	@cp -a $(BUILD_DIR)/taskmgr.elf $(BUILD_DIR)/XJ380/system

	@cp -a $(BUILD_DIR)/picturer.elf $(BUILD_DIR)/XJ380/system

	@cp -a $(BUILD_DIR)/init.elf $(BUILD_DIR)/XJ380/apps
	@cp -a $(BUILD_DIR)/cp537.elf $(BUILD_DIR)/XJ380/apps
	@cp -a $(BUILD_DIR)/bcms-sp.elf $(BUILD_DIR)/XJ380/apps
	@cp -a $(BUILD_DIR)/browser.elf $(BUILD_DIR)/XJ380/apps

	@cp -a ./font/ttf/XJ380F.ttf $(BUILD_DIR)/XJ380/system/font
	@cp -a ./font/ttf/XJ380C.ttf $(BUILD_DIR)/XJ380/system/font

	@cp -a ./graphics/window/theme/skyglass/*.* $(BUILD_DIR)/XJ380/system/theme/skyglass

	@cp -a ./graphics/peek.png $(BUILD_DIR)/XJ380/system/foxland

	@cp -a ./graphics/logo/*.* $(BUILD_DIR)/XJ380/system
# 	@cp -a ./test.wav $(BUILD_DIR)/XJ380/system

	@cp -a ./graphics/icon/*.* $(BUILD_DIR)/XJ380/system/icon

	@cp -a ./out/*.km $(BUILD_DIR)/XJ380/system

	@cp -a ./resources/* $(BUILD_DIR)/XJ380/system/resources

ifeq ($(CONFIG_IMAGE_COPY_STARDUST_EXAMPLES),y)
	@cp ./out/helloworld.elf $(BUILD_DIR)/XJ380/apps
	@cp ./out/layout.elf $(BUILD_DIR)/XJ380/apps
endif
	
# temp test (remember to delete)
ifeq ($(CONFIG_IMAGE_INCLUDE_TEST_DESKTOP_FILE),y)
	@mkdir -p $(BUILD_DIR)/XJ380/users/GuoqiFish/desktop
	@cp -a ./test.txt $(BUILD_DIR)/XJ380/users/GuoqiFish/desktop
endif

	@mcopy -s -i XJ380.img@@$$(($(IMAGE_EFI_PARTITION_START_SECTOR)*512)) $(BUILD_DIR)/XJ380/* ::

	@rm -rf $(BUILD_DIR)/XJ380
	

vmdk: vdisk
	@rm -rf XJ380.vmdk
	@qemu-img convert -O $(VMDK_OUTPUT_FORMAT) XJ380.img XJ380.$(VMDK_OUTPUT_FORMAT)

run: vdisk
	@$(QEMU_CMD)

justrun: $(KCONFIG_AUTO_CONF)
	@$(QEMU_CMD)
