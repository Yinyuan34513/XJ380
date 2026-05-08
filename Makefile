NASM_FLAGS = -f elf64 -DX86_64_TARGET -DUEFI
ASM_FLAGS = -O0 -g -mno-red-zone -mstackrealign -nostdlib -ffreestanding -fno-builtin -m64 -fno-stack-protector -fno-exceptions -mno-80387
C_FLAGS = -O0 -g -mno-red-zone -mstackrealign -nostdlib -ffreestanding -fno-builtin -m64 -fno-stack-protector -fno-exceptions -std=c11 -fshort-wchar -nostdinc -mno-80387 -I./include 
CPP_FLAGS = -O0 -g -mno-red-zone -mstackrealign -nostdlib -ffreestanding -fno-builtin -m64 -fno-stack-protector -fno-exceptions -fno-rtti -std=gnu++17 -fshort-wchar -nostdinc -fno-use-cxa-atexit -fno-threadsafe-statics -mno-80387 -Wno-int-to-pointer-cast -Wno-macro-redefined -Wno-c11-extensions -Wno-c99-extensions -Wno-gnu-statement-expression-from-macro-expansion -I./include -Wno-writable-strings -Wno-c++11-narrowing
BOOT_C_FLAGS = -g -I ./boot/include -Wextra -e efi_main -nostdinc -nostdlib -fno-builtin -Wl,--subsystem,10 -fshort-wchar

DEBUG ?= 1
SMP ?= 4
SUDO ?= 1
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
VDISK_ROOT ?= /tmp/xj380-vdisk-$$(id -u)/XJ380
KMOD_DIR := kmod
STARDUSTUI_EXAMPLE_DIRS := \
	frameworks/StardustUI/examples/helloworld \
	frameworks/StardustUI/examples/layout

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
OPKG_CONF_GEN := $(BUILD_DIR)/tools/xj380_opkg_conf_gen
OPKG_LD_LIBRARY_PATH ?=
OPKG_STAGE_DEPS := tools/stage_elf_deps.sh
OPKG_FEEDS ?= openwrt_base http://mirrors.tuna.tsinghua.edu.cn/openwrt/releases/24.10.2/packages/x86_64/base openwrt_packages http://mirrors.tuna.tsinghua.edu.cn/openwrt/releases/24.10.2/packages/x86_64/packages
APP_BASH := resources/apps/bash
APP_BUSYBOX := resources/apps/busybox
APP_FASTFETCH := resources/apps/fastfetch
APP_OPKG := resources/apps/opkg
FMT_FILES := $(C_FILES:%=%.fmt) $(CPP_FILES:%=%.fmt) $(HEADER_FILES:%=%.fmt)
CHECK_FILES := $(C_FILES:%.c=%.tidy) $(CPP_FILES:%.cpp=%.tidy) $(HEADER_FILES:%.h=%.tidy)
CLANGD_TEMPLATE_CONFS := $(wildcard ./**/.clangd_template ./.clangd_template)
PWD := $(shell pwd)

$(KERNEL_OBJS): $(BUILD_SETTINGS_HEADER)

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

$(OPKG_CONF_GEN): tools/xj380_opkg_conf_gen.c
	@mkdir -p $(@D)
	@$(HOST_CC) -O2 -Wall -Wextra $< -o $@
	@echo "HOSTCC $< -> $@"

.PHONY: clean format check gen.clangd stardustui.examples.copy

clean:
# 事实上直接删out/最简单了
	@rm -rf $(BUILD_DIR) ./XJ380.img serial.log
	@echo Removed build files.

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

vdisk: all $(OPKG_CONF_GEN)
	@rm -rf XJ380.img
	@chmod -R u+rwX $(VDISK_ROOT) 2>/dev/null || true
	@rm -rf $(VDISK_ROOT) || { \
		echo "Cannot remove $(VDISK_ROOT); fix ownership with:"; \
		echo "  sudo chown -R $$(id -u):$$(id -g) $(VDISK_ROOT)"; \
		exit 1; \
	}

	@dd if=/dev/zero of=XJ380.img bs=1024M count=1
# You must execute the following command with sudo privileges, that's not good...
	@sudo sgdisk XJ380.img -n 1:2048 -t 1:ef00

	@sudo mkfs.vfat -F 32 --offset 2048 -S 512 XJ380.img $$((1024*1024-2048*512/1024))

	@echo "a"

	@mkdir -p $(VDISK_ROOT)/EFI/BOOT
	@mkdir -p $(VDISK_ROOT)/system
	@mkdir -p $(VDISK_ROOT)/system/font
	@mkdir -p $(VDISK_ROOT)/system/theme/skyglass
	@mkdir -p $(VDISK_ROOT)/system/foxland
	@mkdir -p $(VDISK_ROOT)/system/icon
	@mkdir -p $(VDISK_ROOT)/apps
	@mkdir -p $(VDISK_ROOT)/lib64
	@mkdir -p $(VDISK_ROOT)/lib/$(HOST_GCC_TRIPLE)
	@mkdir -p $(VDISK_ROOT)/usr/bin
	@mkdir -p $(VDISK_ROOT)/usr/include
	@mkdir -p $(VDISK_ROOT)/usr/lib/gcc/$(HOST_GCC_TRIPLE)
	@mkdir -p $(VDISK_ROOT)/usr/libexec/gcc/$(HOST_GCC_TRIPLE)
	@mkdir -p $(VDISK_ROOT)/usr/lib/$(HOST_GCC_TRIPLE)
	@mkdir -p $(VDISK_ROOT)/system/resources/image
	@mkdir -p $(VDISK_ROOT)/mod
	@mkdir -p $(VDISK_ROOT)/etc/opkg
	@mkdir -p $(VDISK_ROOT)/etc/fastfetch
	@mkdir -p $(VDISK_ROOT)/etc
	@mkdir -p $(VDISK_ROOT)/etc/ssl/certs
	@mkdir -p $(VDISK_ROOT)/usr/lib/opkg/info
	@mkdir -p $(VDISK_ROOT)/usr/lib/opkg/lists
	@mkdir -p $(VDISK_ROOT)/tmp
	@mkdir -p $(VDISK_ROOT)/var
	@mkdir -p $(VDISK_ROOT)/var/lock
	@printf "nameserver 223.5.5.5\n" > $(VDISK_ROOT)/etc/resolv.conf
	@cp -a resources/etc/os-release $(VDISK_ROOT)/etc/os-release
	@cp -a resources/etc/fastfetch/. $(VDISK_ROOT)/etc/fastfetch/
	@if [ -f /etc/ssl/certs/ca-certificates.crt ]; then cp -a /etc/ssl/certs/ca-certificates.crt $(VDISK_ROOT)/etc/ssl/certs/ca-certificates.crt; fi
	@touch $(VDISK_ROOT)/usr/lib/opkg/lock
	@cp -a resources/opkg/status $(VDISK_ROOT)/usr/lib/opkg/status
	@if [ -d resources/opkg/info ]; then cp -a resources/opkg/info/. $(VDISK_ROOT)/usr/lib/opkg/info/; fi
	@$(OPKG_CONF_GEN) $(VDISK_ROOT) $(OPKG_FEEDS)
	@mkdir -p $(VDISK_ROOT)/system/config
	@mkdir -p $(VDISK_ROOT)/apps/builtin
	@mkdir -p $(VDISK_ROOT)/apps/system
	@mkdir -p $(VDISK_ROOT)/apps/thirdparty

	@cp -a ./user/fmanager/fm_bg.png $(VDISK_ROOT)/system/resources/image

	@cp -a $(BUILD_DIR)/BOOTX64.efi $(VDISK_ROOT)/EFI/BOOT
	@cp -a $(BUILD_DIR)/kernel.elf $(VDISK_ROOT)/system

	@cp -a $(BUILD_DIR)/shell.elf    $(VDISK_ROOT)/apps/system
	@cp -a $(BUILD_DIR)/fmanager.elf $(VDISK_ROOT)/apps/system
	@cp -a $(BUILD_DIR)/taskmgr.elf  $(VDISK_ROOT)/apps/system
	@cp -a $(BUILD_DIR)/ctrlmenu.elf $(VDISK_ROOT)/apps/system

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
	@cp -a /lib/x86_64-linux-gnu/ld-linux-x86-64.so.2 $(VDISK_ROOT)/lib64/ld-linux-x86-64.so.2

	@cp -a resources/musl/ld-musl-x86_64.so.1 $(VDISK_ROOT)/lib/ld-musl-x86_64.so.1
	@cp -a resources/musl/libc.so $(VDISK_ROOT)/lib/libc.so
	@cp -a resources/musl/libc.musl-x86_64.so.1 $(VDISK_ROOT)/lib/libc.musl-x86_64.so.1
	@cp -a resources/musl/libgcc_s.so.1 $(VDISK_ROOT)/lib/libgcc_s.so.1

	@cp -a /lib/$(HOST_GCC_TRIPLE)/libc.so.6 $(VDISK_ROOT)/lib64/libc.so.6
	@cp -a $(APP_BASH) $(VDISK_ROOT)/apps/bash
	@cp -a $(APP_BUSYBOX) $(VDISK_ROOT)/apps/busybox
	@cp -a $(APP_FASTFETCH) $(VDISK_ROOT)/apps/fastfetch
	@[ -n "$(HOST_GCC_REAL)" ] && [ -n "$(HOST_GCC_TRIPLE)" ] && [ -n "$(HOST_GCC_VERSION)" ]
	@cp ./1.c $(VDISK_ROOT)/apps/1.c
	@cp -aL $(HOST_GCC_REAL) $(VDISK_ROOT)/usr/bin/$(HOST_GCC_REAL_NAME)
	@cp -aL $(HOST_GCC_REAL) $(VDISK_ROOT)/usr/bin/gcc-$(HOST_GCC_VERSION)
	@cp -aL $(HOST_GCC_REAL) $(VDISK_ROOT)/usr/bin/gcc
	@cp -aL $(HOST_GCC_REAL) $(VDISK_ROOT)/usr/bin/cc
	@cp -aL $(HOST_GCC_AS) $(VDISK_ROOT)/usr/bin/$(HOST_GCC_AS_NAME)
	@cp -aL $(HOST_GCC_AS) $(VDISK_ROOT)/usr/bin/as
	@cp -aL $(HOST_GCC_LD) $(VDISK_ROOT)/usr/bin/$(HOST_GCC_LD_NAME)
	@cp -aL $(HOST_GCC_LD) $(VDISK_ROOT)/usr/bin/$(HOST_GCC_TRIPLE)-ld
	@cp -aL $(HOST_GCC_LD) $(VDISK_ROOT)/usr/bin/ld
	@cp -a $(HOST_GCC_LIBEXECDIR) $(VDISK_ROOT)/usr/libexec/gcc/$(HOST_GCC_TRIPLE)/
	@cp -a $(HOST_GCC_LIBDIR) $(VDISK_ROOT)/usr/lib/gcc/$(HOST_GCC_TRIPLE)/
	@find /usr/include -mindepth 1 | sort | while IFS= read -r src; do \
		rel=$${src#/usr/include/}; \
		dst="$(VDISK_ROOT)/usr/include/$$rel"; \
		if [ -d "$$src" ] && [ ! -L "$$src" ]; then \
			mkdir -p "$$dst"; \
		elif [ -e "$$dst" ] || [ -L "$$dst" ]; then \
			:; \
		else \
			mkdir -p "$$(dirname "$$dst")"; \
			cp -RL "$$src" "$$dst"; \
		fi; \
	done
	@for f in /usr/lib/$(HOST_GCC_TRIPLE)/crt*.o /usr/lib/$(HOST_GCC_TRIPLE)/Scrt*.o /usr/lib/$(HOST_GCC_TRIPLE)/Mcrt*.o /usr/lib/$(HOST_GCC_TRIPLE)/libc.so /usr/lib/$(HOST_GCC_TRIPLE)/libc_nonshared.a /usr/lib/$(HOST_GCC_TRIPLE)/libm.so /usr/lib/$(HOST_GCC_TRIPLE)/libmvec_nonshared.a /usr/lib/$(HOST_GCC_TRIPLE)/libpthread_nonshared.a; do [ ! -e "$$f" ] || cp -a "$$f" $(VDISK_ROOT)/usr/lib/$(HOST_GCC_TRIPLE)/; done
	@$(OPKG_STAGE_DEPS) $(VDISK_ROOT) $(HOST_GCC_REAL) $(HOST_GCC_AS) $(HOST_GCC_LD) $(HOST_GCC_LIBEXECDIR)/cc1 $(HOST_GCC_LIBEXECDIR)/collect2
	@for f in /lib/$(HOST_GCC_TRIPLE)/libbfd-*.so /lib/$(HOST_GCC_TRIPLE)/libctf.so* /lib/$(HOST_GCC_TRIPLE)/libjansson.so* /lib/$(HOST_GCC_TRIPLE)/libsframe.so* /lib/$(HOST_GCC_TRIPLE)/libz.so* /lib/$(HOST_GCC_TRIPLE)/libzstd.so* /lib/$(HOST_GCC_TRIPLE)/libc.so* /lib/$(HOST_GCC_TRIPLE)/libm.so* /lib/$(HOST_GCC_TRIPLE)/libgcc_s.so*; do [ ! -e "$$f" ] || cp -a "$$f" $(VDISK_ROOT)/lib/$(HOST_GCC_TRIPLE)/; done
	@$(OPKG_STAGE_DEPS) $(VDISK_ROOT) $(APP_FASTFETCH)
	@cp -a $(APP_OPKG) $(VDISK_ROOT)/apps/opkg
	@LD_LIBRARY_PATH="$(OPKG_LD_LIBRARY_PATH)" $(OPKG_STAGE_DEPS) $(VDISK_ROOT) $(VDISK_ROOT)/apps/opkg

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

# temp test (remember to delete)
	@mkdir -p $(VDISK_ROOT)/users/GuoqiFish/desktop
	@cp -a ./test.txt $(VDISK_ROOT)/users/GuoqiFish/desktop
	@find $(VDISK_ROOT) -xtype l -delete
	@find $(VDISK_ROOT) -type l -exec sh -c 'for lnk do tmp=$${lnk}.xj380-copy; cp -aL "$$lnk" "$$tmp" 2>/dev/null && rm -f "$$lnk" && mv "$$tmp" "$$lnk" || { rm -rf "$$tmp"; rm -f "$$lnk"; }; done' sh {} +
	@mcopy -o -s -i XJ380.img@@$$((2048*512)) $(VDISK_ROOT)/* ::

	@chmod -R u+rwX $(VDISK_ROOT) 2>/dev/null || true
	@rm -rf $(VDISK_ROOT) || { \
		echo "Cannot remove $(VDISK_ROOT); fix ownership with:"; \
		echo "  sudo chown -R $$(id -u):$$(id -g) $(VDISK_ROOT)"; \
		exit 1; \
	}
	

vmdk: vdisk
	@rm -rf XJ380.vmdk
	@qemu-img convert -O vmdk XJ380.img XJ380.vmdk

run: vdisk
	@$(QEMU_CMD)

justrun:
	@$(QEMU_CMD)
