NASM_FLAGS = -f elf64 -DX86_64_TARGET -DUEFI
ASM_FLAGS = -O0 -g -mno-red-zone -mstackrealign -nostdlib -ffreestanding -fno-builtin -m64 -fno-stack-protector -fno-exceptions -mno-80387
C_FLAGS = -O0 -g -mno-red-zone -mstackrealign -nostdlib -ffreestanding -fno-builtin -m64 -fno-stack-protector -fno-exceptions -std=c11 -fshort-wchar -nostdinc -mno-80387 -I./include
CPP_FLAGS = -O0 -g -mno-red-zone -mstackrealign -nostdlib -ffreestanding -fno-builtin -m64 -fno-stack-protector -fno-exceptions -fno-rtti -std=gnu++17 -fshort-wchar -nostdinc -fno-use-cxa-atexit -fno-threadsafe-statics -mno-80387 -Wno-int-to-pointer-cast -Wno-macro-redefined -Wno-c11-extensions -Wno-c99-extensions -Wno-gnu-statement-expression-from-macro-expansion -I./include -Wno-writable-strings -Wno-c++11-narrowing
BOOT_C_FLAGS = -g -I ./boot/include -Wextra -e efi_main -nostdinc -nostdlib -fno-builtin -Wl,--subsystem,10 -fshort-wchar

DEBUG ?= 0
SMP ?= 4
SUDO ?= 1
VDISK_SUDO ?=
KVM ?= 1

ifeq ($(DEBUG), 1)
DEBUG_FLAG = -s -S
endif

ifeq ($(SUDO), 1)
SUDO_CMD = sudo -E env DISPLAY=$(DISPLAY) XAUTHORITY=$(XAUTHORITY)
endif

ifeq ($(KVM), 1)
KVM_FLAG = --enable-kvm -cpu host
else
KVM_FLAG = -cpu max
endif

BUILD_DIR := out
BOOT_EFI := $(BUILD_DIR)/BOOTX64.efi
KERNEL_ELF := $(BUILD_DIR)/kernel.elf
FONT_BIN_OBJ := $(BUILD_DIR)/font/hankaku.o
LOGO_BIN_OBJ := $(BUILD_DIR)/graphics/logo/xj380_2.o
VDISK_IMAGE := XJ380.img
VMDK_IMAGE := XJ380.vmdk
VDISK_SIZE_MIB ?= 5120
VDISK_FAT_MIB ?= 5120
VDISK_OFFSET_SECTORS ?= 2048
VDISK_ROOT ?= /tmp/xj380-vdisk-$$(id -u)/XJ380
KMOD_DIR := kmod
STAGE_IMAGE := tools/stage_image.sh
STARDUSTUI_EXAMPLE_DIRS := \
	frameworks/StardustUI/examples/helloworld \
	frameworks/StardustUI/examples/layout \
	$(wildcard frameworks/StardustUI/examples/duckchat)
SELFHOST_USER_THIRD_PARTY := \
	third_party/mbedtls-src \
	third_party/litehtml \
	third_party/libvterm
SYSTEM_APPS := shell.elf fmanager.elf taskmgr.elf ctrlmenu.elf
BUILTIN_APPS := calc.elf texter.elf picturer.elf browser.elf nut.elf
USER_APPS := busyterm.elf bcms-sp.elf sigtest.elf posixdemo.elf libctest.elf dyn-hello
STARDUSTUI_BUILTIN_APPS := helloworld.elf layout.elf duckchat.elf
MUSL_RUNTIME_FILES := ld-musl-x86_64.so.1 libc.so libc.musl-x86_64.so.1 libgcc_s.so.1

# QEMU "--display" expects a display backend type (gtk/sdl/curses/none/...),
# not an X11 DISPLAY string like ":0".
# Use "DISPLAY_BACKEND" to select the backend; keep exporting X11 DISPLAY
# via SUDO_CMD when needed.
DISPLAY_BACKEND ?= gtk,gl=on
USB_XHCI_DEVICES ?= -device usb-kbd,bus=xhci.0,port=1 -device usb-mouse,bus=xhci.0,port=2
AUDIO_BACKEND ?= sdl
HDA_CODEC ?= hda-output
QEMU_CMD = $(SUDO_CMD) qemu-system-x86_64 --display $(DISPLAY_BACKEND) -M q35 -bios OVMF.fd -m 8192 -smp $(SMP) $(KVM_FLAG) -drive if=none,id=harddisk,file=$(VDISK_IMAGE),index=0,format=raw -device ahci,id=ahci -device ide-hd,bus=ahci.0,drive=harddisk -device qemu-xhci,id=xhci $(USB_XHCI_DEVICES) -serial file:serial.log -audiodev $(AUDIO_BACKEND),id=snd0 -device ich9-intel-hda -device $(HDA_CODEC),audiodev=snd0 -netdev user,id=net0,hostfwd=tcp::2323-:2323 -device e1000,netdev=net0 $(DEBUG_FLAG) -monitor stdio  -vga virtio

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
HOST_CC ?= cc
HOST_GCC ?= gcc
HOST_CLANG ?= clang
HOST_CLANGXX ?= clang++
HOST_LLVM_AR ?= llvm-ar
HOST_LLVM_RANLIB ?= llvm-ranlib
IMAGE_TOOLCHAIN ?= clang
IMAGE_PACKAGE_MANAGER ?= xbps
IMAGE_BUSYBOX ?= 0
KERNEL_BUSYBOX_ALIASES ?= $(IMAGE_BUSYBOX)
BF_DIR ?= Bf
HOST_GCC_TRIPLE := $(shell $(HOST_GCC) -dumpmachine 2>/dev/null)
HOST_GCC_VERSION := $(shell $(HOST_GCC) -dumpversion 2>/dev/null)
HOST_GCC_REAL := $(shell readlink -f "$$(command -v $(HOST_GCC))" 2>/dev/null)
HOST_GCC_REAL_NAME := $(notdir $(HOST_GCC_REAL))
HOST_GCC_LIBDIR := $(patsubst %/,%,$(dir $(shell $(HOST_GCC) -print-file-name=libgcc.a 2>/dev/null)))
HOST_GCC_CC1 := $(shell $(HOST_GCC) -print-prog-name=cc1 2>/dev/null)
HOST_GCC_LIBEXECDIR := $(patsubst %/,%,$(dir $(HOST_GCC_CC1)))
HOST_GCC_AS := $(shell command -v $(HOST_GCC_TRIPLE)-as 2>/dev/null || command -v as 2>/dev/null)
HOST_GCC_AS_NAME := $(notdir $(HOST_GCC_AS))
HOST_GCC_LD := $(shell command -v $(HOST_GCC_TRIPLE)-ld.bfd 2>/dev/null || command -v $(HOST_GCC_TRIPLE)-ld 2>/dev/null || command -v ld.bfd 2>/dev/null || command -v ld 2>/dev/null)
HOST_GCC_LD_NAME := $(notdir $(HOST_GCC_LD))
APP_BUSYBOX := resources/apps/busybox
APP_FASTFETCH := resources/apps/fastfetch
XBPS_PREPARE_ROOT ?= Bf/xbps-root
XBPS_BOOTSTRAP_BASE ?= base-minimal bash dash coreutils findutils sed grep gawk diffutils gzip tar bzip2 util-linux ncurses tzdata which fastfetch gcc binutils make strace inetutils bind-utils curl
XBPS_BOOTSTRAP ?= $(XBPS_BOOTSTRAP_BASE)

NASM_OBJS := $(patsubst ./%.asm, ./$(BUILD_DIR)/%.o, $(NASM_FILES))
ASM_OBJS := $(patsubst ./%.S, ./$(BUILD_DIR)/%.o, $(ASM_FILES))
C_OBJS := $(patsubst ./%.c, ./$(BUILD_DIR)/%.o, $(C_FILES))
CPP_OBJS := $(patsubst ./%.cpp, ./$(BUILD_DIR)/%.o, $(CPP_FILES))
KERNEL_OBJS := $(ASM_OBJS) $(C_OBJS) $(CPP_OBJS) $(NASM_OBJS)
BUILD_SETTINGS_HEADER := kernel/build_settings.h
BUILD_CONFIG_HEADER := kernel/build_config.h
FMT_FILES := $(C_FILES:%=%.fmt) $(CPP_FILES:%=%.fmt) $(HEADER_FILES:%=%.fmt)
CHECK_FILES := $(C_FILES:%.c=%.tidy) $(CPP_FILES:%.cpp=%.tidy) $(HEADER_FILES:%.h=%.tidy)
CLANGD_TEMPLATE_CONFS := $(wildcard ./**/.clangd_template ./.clangd_template)
PWD := $(shell pwd)

STAGE_IMAGE_COMMON_ENV = \
	APP_BUSYBOX="$(APP_BUSYBOX)" \
	APP_FASTFETCH="$(APP_FASTFETCH)" \
	BF_DIR="$(BF_DIR)" \
	BUILD_DIR="$(BUILD_DIR)" \
	BUILTIN_APPS="$(BUILTIN_APPS)" \
	HOST_CLANG="$(HOST_CLANG)" \
	HOST_CLANGXX="$(HOST_CLANGXX)" \
	HOST_GCC="$(HOST_GCC)" \
	HOST_GCC_TRIPLE="$(HOST_GCC_TRIPLE)" \
	HOST_LLVM_AR="$(HOST_LLVM_AR)" \
	HOST_LLVM_RANLIB="$(HOST_LLVM_RANLIB)" \
	IMAGE_BUSYBOX="$(IMAGE_BUSYBOX)" \
	IMAGE_PACKAGE_MANAGER="$(IMAGE_PACKAGE_MANAGER)" \
	IMAGE_TOOLCHAIN="$(IMAGE_TOOLCHAIN)" \
	MUSL_RUNTIME_FILES="$(MUSL_RUNTIME_FILES)" \
	SELFHOST_USER_THIRD_PARTY="$(SELFHOST_USER_THIRD_PARTY)" \
	STARDUSTUI_BUILTIN_APPS="$(STARDUSTUI_BUILTIN_APPS)" \
	SYSTEM_APPS="$(SYSTEM_APPS)" \
	USER_APPS="$(USER_APPS)" \
	XBPS_BOOTSTRAP="$(XBPS_BOOTSTRAP)" \
	XBPS_BOOTSTRAP_BASE="$(XBPS_BOOTSTRAP_BASE)" \
	XBPS_PREPARE_ROOT="$(XBPS_PREPARE_ROOT)"
STAGE_IMAGE_OPTIONAL_ENV = \
	$(if $(CLANG_STAGE_FULL_RESOURCE),CLANG_STAGE_FULL_RESOURCE="$(CLANG_STAGE_FULL_RESOURCE)") \
	$(if $(XBPS_CACHEDIR),XBPS_CACHEDIR="$(XBPS_CACHEDIR)") \
	$(if $(XBPS_REPOSITORY),XBPS_REPOSITORY="$(XBPS_REPOSITORY)") \
	$(if $(XBPS_STATIC_ARCH),XBPS_STATIC_ARCH="$(XBPS_STATIC_ARCH)") \
	$(if $(XBPS_STATIC_TARBALL),XBPS_STATIC_TARBALL="$(XBPS_STATIC_TARBALL)") \
	$(if $(XBPS_STATIC_URI),XBPS_STATIC_URI="$(XBPS_STATIC_URI)") \
	$(if $(XBPS_TRIGGERS_DIR),XBPS_TRIGGERS_DIR="$(XBPS_TRIGGERS_DIR)") \
	$(if $(XBPS_TRIGGERS_PKG),XBPS_TRIGGERS_PKG="$(XBPS_TRIGGERS_PKG)")
STAGE_IMAGE_ENV = $(STAGE_IMAGE_COMMON_ENV) $(STAGE_IMAGE_OPTIONAL_ENV)

$(KERNEL_OBJS): $(BUILD_SETTINGS_HEADER) $(BUILD_CONFIG_HEADER)

$(BUILD_CONFIG_HEADER): FORCE
	@tmp="$@.tmp"; \
	{ \
		echo "#pragma once"; \
		echo "#define CONFIG_KERNEL_BUSYBOX_ALIASES $(KERNEL_BUSYBOX_ALIASES)"; \
	} > "$$tmp"; \
	if ! cmp -s "$$tmp" "$@"; then mv "$$tmp" "$@"; else rm -f "$$tmp"; fi

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
	@$(CPP) -fno-pic -fno-pie -mcmodel=large $(CPP_FLAGS) -c $< -o $@
	@echo "CPP $< -> $@"

$(BOOT_EFI): boot/bootx64.c boot/bootlib.c
	@mkdir -p $(@D)
	@x86_64-w64-mingw32-gcc $(BOOT_C_FLAGS) -o $@ $^

$(FONT_BIN_OBJ): font/hankaku.bin
	@mkdir -p $(@D)
	@$(OBJC) -I binary -O elf64-x86-64 ./$< $@

$(LOGO_BIN_OBJ): graphics/logo/xj380_2.png
	@mkdir -p $(@D)
	@$(OBJC) -I binary -O elf64-x86-64 ./$< $@

$(KERNEL_ELF): $(KERNEL_OBJS) $(FONT_BIN_OBJ) $(LOGO_BIN_OBJ) liballoc-x86_64.a linker.ld
	@$(LD) -z muldefs -T linker.ld --static -o $@ $(KERNEL_OBJS) $(FONT_BIN_OBJ) $(LOGO_BIN_OBJ) liballoc-x86_64.a

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
	@echo "# Generated by Makefile" > $@
	@sed "s/\$${workspaceFolder}/$(subst /,\/,${PWD})/g" $@_template >> $@
	@printf "\033[1;32m[Done]\033[0m %s was generated.\n" "$@"

%.clangd: %
	@echo "# Generated by Makefile" > $@
	@sed "s/\$${workspaceFolder}/$(subst /,\/,${PWD})/g" $@_template >> $@
	@printf "\033[1;32m[Done]\033[0m %s was generated.\n" "$@"

liballoc-x86_64.a:
	curl -Lo $@ https://github.com/plos-clan/liballoc/releases/download/release/liballoc-x86_64.a

kernel.artifacts: $(BOOT_EFI) $(KERNEL_ELF)

user.xapi: kernel.artifacts liballoc-x86_64.a
	@$(MAKE) -C user build_xapi

user.apps: user.xapi
	@$(MAKE) -C user

kmods: user.apps
	@$(MAKE) -C $(KMOD_DIR)

stardustui.examples.copy: kmods
	@for dir in $(STARDUSTUI_EXAMPLE_DIRS); do \
		$(MAKE) -C $$dir copy PLATFORM=xj380 || exit $$?; \
	done

all: stardustui.examples.copy

.PHONY: FORCE all clean format check clean.clangd gen.clangd prepare kernel.artifacts user.xapi user.apps kmods stage.selfhost-user stardustui.examples.copy vdisk vmdk run justrun
FORCE:

clean:
	@rm -rf $(BUILD_DIR) $(VDISK_IMAGE) $(VMDK_IMAGE) serial.log $(BUILD_CONFIG_HEADER)
	@echo Removed build files.

prepare:
	@if [ "$(IMAGE_PACKAGE_MANAGER)" = "xbps" ] && [ -n "$(XBPS_BOOTSTRAP)" ]; then \
		chmod -R u+rwX "$(XBPS_PREPARE_ROOT)" 2>/dev/null || true; \
		rm -rf "$(XBPS_PREPARE_ROOT)"; \
		$(STAGE_IMAGE_ENV) sh $(STAGE_IMAGE) xbps-bootstrap "$(XBPS_PREPARE_ROOT)"; \
	else \
		echo "Nothing to prepare for IMAGE_PACKAGE_MANAGER=$(IMAGE_PACKAGE_MANAGER)"; \
	fi

stage.selfhost-user:
	@$(STAGE_IMAGE_ENV) sh $(STAGE_IMAGE) selfhost-user "$(VDISK_ROOT)"

format: $(FMT_FILES)
	@echo "Formatted all the code ..."

check: $(CHECK_FILES)
	@echo "Checked all the code ..."

clean.clangd:
	@rm -f $(CLANGD_TEMPLATE_CONFS:%.clangd_template=%.clangd)

gen.clangd: clean.clangd $(CLANGD_TEMPLATE_CONFS:%.clangd_template=%.clangd)
	@echo "\033[1;32m[Done]\033[0m .clangd configurations was generated.\n"

vdisk: all
	@rm -f $(VDISK_IMAGE)
	@chmod -R u+rwX $(VDISK_ROOT) 2>/dev/null || true
	@rm -rf $(VDISK_ROOT) || { \
		echo "Cannot remove $(VDISK_ROOT); fix ownership with:"; \
		echo "  sudo chown -R $$(id -u):$$(id -g) $(VDISK_ROOT)"; \
		exit 1; \
	}
	@dd if=/dev/zero of=$(VDISK_IMAGE) bs=1M count=$(VDISK_SIZE_MIB)
	@$(VDISK_SUDO) sgdisk $(VDISK_IMAGE) -n 1:$(VDISK_OFFSET_SECTORS) -t 1:ef00
	@$(VDISK_SUDO) mkfs.vfat -F 32 --offset $(VDISK_OFFSET_SECTORS) -S 512 $(VDISK_IMAGE) $$(( $(VDISK_FAT_MIB) * 1024 ))
	@$(STAGE_IMAGE_ENV) sh $(STAGE_IMAGE) populate-root "$(VDISK_ROOT)"
	@mcopy -o -s -i $(VDISK_IMAGE)@@$$(( $(VDISK_OFFSET_SECTORS) * 512 )) $(VDISK_ROOT)/* ::
	@chmod -R u+rwX $(VDISK_ROOT) 2>/dev/null || true
	@rm -rf $(VDISK_ROOT) || { \
		echo "Cannot remove $(VDISK_ROOT); fix ownership with:"; \
		echo "  sudo chown -R $$(id -u):$$(id -g) $(VDISK_ROOT)"; \
		exit 1; \
	}

vmdk: vdisk
	@rm -f $(VMDK_IMAGE)
	@qemu-img convert -O vmdk $(VDISK_IMAGE) $(VMDK_IMAGE)

run: vdisk
	@$(QEMU_CMD)

justrun:
	@$(QEMU_CMD)
