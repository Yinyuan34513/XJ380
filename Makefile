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
SUDO_CMD = sudo env DISPLAY=$(DISPLAY) XAUTHORITY=$(XAUTHORITY)
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
VDISK_ESP_IMAGE := XJ380-esp.img
VDISK_ROOTFS_IMAGE := XJ380-rootfs.img
VDISK_SIZE_MIB ?= 14096
VDISK_OFFSET_SECTORS ?= 2048
VDISK_ESP_START_MIB ?= 1
VDISK_ROOTFS_START_MIB ?= 256
VDISK_ESP_MIB := $(shell expr $(VDISK_ROOTFS_START_MIB) - $(VDISK_ESP_START_MIB))
VDISK_ROOTFS_MIB ?= $(shell expr $(VDISK_SIZE_MIB) - $(VDISK_ROOTFS_START_MIB))
VDISK_EXT_BLOCK_SIZE ?= 1024
VDISK_EXT_INODE_SIZE ?= 256
VDISK_EXT_FEATURES ?= extent,64bit,flex_bg,huge_file,dir_nlink,extra_isize,dir_index,metadata_csum,^has_journal,^quota,^metadata_csum_seed,^orphan_file,^project,^encrypt,^verity,^casefold,^inline_data,^ea_inode,^bigalloc,^mmp,^fast_commit,^sparse_super2
VDISK_ROOT ?= $(XBPS_PREPARE_ROOT)
VDISK_ROOTFS_START_SECTORS := $(shell expr $(VDISK_ROOTFS_START_MIB) \* 2048)
VDISK_ESP_END_SECTORS := $(shell expr $(VDISK_ROOTFS_START_SECTORS) - 1)
KMOD_DIR := kmod
STARDUSTUI_DUCKCHAT_DIR := $(wildcard frameworks/StardustUI/examples/duckchat)
STARDUSTUI_EXAMPLE_DIRS := \
	frameworks/StardustUI/examples/helloworld \
	frameworks/StardustUI/examples/layout \
	$(STARDUSTUI_DUCKCHAT_DIR)
SYSTEM_APPS := shell.elf fmanager.elf taskmgr.elf ctrlmenu.elf
BUILTIN_APPS := calc.elf texter.elf picturer.elf browser.elf nut.elf
USER_APPS := init.elf terminal.elf bcms-sp.elf sigtest.elf posixdemo.elf libctest.elf scmtest.elf anonfdtest.elf dyn-hello
STARDUSTUI_BUILTIN_APPS := helloworld.elf layout.elf $(if $(STARDUSTUI_DUCKCHAT_DIR),duckchat.elf)

# QEMU "--display" expects a display backend type (gtk/sdl/curses/none/...),
# not an X11 DISPLAY string like ":0".
# Use "DISPLAY_BACKEND" to select the backend; keep exporting X11 DISPLAY
# via SUDO_CMD when needed.
DISPLAY_BACKEND ?= gtk
USB_XHCI_DEVICES ?= -device usb-kbd,bus=xhci.0,port=1 -device usb-mouse,bus=xhci.0,port=2
AUDIO ?= 0
AUDIO_BACKEND ?= sdl
HDA_CODEC ?= hda-output
ifeq ($(AUDIO), 1)
QEMU_AUDIO_ARGS = -audiodev $(AUDIO_BACKEND),id=snd0 -device ich9-intel-hda -device $(HDA_CODEC),audiodev=snd0
endif
QEMU_CMD = $(SUDO_CMD) qemu-system-x86_64 --display $(DISPLAY_BACKEND) -M q35 -bios OVMF.fd -m 8192 -smp $(SMP) $(KVM_FLAG) -drive if=none,id=harddisk,file=$(VDISK_IMAGE),index=0,format=raw -device ahci,id=ahci -device ide-hd,bus=ahci.0,drive=harddisk -device qemu-xhci,id=xhci $(USB_XHCI_DEVICES) -serial file:serial.log $(QEMU_AUDIO_ARGS) -netdev user,id=net0,hostfwd=tcp::2323-:2323 -device e1000,netdev=net0 $(DEBUG_FLAG) -vga virtio

COMPILER_PREFIX ?=

# FILES
NASM_FILES := $(shell find ./kernel ./driver ./graphics ./font ./lib -name "*.asm")
ASM_FILES := $(shell find ./kernel ./driver ./graphics ./font ./lib -name "*.S")
C_FILES := $(shell find ./kernel ./driver ./graphics ./font ./lib -name "*.c")
CPP_FILES := $(shell find ./kernel ./driver ./graphics ./font ./lib -name "*.cpp")
HEADER_FILES := $(shell find ./include -name "*.h" -o -name "*.hpp")
LWEXT4_C_FILES := $(shell find ./third_party/lwext4/src -name "*.c")

export CC = $(COMPILER_PREFIX)clang
export CPP = $(COMPILER_PREFIX)clang++
export NASM = $(COMPILER_PREFIX)nasm
export LD = $(COMPILER_PREFIX)ld
export NM = $(COMPILER_PREFIX)nm
export OBJC = $(COMPILER_PREFIX)objcopy
export OBJD = $(COMPILER_PREFIX)objdump
HOST_CC ?= cc
IMAGE_PACKAGE_MANAGER ?= xbps
KERNEL_BUSYBOX_ALIASES ?= 0
KERNEL_START_DESKTOP ?= 0
KERNEL_START_COMPONENT_FLUSHER ?= $(KERNEL_START_DESKTOP)
KERNEL_UDEV_TRACE ?= 0
KERNEL_BUSYBOX_DEBUG ?= 0
KERNEL_LINUX_OUTPUT_MIRROR ?= 0
KERNEL_XBPS_WAIT_DEBUG ?= 0
KERNEL_PROFILER ?= 1
BF_DIR ?= Bf
XBPS_BOOTSTRAP_EXTRA ?= weston seatd eudev dbus wayland wayland-protocols libxkbcommon pixman xkeyboard-config adwaita-icon-theme dejavu-fonts-ttf glibc-locales xfce4 labwc xorg-server-xwayland xrandr pciutils sudo iw ptyxis adwaita-cursor-theme
XBPS_PREPARE_ROOT ?= Bf/xbps-root
XBPS_ARCH ?= x86_64
XBPS_INSTALL ?= $(BF_DIR)/usr/bin/xbps-install
XBPS_STATIC_TARBALL ?= $(BF_DIR)/xbps-static-latest.$(XBPS_ARCH)-musl.tar.xz
XBPS_REPOSITORY ?= http://mirrors.tuna.tsinghua.edu.cn/voidlinux/current
XBPS_CACHEDIR ?= $(BF_DIR)/xbps-cache
XBPS_BOOTSTRAP_BASE ?= base-minimal bash dash coreutils findutils sed grep gawk diffutils gzip tar bzip2 util-linux ncurses tzdata which shadow fastfetch gcc binutils make strace elfutils git inetutils bind-utils curl
XBPS_BOOTSTRAP ?= $(strip $(XBPS_BOOTSTRAP_BASE) $(XBPS_BOOTSTRAP_EXTRA))
KERNEL_TRACE_DEFS := \
	-DCONFIG_KERNEL_UDEV_TRACE=$(KERNEL_UDEV_TRACE) \
	-DCONFIG_KERNEL_BUSYBOX_DEBUG=$(KERNEL_BUSYBOX_DEBUG) \
	-DCONFIG_KERNEL_LINUX_OUTPUT_MIRROR=$(KERNEL_LINUX_OUTPUT_MIRROR) \
	-DCONFIG_KERNEL_XBPS_WAIT_DEBUG=$(KERNEL_XBPS_WAIT_DEBUG) \
	-DCONFIG_KERNEL_PROFILER=$(KERNEL_PROFILER)
LWEXT4_FLAGS := \
	-I./third_party/lwext4/include \
	-DCONFIG_USE_DEFAULT_CFG=1 \
	-DCONFIG_EXT_FEATURE_SET_LVL=4 \
	-DCONFIG_HAVE_OWN_ERRNO=1 \
	-DCONFIG_HAVE_OWN_OFLAGS=1 \
	-DCONFIG_HAVE_OWN_ASSERT=1 \
	-DCONFIG_DEBUG_PRINTF=0 \
	-DCONFIG_DEBUG_ASSERT=0 \
	-DCONFIG_JOURNALING_ENABLE=0 \
	-DCONFIG_BLOCK_DEV_ENABLE_STATS=0 \
	-DCONFIG_BLOCK_DEV_CACHE_SIZE=64 \
	-DCONFIG_EXT4_MAX_BLOCKDEV_NAME=64 \
	-DCONFIG_EXT4_BLOCKDEVS_COUNT=16 \
	-DCONFIG_EXT4_MAX_MP_NAME=128 \
	-DCONFIG_EXT4_MOUNTPOINTS_COUNT=16
C_FLAGS += $(KERNEL_TRACE_DEFS)
CPP_FLAGS += $(KERNEL_TRACE_DEFS)
C_FLAGS += $(LWEXT4_FLAGS)
CPP_FLAGS += $(LWEXT4_FLAGS)

NASM_OBJS := $(patsubst ./%.asm, ./$(BUILD_DIR)/%.o, $(NASM_FILES))
ASM_OBJS := $(patsubst ./%.S, ./$(BUILD_DIR)/%.o, $(ASM_FILES))
C_OBJS := $(patsubst ./%.c, ./$(BUILD_DIR)/%.o, $(C_FILES))
CPP_OBJS := $(patsubst ./%.cpp, ./$(BUILD_DIR)/%.o, $(CPP_FILES))
LWEXT4_OBJS := $(patsubst ./%.c, ./$(BUILD_DIR)/%.o, $(LWEXT4_C_FILES))
KERNEL_OBJS := $(ASM_OBJS) $(C_OBJS) $(CPP_OBJS) $(NASM_OBJS) $(LWEXT4_OBJS)
BUILD_SETTINGS_HEADER := kernel/build_settings.h
BUILD_CONFIG_HEADER := kernel/build_config.h
FMT_FILES := $(C_FILES:%=%.fmt) $(CPP_FILES:%=%.fmt) $(HEADER_FILES:%=%.fmt)
CHECK_FILES := $(C_FILES:%.c=%.tidy) $(CPP_FILES:%.cpp=%.tidy) $(HEADER_FILES:%.h=%.tidy)
CLANGD_TEMPLATE_CONFS := $(wildcard ./**/.clangd_template ./.clangd_template)
PWD := $(shell pwd)

$(KERNEL_OBJS): $(BUILD_SETTINGS_HEADER) $(BUILD_CONFIG_HEADER) Makefile

$(BUILD_CONFIG_HEADER): FORCE
	@tmp="$@.tmp"; \
	{ \
		echo "#pragma once"; \
		echo "#define CONFIG_KERNEL_BUSYBOX_ALIASES $(KERNEL_BUSYBOX_ALIASES)"; \
		echo "#define CONFIG_KERNEL_START_DESKTOP $(KERNEL_START_DESKTOP)"; \
		echo "#define CONFIG_KERNEL_START_COMPONENT_FLUSHER $(KERNEL_START_COMPONENT_FLUSHER)"; \
		echo "#define CONFIG_KERNEL_UDEV_TRACE $(KERNEL_UDEV_TRACE)"; \
		echo "#define CONFIG_KERNEL_BUSYBOX_DEBUG $(KERNEL_BUSYBOX_DEBUG)"; \
		echo "#define CONFIG_KERNEL_LINUX_OUTPUT_MIRROR $(KERNEL_LINUX_OUTPUT_MIRROR)"; \
		echo "#define CONFIG_KERNEL_XBPS_WAIT_DEBUG $(KERNEL_XBPS_WAIT_DEBUG)"; \
		echo "#define CONFIG_KERNEL_PROFILER $(KERNEL_PROFILER)"; \
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

stage.xj380: stardustui.examples.copy stage.xbps-caches
	@test -d "$(VDISK_ROOT)/var/db/xbps" || { echo "missing prepared root: $(VDISK_ROOT)"; echo "run: make prepare"; exit 1; }
	@mkdir -p "$(VDISK_ROOT)/EFI/BOOT" "$(VDISK_ROOT)/apps/system" "$(VDISK_ROOT)/apps/builtin" "$(VDISK_ROOT)/mod" \
		"$(VDISK_ROOT)/system/config" "$(VDISK_ROOT)/system/font" "$(VDISK_ROOT)/system/foxland" \
		"$(VDISK_ROOT)/system/resources/image" "$(VDISK_ROOT)/system/theme" "$(VDISK_ROOT)/etc/xdg/weston"
	@cp -a "$(BOOT_EFI)" "$(VDISK_ROOT)/EFI/BOOT/BOOTX64.efi"
	@cp -a "$(KERNEL_ELF)" "$(VDISK_ROOT)/system/kernel.elf"
	@rm -f $(addprefix $(VDISK_ROOT)/apps/system/,$(SYSTEM_APPS))
	@rm -f $(addprefix $(VDISK_ROOT)/apps/builtin/,$(BUILTIN_APPS) $(STARDUSTUI_BUILTIN_APPS))
	@rm -f $(addprefix $(VDISK_ROOT)/apps/,$(USER_APPS))
	@cp -a $(addprefix $(BUILD_DIR)/,$(SYSTEM_APPS)) "$(VDISK_ROOT)/apps/system/"
	@cp -a $(addprefix $(BUILD_DIR)/,$(BUILTIN_APPS) $(STARDUSTUI_BUILTIN_APPS)) "$(VDISK_ROOT)/apps/builtin/"
	@cp -a $(addprefix $(BUILD_DIR)/,$(USER_APPS)) "$(VDISK_ROOT)/apps/"
	@rm -rf "$(VDISK_ROOT)/apps/user" "$(VDISK_ROOT)/apps/include" "$(VDISK_ROOT)/apps/lib" "$(VDISK_ROOT)/apps/liballoc-x86_64.a"
	@cp -a ./user "$(VDISK_ROOT)/apps/user"
	@cp -a ./include "$(VDISK_ROOT)/apps/include"
	@cp -a ./lib "$(VDISK_ROOT)/apps/lib"
	@cp -a ./liballoc-x86_64.a "$(VDISK_ROOT)/apps/liballoc-x86_64.a"
	@find "$(VDISK_ROOT)/apps/user" -type d -name .git -prune -exec rm -rf {} +
	@cp -a ./font/ttf/XJ380F.ttf "$(VDISK_ROOT)/system/font/XJ380F.ttf"
	@cp -a ./font/ttf/XJ380C.ttf "$(VDISK_ROOT)/system/font/XJ380C.ttf"
	@rm -rf "$(VDISK_ROOT)/system/theme/skyglass" "$(VDISK_ROOT)/system/icon" "$(VDISK_ROOT)/system/resources/image" \
		"$(VDISK_ROOT)/system/resources/svg" "$(VDISK_ROOT)/system/resources/weston"
	@cp -a ./graphics/window/theme/skyglass "$(VDISK_ROOT)/system/theme/skyglass"
	@cp -a ./graphics/peek.png "$(VDISK_ROOT)/system/foxland/peek.png"
	@cp -a ./graphics/logo/. "$(VDISK_ROOT)/system/"
	@cp -a ./graphics/icon "$(VDISK_ROOT)/system/icon"
	@mkdir -p "$(VDISK_ROOT)/system/resources"
	@cp -a ./resources/image "$(VDISK_ROOT)/system/resources/image"
	@cp -a ./resources/svg "$(VDISK_ROOT)/system/resources/svg"
	@cp -a ./resources/weston "$(VDISK_ROOT)/system/resources/weston"
	@cp -a ./resources/start-wayland.sh "$(VDISK_ROOT)/system/resources/start-wayland.sh"
	@cp -a ./resources/weston/weston.ini "$(VDISK_ROOT)/etc/xdg/weston/weston.ini"
	@cp -a ./resources/weston/start-weston.sh "$(VDISK_ROOT)/system/resources/start-wayland.sh"
	@cp -a ./user/fmanager/fm_bg.png "$(VDISK_ROOT)/system/resources/image/fm_bg.png"
	@find "$(VDISK_ROOT)/mod" -maxdepth 1 -type f -name '*.km' -delete
	@cp -a ./$(BUILD_DIR)/*.km "$(VDISK_ROOT)/mod/" 2>/dev/null || true

all: $(VDISK_IMAGE)

.PHONY: FORCE all clean clean.vdisk format check clean.clangd gen.clangd prepare stage.xbps-caches kernel.artifacts user.xapi user.apps kmods stage.xj380 stardustui.examples.copy vdisk vmdk run justrun
FORCE:

clean:
	@rm -rf $(BUILD_DIR) serial.log $(BUILD_CONFIG_HEADER)
	@rm -rf $(VDISK_IMAGE) $(VDISK_ESP_IMAGE) $(VDISK_ROOTFS_IMAGE) $(VMDK_IMAGE) xj380-vdisk
	@echo "Removed build files. "

clean.vdisk:
	@rm -rf $(VDISK_IMAGE) $(VDISK_ESP_IMAGE) $(VDISK_ROOTFS_IMAGE) $(VMDK_IMAGE)
	@echo Removed vdisk image files.

$(XBPS_INSTALL): $(XBPS_STATIC_TARBALL)
	@tar -xJf "$<" -C "$(BF_DIR)" ./usr/bin ./var/db/xbps/keys

prepare: $(XBPS_INSTALL)
	@if [ "$(IMAGE_PACKAGE_MANAGER)" = "xbps" ] && [ -n "$(XBPS_BOOTSTRAP)" ]; then \
		chmod -R u+rwX "$(XBPS_PREPARE_ROOT)" 2>/dev/null || true; \
		rm -rf "$(XBPS_PREPARE_ROOT)"; \
		mkdir -p "$(XBPS_PREPARE_ROOT)"; \
		mkdir -p "$(XBPS_CACHEDIR)" "$(XBPS_PREPARE_ROOT)/var/db/xbps/keys" "$(XBPS_PREPARE_ROOT)/etc/xbps.d" "$(XBPS_PREPARE_ROOT)/usr/share/xbps.d"; \
		printf 'repository=%s\n' "$(XBPS_REPOSITORY)" > "$(XBPS_PREPARE_ROOT)/usr/share/xbps.d/00-repository-main.conf"; \
		printf 'repository=%s\n' "$(XBPS_REPOSITORY)" > "$(XBPS_PREPARE_ROOT)/etc/xbps.d/00-repository-main.conf"; \
		if [ -d "$(BF_DIR)/var/db/xbps/keys" ]; then cp -a "$(BF_DIR)/var/db/xbps/keys/." "$(XBPS_PREPARE_ROOT)/var/db/xbps/keys/"; fi; \
		if [ -d "$(BF_DIR)/usr/var/db/xbps/keys" ]; then cp -a "$(BF_DIR)/usr/var/db/xbps/keys/." "$(XBPS_PREPARE_ROOT)/var/db/xbps/keys/"; fi; \
		if [ -d "$(BF_DIR)/xbps-triggers" ]; then mkdir -p "$(XBPS_PREPARE_ROOT)/usr/libexec"; cp -aL "$(BF_DIR)/xbps-triggers" "$(XBPS_PREPARE_ROOT)/usr/libexec/xbps-triggers"; fi; \
		XBPS_ARCH="$(XBPS_ARCH)" "$(XBPS_INSTALL)" -C /dev/null -c "$(XBPS_CACHEDIR)" -r "$(XBPS_PREPARE_ROOT)" --repository="$(XBPS_REPOSITORY)" -Sy $(XBPS_BOOTSTRAP); \
		ldso="$(XBPS_PREPARE_ROOT)/lib64/ld-linux-x86-64.so.2"; \
		libpath="$(XBPS_PREPARE_ROOT)/usr/lib:$(XBPS_PREPARE_ROOT)/lib"; \
		if [ -x "$(XBPS_PREPARE_ROOT)/usr/bin/gdk-pixbuf-query-loaders" ]; then \
			mkdir -p "$(XBPS_PREPARE_ROOT)/usr/lib/gdk-pixbuf-2.0/2.10.0"; \
			tmp_root="/tmp/xj380-xbps-root.$$$$"; \
			rm -f "$$tmp_root"; \
			ln -s "$$(cd "$(XBPS_PREPARE_ROOT)" && pwd)" "$$tmp_root"; \
			env GDK_PIXBUF_MODULEDIR="$$tmp_root/usr/lib/gdk-pixbuf-2.0/2.10.0/loaders" \
				"$$ldso" --library-path "$$libpath" "$(XBPS_PREPARE_ROOT)/usr/bin/gdk-pixbuf-query-loaders" \
				| sed "s#$$tmp_root##g" > "$(XBPS_PREPARE_ROOT)/usr/lib/gdk-pixbuf-2.0/2.10.0/loaders.cache"; \
			rm -f "$$tmp_root"; \
		fi; \
		if [ -x "$(XBPS_PREPARE_ROOT)/usr/bin/update-mime-database" ] && [ -d "$(XBPS_PREPARE_ROOT)/usr/share/mime" ]; then \
			"$$ldso" --library-path "$$libpath" "$(XBPS_PREPARE_ROOT)/usr/bin/update-mime-database" -n "$(XBPS_PREPARE_ROOT)/usr/share/mime"; \
		fi; \
		if [ -x "$(XBPS_PREPARE_ROOT)/usr/bin/glib-compile-schemas" ] && [ -d "$(XBPS_PREPARE_ROOT)/usr/share/glib-2.0/schemas" ]; then \
			"$$ldso" --library-path "$$libpath" "$(XBPS_PREPARE_ROOT)/usr/bin/glib-compile-schemas" "$(XBPS_PREPARE_ROOT)/usr/share/glib-2.0/schemas"; \
		fi; \
		if [ -x "$(XBPS_PREPARE_ROOT)/usr/bin/gtk-update-icon-cache" ] && [ -d "$(XBPS_PREPARE_ROOT)/usr/share/icons" ]; then \
			for theme in Adwaita gnome hicolor; do \
				if [ -d "$(XBPS_PREPARE_ROOT)/usr/share/icons/$$theme" ]; then \
					"$$ldso" --library-path "$$libpath" "$(XBPS_PREPARE_ROOT)/usr/bin/gtk-update-icon-cache" -q -f -t "$(XBPS_PREPARE_ROOT)/usr/share/icons/$$theme" || true; \
				fi; \
			done; \
		fi; \
		if [ -x "$(XBPS_PREPARE_ROOT)/usr/bin/localedef" ]; then \
			mkdir -p "$(XBPS_PREPARE_ROOT)/usr/lib64/locale" "$(XBPS_PREPARE_ROOT)/usr/lib/locale"; \
			env I18NPATH="$$(cd "$(XBPS_PREPARE_ROOT)" && pwd)/usr/share/i18n" \
				"$$ldso" --library-path "$$libpath" "$(XBPS_PREPARE_ROOT)/usr/bin/localedef" \
				--prefix "$$(cd "$(XBPS_PREPARE_ROOT)" && pwd)" -i C -f UTF-8 C.UTF-8 || true; \
			env I18NPATH="$$(cd "$(XBPS_PREPARE_ROOT)" && pwd)/usr/share/i18n" \
				"$$ldso" --library-path "$$libpath" "$(XBPS_PREPARE_ROOT)/usr/bin/localedef" \
				--prefix "$$(cd "$(XBPS_PREPARE_ROOT)" && pwd)" -i en_US -f UTF-8 en_US.UTF-8 || true; \
		fi; \
	else \
		echo "Nothing to prepare for IMAGE_PACKAGE_MANAGER=$(IMAGE_PACKAGE_MANAGER)"; \
	fi

stage.xbps-caches:
	@if [ "$(IMAGE_PACKAGE_MANAGER)" = "xbps" ] && [ -d "$(XBPS_PREPARE_ROOT)" ]; then \
		ldso="$(XBPS_PREPARE_ROOT)/lib64/ld-linux-x86-64.so.2"; \
		libpath="$(XBPS_PREPARE_ROOT)/usr/lib:$(XBPS_PREPARE_ROOT)/lib"; \
		if [ ! -x "$$ldso" ]; then exit 0; fi; \
		if [ -x "$(XBPS_PREPARE_ROOT)/usr/bin/gdk-pixbuf-query-loaders" ]; then \
			mkdir -p "$(XBPS_PREPARE_ROOT)/usr/lib/gdk-pixbuf-2.0/2.10.0"; \
			tmp_root="/tmp/xj380-xbps-root.$$$$"; \
			rm -f "$$tmp_root"; \
			ln -s "$$(cd "$(XBPS_PREPARE_ROOT)" && pwd)" "$$tmp_root"; \
			env GDK_PIXBUF_MODULEDIR="$$tmp_root/usr/lib/gdk-pixbuf-2.0/2.10.0/loaders" \
				"$$ldso" --library-path "$$libpath" "$(XBPS_PREPARE_ROOT)/usr/bin/gdk-pixbuf-query-loaders" \
				| sed "s#$$tmp_root##g" > "$(XBPS_PREPARE_ROOT)/usr/lib/gdk-pixbuf-2.0/2.10.0/loaders.cache"; \
			rm -f "$$tmp_root"; \
		fi; \
		if [ -x "$(XBPS_PREPARE_ROOT)/usr/bin/update-mime-database" ] && [ -d "$(XBPS_PREPARE_ROOT)/usr/share/mime" ]; then \
			"$$ldso" --library-path "$$libpath" "$(XBPS_PREPARE_ROOT)/usr/bin/update-mime-database" -n "$(XBPS_PREPARE_ROOT)/usr/share/mime"; \
		fi; \
		if [ -x "$(XBPS_PREPARE_ROOT)/usr/bin/glib-compile-schemas" ] && [ -d "$(XBPS_PREPARE_ROOT)/usr/share/glib-2.0/schemas" ]; then \
			"$$ldso" --library-path "$$libpath" "$(XBPS_PREPARE_ROOT)/usr/bin/glib-compile-schemas" "$(XBPS_PREPARE_ROOT)/usr/share/glib-2.0/schemas"; \
		fi; \
		if [ -x "$(XBPS_PREPARE_ROOT)/usr/bin/gtk-update-icon-cache" ] && [ -d "$(XBPS_PREPARE_ROOT)/usr/share/icons" ]; then \
			for theme in Adwaita gnome hicolor; do \
				if [ -d "$(XBPS_PREPARE_ROOT)/usr/share/icons/$$theme" ]; then \
					"$$ldso" --library-path "$$libpath" "$(XBPS_PREPARE_ROOT)/usr/bin/gtk-update-icon-cache" -q -f -t "$(XBPS_PREPARE_ROOT)/usr/share/icons/$$theme" || true; \
				fi; \
			done; \
		fi; \
		if [ -x "$(XBPS_PREPARE_ROOT)/usr/bin/localedef" ]; then \
			mkdir -p "$(XBPS_PREPARE_ROOT)/usr/lib64/locale" "$(XBPS_PREPARE_ROOT)/usr/lib/locale"; \
			env I18NPATH="$$(cd "$(XBPS_PREPARE_ROOT)" && pwd)/usr/share/i18n" \
				"$$ldso" --library-path "$$libpath" "$(XBPS_PREPARE_ROOT)/usr/bin/localedef" \
				--prefix "$$(cd "$(XBPS_PREPARE_ROOT)" && pwd)" -i C -f UTF-8 C.UTF-8 || true; \
			env I18NPATH="$$(cd "$(XBPS_PREPARE_ROOT)" && pwd)/usr/share/i18n" \
				"$$ldso" --library-path "$$libpath" "$(XBPS_PREPARE_ROOT)/usr/bin/localedef" \
				--prefix "$$(cd "$(XBPS_PREPARE_ROOT)" && pwd)" -i en_US -f UTF-8 en_US.UTF-8 || true; \
		fi; \
	fi

format: $(FMT_FILES)
	@echo "Formatted all the code ..."

check: $(CHECK_FILES)
	@echo "Checked all the code ..."

clean.clangd:
	@rm -f $(CLANGD_TEMPLATE_CONFS:%.clangd_template=%.clangd)

gen.clangd: clean.clangd $(CLANGD_TEMPLATE_CONFS:%.clangd_template=%.clangd)
	@echo "\033[1;32m[Done]\033[0m .clangd configurations was generated.\n"

$(VDISK_ROOTFS_IMAGE): stage.xj380
	@rm -f "$(VDISK_ROOTFS_IMAGE)"
	@dd if=/dev/zero of="$(VDISK_ROOTFS_IMAGE)" bs=1M count=0 seek=$(VDISK_ROOTFS_MIB)
	@$(VDISK_SUDO) mkfs.ext4 -b $(VDISK_EXT_BLOCK_SIZE) -I $(VDISK_EXT_INODE_SIZE) -O "$(VDISK_EXT_FEATURES)" \
		-E lazy_itable_init=0,lazy_journal_init=0 -F -q -d "$(VDISK_ROOT)" "$(VDISK_ROOTFS_IMAGE)"

$(VDISK_IMAGE): $(VDISK_ROOTFS_IMAGE)
	@rm -f "$(VDISK_IMAGE)" "$(VDISK_ESP_IMAGE)"
	@dd if=/dev/zero of="$(VDISK_IMAGE)" bs=1M count=0 seek=$(VDISK_SIZE_MIB)
	@$(VDISK_SUDO) sgdisk --new=1:$(VDISK_OFFSET_SECTORS):$(VDISK_ESP_END_SECTORS) --typecode=1:ef00 --change-name=1:XJ380ESP \
		--new=2:$(VDISK_ROOTFS_START_SECTORS):0 --typecode=2:8300 --change-name=2:XJ380ROOT "$(VDISK_IMAGE)"
	@dd if=/dev/zero of="$(VDISK_ESP_IMAGE)" bs=1M count=0 seek=$(VDISK_ESP_MIB)
	@$(VDISK_SUDO) mkfs.vfat -F 32 -S 512 "$(VDISK_ESP_IMAGE)"
	@mmd -i "$(VDISK_ESP_IMAGE)" ::/EFI ::/EFI/BOOT ::/system
	@mcopy -o -i "$(VDISK_ESP_IMAGE)" "$(VDISK_ROOT)/EFI/BOOT/BOOTX64.efi" ::/EFI/BOOT
	@mcopy -o -i "$(VDISK_ESP_IMAGE)" "$(VDISK_ROOT)/system/kernel.elf" ::/system
	@dd if="$(VDISK_ESP_IMAGE)" of="$(VDISK_IMAGE)" bs=512 seek=$(VDISK_OFFSET_SECTORS) conv=notrunc,sparse status=none
	@dd if="$(VDISK_ROOTFS_IMAGE)" of="$(VDISK_IMAGE)" bs=1M seek=$(VDISK_ROOTFS_START_MIB) conv=notrunc,sparse status=none

vdisk: $(VDISK_IMAGE)

vmdk: vdisk
	@rm -f $(VMDK_IMAGE)
	@qemu-img convert -O vmdk $(VDISK_IMAGE) $(VMDK_IMAGE)

run: vdisk
	@$(QEMU_CMD)

justrun:
	@$(QEMU_CMD)
