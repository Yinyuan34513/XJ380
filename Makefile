NASM_FLAGS = -f elf64 -DX86_64_TARGET -DUEFI
ASM_FLAGS = -O0 -g -mno-red-zone -mstackrealign -nostdlib -ffreestanding -fno-builtin -m64 -fno-stack-protector -fno-exceptions -mno-80387
C_FLAGS = -O0 -g -mno-red-zone -mstackrealign -nostdlib -ffreestanding -fno-builtin -m64 -fno-stack-protector -fno-exceptions -std=c11 -fshort-wchar -nostdinc -mno-80387 -I./include
CPP_FLAGS = -O0 -g -mno-red-zone -mstackrealign -nostdlib -ffreestanding -fno-builtin -m64 -fno-stack-protector -fno-exceptions -fno-rtti -std=gnu++17 -fshort-wchar -nostdinc -fno-use-cxa-atexit -fno-threadsafe-statics -mno-80387 -Wno-int-to-pointer-cast -Wno-macro-redefined -Wno-c11-extensions -Wno-c99-extensions -Wno-gnu-statement-expression-from-macro-expansion -I./include -Wno-writable-strings -Wno-c++11-narrowing
BOOT_C_FLAGS = -g -I ./boot/include -Wextra -e efi_main -nostdinc -nostdlib -fno-builtin -Wl,--subsystem,10 -fshort-wchar

DEBUG ?= 1
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
VDISK_SIZE_MIB ?= 3072
VDISK_FAT_MIB ?= 3072
VDISK_ROOT ?= /tmp/xj380-vdisk-$$(id -u)/XJ380
KMOD_DIR := kmod
STARDUSTUI_EXAMPLE_DIRS := \
	frameworks/StardustUI/examples/helloworld \
	frameworks/StardustUI/examples/layout \
	$(wildcard frameworks/StardustUI/examples/duckchat)

# QEMU "--display" expects a display backend type (gtk/sdl/curses/none/...),
# not an X11 DISPLAY string like ":0".
# Use "DISPLAY_BACKEND" to select the backend; keep exporting X11 DISPLAY
# via SUDO_CMD when needed.
DISPLAY_BACKEND ?= gtk
USB_XHCI_DEVICES ?= -device usb-kbd,bus=xhci.0,port=1 -device usb-mouse,bus=xhci.0,port=2
AUDIO_BACKEND ?= sdl
HDA_CODEC ?= hda-output
QEMU_CMD = $(SUDO_CMD) qemu-system-x86_64 --display $(DISPLAY_BACKEND) -M q35 -bios OVMF.fd -m 8192 -smp $(SMP) $(KVM_FLAG) -drive if=none,id=harddisk,file=XJ380.img,index=0,format=raw -device ahci,id=ahci -device ide-hd,bus=ahci.0,drive=harddisk -device qemu-xhci,id=xhci $(USB_XHCI_DEVICES) -serial file:serial.log -audiodev $(AUDIO_BACKEND),id=snd0 -device ich9-intel-hda -device $(HDA_CODEC),audiodev=snd0 -netdev user,id=net0,hostfwd=tcp::2323-:2323 -device e1000,netdev=net0 $(DEBUG_FLAG) -monitor stdio

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
IMAGE_BUSYBOX ?= 1
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

NASM_OBJS := $(patsubst ./%.asm, ./$(BUILD_DIR)/%.o, $(NASM_FILES))
ASM_OBJS := $(patsubst ./%.S, ./$(BUILD_DIR)/%.o, $(ASM_FILES))
C_OBJS := $(patsubst ./%.c, ./$(BUILD_DIR)/%.o, $(C_FILES))
CPP_OBJS := $(patsubst ./%.cpp, ./$(BUILD_DIR)/%.o, $(CPP_FILES))
KERNEL_OBJS := $(ASM_OBJS) $(C_OBJS) $(CPP_OBJS) $(NASM_OBJS)
BUILD_SETTINGS_HEADER := kernel/build_settings.h
BUILD_CONFIG_HEADER := kernel/build_config.h
STAGE_ELF_DEPS := tools/stage_elf_deps.sh
STAGE_IMAGE_BASE := tools/stage_image_base.sh
STAGE_IMAGE_TOOLCHAIN := tools/stage_image_toolchain.sh
STAGE_IMAGE_XBPS := tools/stage_image_xbps.sh
STAGE_XBPS_BOOTSTRAP := tools/stage_xbps_bootstrap.sh
STAGE_PREPARED_ROOT := tools/stage_prepared_root.sh
MAKE_PAK := tools/make_pak.py
MAKE_INSTALLER_ISO := tools/make_installer_iso.sh
INSTALLER_STAGE_ROOT ?= /tmp/xj380-installer-$$(id -u)
INSTALLER_SYSTEM_ROOT := $(INSTALLER_STAGE_ROOT)/system-root
INSTALLER_ROOT := $(INSTALLER_STAGE_ROOT)/installer-root
INSTALLER_ISO_ROOT := $(INSTALLER_STAGE_ROOT)/iso-root
INSTALLER_ROOT_PAK := $(BUILD_DIR)/installer-root.pak
INSTALLER_SYSTEM_PAK := $(BUILD_DIR)/system-payload.pak
SELFHOST_USER_THIRD_PARTY := \
	third_party/mbedtls-src \
	third_party/litehtml \
	third_party/libvterm
XBPS_PREPARE_ROOT ?= Bf/xbps-root
XBPS_PREPARE_MARKER := $(XBPS_PREPARE_ROOT)/.xj380-bootstrap-complete
XBPS_BOOTSTRAP_BASE ?= base-minimal bash dash coreutils findutils sed grep gawk diffutils gzip tar bzip2 util-linux ncurses tzdata which fastfetch gcc binutils make strace inetutils bind-utils curl
XBPS_BOOTSTRAP ?= $(XBPS_BOOTSTRAP_BASE)
APP_BUSYBOX := resources/apps/busybox
APP_FASTFETCH := resources/apps/fastfetch
FMT_FILES := $(C_FILES:%=%.fmt) $(CPP_FILES:%=%.fmt) $(HEADER_FILES:%=%.fmt)
CHECK_FILES := $(C_FILES:%.c=%.tidy) $(CPP_FILES:%.cpp=%.tidy) $(HEADER_FILES:%.h=%.tidy)
CLANGD_TEMPLATE_CONFS := $(wildcard ./**/.clangd_template ./.clangd_template)
PWD := $(shell pwd)

$(KERNEL_OBJS): $(BUILD_SETTINGS_HEADER) $(BUILD_CONFIG_HEADER)

$(BUILD_CONFIG_HEADER): FORCE
	@tmp="$@.tmp"; \
	{ \
		echo "#pragma once"; \
		echo "#define CONFIG_KERNEL_BUSYBOX_ALIASES $(KERNEL_BUSYBOX_ALIASES)"; \
	} > "$$tmp"; \
	if ! cmp -s "$$tmp" "$@"; then mv "$$tmp" "$@"; else rm -f "$$tmp"; fi

.PHONY: FORCE
FORCE:

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

all: $(KERNEL_OBJS) liballoc-x86_64.a
# 针对BOOT和KERNEL进行特殊处理
	@x86_64-w64-mingw32-gcc $(BOOT_C_FLAGS) -o $(BUILD_DIR)/BOOTX64.efi boot/bootx64.c boot/bootlib.c

	@$(OBJC) -I binary -O elf64-x86-64 ./font/hankaku.bin ./font/hankaku.o
	@$(OBJC) -I binary -O elf64-x86-64 ./graphics/logo/xj380_2.png ./graphics/logo/xj380_2.o

	@$(LD) -z muldefs -T linker.ld --static -o ./$(BUILD_DIR)/kernel.elf $(KERNEL_OBJS) ./font/hankaku.o ./graphics/logo/xj380_2.o liballoc-x86_64.a

	@$(MAKE) -C user build_xapi
	@$(MAKE) -C user
	@$(MAKE) -C $(KMOD_DIR)
	@$(MAKE) stardustui.examples.copy

liballoc-x86_64.a:
	curl -Lo liballoc-x86_64.a https://github.com/plos-clan/liballoc/releases/download/release/liballoc-x86_64.a

.PHONY: clean format check gen.clangd prepare stage.selfhost-user stardustui.examples.copy installer.prepare installer.system.stage installer.root.stage installer.iso

clean:
# 事实上直接删out/最简单了
	@rm -rf $(BUILD_DIR) ./XJ380.img serial.log $(BUILD_CONFIG_HEADER)
	@echo Removed build files.

prepare:
	@if [ "$(IMAGE_PACKAGE_MANAGER)" = "xbps" ] && [ -n "$(XBPS_BOOTSTRAP)" ]; then \
		chmod -R u+rwX "$(XBPS_PREPARE_ROOT)" 2>/dev/null || true; \
		rm -rf "$(XBPS_PREPARE_ROOT)"; \
		IMAGE_BUSYBOX="$(IMAGE_BUSYBOX)" XBPS_BOOTSTRAP="$(XBPS_BOOTSTRAP)" XBPS_BOOTSTRAP_BASE="$(XBPS_BOOTSTRAP_BASE)" BF_DIR="$(BF_DIR)" sh $(STAGE_XBPS_BOOTSTRAP) "$(XBPS_PREPARE_ROOT)"; \
		touch "$(XBPS_PREPARE_MARKER)"; \
	else \
		echo "Nothing to prepare for IMAGE_PACKAGE_MANAGER=$(IMAGE_PACKAGE_MANAGER)"; \
	fi

installer.prepare:
	@if [ "$(IMAGE_PACKAGE_MANAGER)" = "xbps" ] && [ -n "$(XBPS_BOOTSTRAP)" ] && [ ! -f "$(XBPS_PREPARE_MARKER)" ] && [ ! -x "$(XBPS_PREPARE_ROOT)/usr/bin/gcc" ]; then \
		echo "Preparing installer xbps root: $(XBPS_PREPARE_ROOT)"; \
		chmod -R u+rwX "$(XBPS_PREPARE_ROOT)" 2>/dev/null || true; \
		rm -rf "$(XBPS_PREPARE_ROOT)"; \
		IMAGE_BUSYBOX="$(IMAGE_BUSYBOX)" XBPS_BOOTSTRAP="$(XBPS_BOOTSTRAP)" XBPS_BOOTSTRAP_BASE="$(XBPS_BOOTSTRAP_BASE)" BF_DIR="$(BF_DIR)" sh $(STAGE_XBPS_BOOTSTRAP) "$(XBPS_PREPARE_ROOT)"; \
		touch "$(XBPS_PREPARE_MARKER)"; \
	fi

stage.selfhost-user:
	@rm -rf $(VDISK_ROOT)/apps/user $(VDISK_ROOT)/apps/include $(VDISK_ROOT)/apps/lib $(VDISK_ROOT)/apps/third_party $(VDISK_ROOT)/apps/liballoc-x86_64.a
	@mkdir -p $(VDISK_ROOT)/apps/third_party
	@cp -a ./user $(VDISK_ROOT)/apps/user
	@cp -a ./include $(VDISK_ROOT)/apps/include
	@cp -a ./lib $(VDISK_ROOT)/apps/lib
	@cp -a ./liballoc-x86_64.a $(VDISK_ROOT)/apps/liballoc-x86_64.a
	@for dir in $(SELFHOST_USER_THIRD_PARTY); do \
		cp -a ./$$dir $(VDISK_ROOT)/apps/third_party/; \
	done
	@find $(VDISK_ROOT)/apps/user $(VDISK_ROOT)/apps/third_party -type d -name .git -prune -exec rm -rf {} +

stardustui.examples.copy:
	@for dir in $(STARDUSTUI_EXAMPLE_DIRS); do \
		$(MAKE) -C $$dir copy PLATFORM=xj380 || exit $$?; \
	done

format: $(FMT_FILES)
	@echo "Formatted all the code ..."

check: $(CHECK_FILES)
	@echo "Checked all the code ..."

clean.clangd:
	@rm -f $(CLANGD_TEMPLATE_CONFS:%.clangd_template=%.clangd)

gen.clangd: clean.clangd $(CLANGD_TEMPLATE_CONFS:%.clangd_template=%.clangd)
	@echo "\033[1;32m[Done]\033[0m .clangd configurations was generated.\n"

generate_image: all
	@rm -rf XJ380.img
	@chmod -R u+rwX $(VDISK_ROOT) 2>/dev/null || true
	@rm -rf $(VDISK_ROOT) || { \
		echo "Cannot remove $(VDISK_ROOT); fix ownership with:"; \
		echo "  sudo chown -R $$(id -u):$$(id -g) $(VDISK_ROOT)"; \
		exit 1; \
	}
	@dd if=/dev/zero of=XJ380.img bs=1M count=$(VDISK_SIZE_MIB)
# You must execute the following command with sudo privileges, that's not good...
	@$(VDISK_SUDO) sgdisk XJ380.img -n 1:2048 -t 1:ef00

	@$(VDISK_SUDO) mkfs.vfat -F 32 --offset 2048 -S 512 XJ380.img $$(( $(VDISK_FAT_MIB) * 1024 ))

	@sh $(STAGE_IMAGE_BASE) $(VDISK_ROOT)

	@cp -a ./user/fmanager/fm_bg.png $(VDISK_ROOT)/system/resources/image

	@cp -a $(BUILD_DIR)/BOOTX64.efi $(VDISK_ROOT)/EFI/BOOT
	@cp -a $(BUILD_DIR)/kernel.elf $(VDISK_ROOT)/system

	@cp -a $(BUILD_DIR)/shell.elf    $(VDISK_ROOT)/apps/system
	@cp -a $(BUILD_DIR)/fmanager.elf $(VDISK_ROOT)/apps/system
	@cp -a $(BUILD_DIR)/taskmgr.elf  $(VDISK_ROOT)/apps/system
	@cp -a $(BUILD_DIR)/ctrlmenu.elf $(VDISK_ROOT)/apps/system
	@cp -a $(BUILD_DIR)/filedlg.elf  $(VDISK_ROOT)/apps/system

	@cp -a $(BUILD_DIR)/calc.elf     $(VDISK_ROOT)/apps/builtin
	@cp -a $(BUILD_DIR)/texter.elf   $(VDISK_ROOT)/apps/builtin
	@cp -a $(BUILD_DIR)/picturer.elf $(VDISK_ROOT)/apps/builtin
	@cp -a $(BUILD_DIR)/browser.elf $(VDISK_ROOT)/apps/builtin
	@cp -a $(BUILD_DIR)/nut.elf $(VDISK_ROOT)/apps/builtin

#	@cp -a $(BUILD_DIR)/init.elf $(BUILD_DIR)/XJ380/apps
#	@cp -a $(BUILD_DIR)/cp537.elf $(BUILD_DIR)/XJ380/apps
	@cp -a $(BUILD_DIR)/busyterm.elf $(VDISK_ROOT)/apps
	@cp -a $(BUILD_DIR)/bcms-sp.elf $(VDISK_ROOT)/apps
	@cp -a $(BUILD_DIR)/sigtest.elf $(VDISK_ROOT)/apps
	@cp -a $(BUILD_DIR)/posixdemo.elf $(VDISK_ROOT)/apps
	@cp -a $(BUILD_DIR)/libctest.elf $(VDISK_ROOT)/apps
	@cp -a $(BUILD_DIR)/dyn-hello $(VDISK_ROOT)/apps
	@$(MAKE) stage.selfhost-user VDISK_ROOT="$(VDISK_ROOT)"
	@cp -a /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 $(VDISK_ROOT)/lib64/ld-linux-x86-64.so.2

	@cp -a resources/musl/ld-musl-x86_64.so.1 $(VDISK_ROOT)/lib/ld-musl-x86_64.so.1
	@cp -a resources/musl/libc.so $(VDISK_ROOT)/lib/libc.so
	@cp -a resources/musl/libc.musl-x86_64.so.1 $(VDISK_ROOT)/lib/libc.musl-x86_64.so.1
	@cp -a resources/musl/libgcc_s.so.1 $(VDISK_ROOT)/lib/libgcc_s.so.1

	@cp -a /lib/$(HOST_GCC_TRIPLE)/libc.so.6 $(VDISK_ROOT)/lib64/libc.so.6
	@if [ "$(IMAGE_BUSYBOX)" = "1" ]; then cp -a $(APP_BUSYBOX) $(VDISK_ROOT)/apps/busybox; fi
	@if [ -f ./lolcat_100.0.1-3_all.deb ]; then cp -a ./lolcat_100.0.1-3_all.deb $(VDISK_ROOT)/apps/lolcat.deb; fi
	@cp -a $(APP_FASTFETCH) $(VDISK_ROOT)/apps/fastfetch
	@cp ./1.c $(VDISK_ROOT)/apps/1.c
	@IMAGE_TOOLCHAIN="$(IMAGE_TOOLCHAIN)" HOST_GCC="$(HOST_GCC)" HOST_CLANG="$(HOST_CLANG)" HOST_CLANGXX="$(HOST_CLANGXX)" HOST_LLVM_AR="$(HOST_LLVM_AR)" HOST_LLVM_RANLIB="$(HOST_LLVM_RANLIB)" STAGE_ELF_DEPS="$(STAGE_ELF_DEPS)" sh $(STAGE_IMAGE_TOOLCHAIN) $(VDISK_ROOT)
	@$(STAGE_ELF_DEPS) $(VDISK_ROOT) $(APP_FASTFETCH)
	@if [ "$(IMAGE_PACKAGE_MANAGER)" = "xbps" ]; then \
		IMAGE_BUSYBOX="$(IMAGE_BUSYBOX)" APP_BUSYBOX="$(APP_BUSYBOX)" STAGE_ELF_DEPS="$(STAGE_ELF_DEPS)" sh $(STAGE_IMAGE_XBPS) $(VDISK_ROOT); \
		if [ -n "$(XBPS_BOOTSTRAP)" ]; then \
			if [ ! -d "$(XBPS_PREPARE_ROOT)/var/db/xbps" ]; then \
				echo "missing prepared xbps root: $(XBPS_PREPARE_ROOT)" >&2; \
				echo "run: make prepare" >&2; \
				exit 1; \
			fi; \
			IMAGE_BUSYBOX="$(IMAGE_BUSYBOX)" sh $(STAGE_PREPARED_ROOT) "$(XBPS_PREPARE_ROOT)" $(VDISK_ROOT); \
		fi; \
	elif [ "$(IMAGE_PACKAGE_MANAGER)" != "none" ]; then \
		echo "unsupported IMAGE_PACKAGE_MANAGER=$(IMAGE_PACKAGE_MANAGER) (expected xbps or none)" >&2; \
		exit 1; \
	fi

	@cp -a ./font/ttf/XJ380F.ttf $(VDISK_ROOT)/system/font
	@cp -a ./font/ttf/XJ380C.ttf $(VDISK_ROOT)/system/font

	@cp -a ./graphics/window/theme/skyglass/*.* $(VDISK_ROOT)/system/theme/skyglass

	@cp -a ./graphics/peek.png $(VDISK_ROOT)/system/foxland

	@cp -a ./graphics/logo/*.* $(VDISK_ROOT)/system
# 	@cp -a ./test.wav $(BUILD_DIR)/XJ380/system

	@cp -a ./graphics/icon/*.* $(VDISK_ROOT)/system/icon

	@cp -a ./out/*.km $(VDISK_ROOT)/mod

	@cp -a ./resources/* $(VDISK_ROOT)/system/resources

	@cp ./out/helloworld.elf $(VDISK_ROOT)/apps/builtin
	@cp ./out/layout.elf $(VDISK_ROOT)/apps/builtin
	@if [ -d ./frameworks/StardustUI/examples/duckchat ] && [ -f ./out/duckchat.elf ]; then cp ./out/duckchat.elf $(VDISK_ROOT)/apps/builtin; fi

	@find $(VDISK_ROOT) -xtype l -delete
	@find $(VDISK_ROOT) -type l -exec sh -c 'for lnk do tmp=$${lnk}.xj380-copy; cp -aL "$$lnk" "$$tmp" 2>/dev/null && rm -f "$$lnk" && mv "$$tmp" "$$lnk" || { rm -rf "$$tmp"; rm -f "$$lnk"; }; done' sh {} +
	@mcopy -o -s -i XJ380.img@@$$((2048*512)) $(VDISK_ROOT)/* ::

	@chmod -R u+rwX $(VDISK_ROOT) 2>/dev/null || true

grmti: generate_image
	cp -a /tmp/xj380-vdisk-1000/XJ380 .
	@rm -rf $(VDISK_ROOT) || { \
		echo "Cannot remove $(VDISK_ROOT); fix ownership with:"; \
		echo "  sudo chown -R $$(id -u):$$(id -g) $(VDISK_ROOT)"; \
		exit 1; \
	}

vdisk: generate_image
	@rm -rf $(VDISK_ROOT) || { \
		echo "Cannot remove $(VDISK_ROOT); fix ownership with:"; \
		echo "  sudo chown -R $$(id -u):$$(id -g) $(VDISK_ROOT)"; \
		exit 1; \
	}

installer.system.stage: all installer.prepare
	@chmod -R u+rwX $(INSTALLER_SYSTEM_ROOT) 2>/dev/null || true
	@rm -rf $(INSTALLER_SYSTEM_ROOT)
	@sh $(STAGE_IMAGE_BASE) $(INSTALLER_SYSTEM_ROOT)
	@cp -a ./user/fmanager/fm_bg.png $(INSTALLER_SYSTEM_ROOT)/system/resources/image
	@cp -a $(BUILD_DIR)/BOOTX64.efi $(INSTALLER_SYSTEM_ROOT)/EFI/BOOT
	@cp -a $(BUILD_DIR)/kernel.elf $(INSTALLER_SYSTEM_ROOT)/system
	@cp -a $(BUILD_DIR)/shell.elf    $(INSTALLER_SYSTEM_ROOT)/apps/system
	@cp -a $(BUILD_DIR)/fmanager.elf $(INSTALLER_SYSTEM_ROOT)/apps/system
	@cp -a $(BUILD_DIR)/taskmgr.elf  $(INSTALLER_SYSTEM_ROOT)/apps/system
	@cp -a $(BUILD_DIR)/ctrlmenu.elf $(INSTALLER_SYSTEM_ROOT)/apps/system
	@cp -a $(BUILD_DIR)/filedlg.elf  $(INSTALLER_SYSTEM_ROOT)/apps/system
	@cp -a $(BUILD_DIR)/calc.elf     $(INSTALLER_SYSTEM_ROOT)/apps/builtin
	@cp -a $(BUILD_DIR)/texter.elf   $(INSTALLER_SYSTEM_ROOT)/apps/builtin
	@cp -a $(BUILD_DIR)/picturer.elf $(INSTALLER_SYSTEM_ROOT)/apps/builtin
	@cp -a $(BUILD_DIR)/browser.elf  $(INSTALLER_SYSTEM_ROOT)/apps/builtin
	@cp -a $(BUILD_DIR)/nut.elf      $(INSTALLER_SYSTEM_ROOT)/apps/builtin
	@cp -a $(BUILD_DIR)/busyterm.elf $(INSTALLER_SYSTEM_ROOT)/apps
	@cp -a $(BUILD_DIR)/bcms-sp.elf $(INSTALLER_SYSTEM_ROOT)/apps
	@cp -a $(BUILD_DIR)/sigtest.elf $(INSTALLER_SYSTEM_ROOT)/apps
	@cp -a $(BUILD_DIR)/posixdemo.elf $(INSTALLER_SYSTEM_ROOT)/apps
	@cp -a $(BUILD_DIR)/libctest.elf $(INSTALLER_SYSTEM_ROOT)/apps
	@cp -a $(BUILD_DIR)/dyn-hello $(INSTALLER_SYSTEM_ROOT)/apps
	@$(MAKE) stage.selfhost-user VDISK_ROOT="$(INSTALLER_SYSTEM_ROOT)"
	@cp -a /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 $(INSTALLER_SYSTEM_ROOT)/lib64/ld-linux-x86-64.so.2
	@cp -a resources/musl/ld-musl-x86_64.so.1 $(INSTALLER_SYSTEM_ROOT)/lib/ld-musl-x86_64.so.1
	@cp -a resources/musl/libc.so $(INSTALLER_SYSTEM_ROOT)/lib/libc.so
	@cp -a resources/musl/libc.musl-x86_64.so.1 $(INSTALLER_SYSTEM_ROOT)/lib/libc.musl-x86_64.so.1
	@cp -a resources/musl/libgcc_s.so.1 $(INSTALLER_SYSTEM_ROOT)/lib/libgcc_s.so.1
	@cp -a /lib/$(HOST_GCC_TRIPLE)/libc.so.6 $(INSTALLER_SYSTEM_ROOT)/lib64/libc.so.6
	@if [ "$(IMAGE_BUSYBOX)" = "1" ]; then cp -a $(APP_BUSYBOX) $(INSTALLER_SYSTEM_ROOT)/apps/busybox; fi
	@if [ -f ./lolcat_100.0.1-3_all.deb ]; then cp -a ./lolcat_100.0.1-3_all.deb $(INSTALLER_SYSTEM_ROOT)/apps/lolcat.deb; fi
	@cp -a $(APP_FASTFETCH) $(INSTALLER_SYSTEM_ROOT)/apps/fastfetch
	@cp ./1.c $(INSTALLER_SYSTEM_ROOT)/apps/1.c
	@IMAGE_TOOLCHAIN="$(IMAGE_TOOLCHAIN)" HOST_GCC="$(HOST_GCC)" HOST_CLANG="$(HOST_CLANG)" HOST_CLANGXX="$(HOST_CLANGXX)" HOST_LLVM_AR="$(HOST_LLVM_AR)" HOST_LLVM_RANLIB="$(HOST_LLVM_RANLIB)" STAGE_ELF_DEPS="$(STAGE_ELF_DEPS)" sh $(STAGE_IMAGE_TOOLCHAIN) $(INSTALLER_SYSTEM_ROOT)
	@$(STAGE_ELF_DEPS) $(INSTALLER_SYSTEM_ROOT) $(APP_FASTFETCH)
	@if [ "$(IMAGE_PACKAGE_MANAGER)" = "xbps" ]; then \
		IMAGE_BUSYBOX="$(IMAGE_BUSYBOX)" APP_BUSYBOX="$(APP_BUSYBOX)" STAGE_ELF_DEPS="$(STAGE_ELF_DEPS)" sh $(STAGE_IMAGE_XBPS) $(INSTALLER_SYSTEM_ROOT); \
		if [ -n "$(XBPS_BOOTSTRAP)" ]; then \
			if [ ! -d "$(XBPS_PREPARE_ROOT)/var/db/xbps" ]; then \
				echo "missing prepared xbps root: $(XBPS_PREPARE_ROOT)" >&2; \
				echo "run: make prepare" >&2; \
				exit 1; \
			fi; \
			IMAGE_BUSYBOX="$(IMAGE_BUSYBOX)" sh $(STAGE_PREPARED_ROOT) "$(XBPS_PREPARE_ROOT)" $(INSTALLER_SYSTEM_ROOT); \
		fi; \
	elif [ "$(IMAGE_PACKAGE_MANAGER)" != "none" ]; then \
		echo "unsupported IMAGE_PACKAGE_MANAGER=$(IMAGE_PACKAGE_MANAGER) (expected xbps or none)" >&2; \
		exit 1; \
	fi
	@cp -a ./font/ttf/XJ380F.ttf $(INSTALLER_SYSTEM_ROOT)/system/font
	@cp -a ./font/ttf/XJ380C.ttf $(INSTALLER_SYSTEM_ROOT)/system/font
	@cp -a ./graphics/window/theme/skyglass/*.* $(INSTALLER_SYSTEM_ROOT)/system/theme/skyglass
	@cp -a ./graphics/peek.png $(INSTALLER_SYSTEM_ROOT)/system/foxland
	@cp -a ./graphics/logo/*.* $(INSTALLER_SYSTEM_ROOT)/system
	@cp -a ./graphics/icon/*.* $(INSTALLER_SYSTEM_ROOT)/system/icon
	@cp -a ./out/*.km $(INSTALLER_SYSTEM_ROOT)/mod
	@cp -a ./resources/* $(INSTALLER_SYSTEM_ROOT)/system/resources
	@cp ./out/helloworld.elf $(INSTALLER_SYSTEM_ROOT)/apps/builtin
	@cp ./out/layout.elf $(INSTALLER_SYSTEM_ROOT)/apps/builtin
	@if [ -d ./frameworks/StardustUI/examples/duckchat ] && [ -f ./out/duckchat.elf ]; then cp ./out/duckchat.elf $(INSTALLER_SYSTEM_ROOT)/apps/builtin; fi
	@find $(INSTALLER_SYSTEM_ROOT) -xtype l -delete
	@find $(INSTALLER_SYSTEM_ROOT) -type l -exec sh -c 'for lnk do tmp=$${lnk}.xj380-copy; cp -aL "$$lnk" "$$tmp" 2>/dev/null && rm -f "$$lnk" && mv "$$tmp" "$$lnk" || { rm -rf "$$tmp"; rm -f "$$lnk"; }; done' sh {} +
	@python3 $(MAKE_PAK) $(INSTALLER_SYSTEM_ROOT) $(INSTALLER_SYSTEM_PAK)

installer.root.stage: all
	@chmod -R u+rwX $(INSTALLER_ROOT) 2>/dev/null || true
	@rm -rf $(INSTALLER_ROOT)
	@mkdir -p $(INSTALLER_ROOT)/apps $(INSTALLER_ROOT)/apps/system $(INSTALLER_ROOT)/apps/builtin
	@mkdir -p $(INSTALLER_ROOT)/system/font $(INSTALLER_ROOT)/system/theme/skyglass $(INSTALLER_ROOT)/system/icon $(INSTALLER_ROOT)/system/resources/image $(INSTALLER_ROOT)/tmp
	@cp -a $(BUILD_DIR)/installer.elf $(INSTALLER_ROOT)/apps/installer.elf
	@cp -a $(BUILD_DIR)/shell.elf $(INSTALLER_ROOT)/apps/system/shell.elf
	@cp -a $(BUILD_DIR)/filedlg.elf $(INSTALLER_ROOT)/apps/system/filedlg.elf
	@cp -a ./font/ttf/XJ380F.ttf $(INSTALLER_ROOT)/system/font
	@cp -a ./font/ttf/XJ380C.ttf $(INSTALLER_ROOT)/system/font
	@cp -a ./graphics/window/theme/skyglass/*.* $(INSTALLER_ROOT)/system/theme/skyglass
	@cp -a ./graphics/icon/*.* $(INSTALLER_ROOT)/system/icon
	@cp -a ./graphics/logo/*.* $(INSTALLER_ROOT)/system
	@cp -a ./resources/svg $(INSTALLER_ROOT)/system/resources/svg
	@python3 $(MAKE_PAK) $(INSTALLER_ROOT) $(INSTALLER_ROOT_PAK)

installer.iso: installer.system.stage installer.root.stage
	@rm -f XJ380-installer.iso
	@sh $(MAKE_INSTALLER_ISO) $(INSTALLER_ISO_ROOT) $(BUILD_DIR)/BOOTX64.efi $(BUILD_DIR)/kernel.elf $(INSTALLER_ROOT_PAK) $(INSTALLER_SYSTEM_PAK)


vmdk: vdisk
	@rm -rf XJ380.vmdk
	@qemu-img convert -O vmdk XJ380.img XJ380.vmdk

run: vdisk
	@$(QEMU_CMD)

justrun:
	@$(QEMU_CMD)
