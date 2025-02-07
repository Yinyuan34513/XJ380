ASM_FLAGS = -O0 -Wall -g -nostdlib -ffreestanding -fno-builtin -mno-sse -mno-mmx -mno-sse2 -m64 -fno-stack-protector -fno-exceptions
C_FLAGS = -O0 -Wall -g -nostdlib -ffreestanding -fno-builtin -mno-sse -mno-mmx -mno-sse2 -m64 -fno-stack-protector -fno-exceptions -std=c99 -fshort-wchar -nostdinc
CPP_FLAGS = -O0 -Wall -g -nostdlib -ffreestanding -fno-builtin -mno-sse -mno-mmx -mno-sse2 -m64 -fno-stack-protector -fno-exceptions -fno-rtti -std=c++17 -fshort-wchar -nostdinc -fno-use-cxa-atexit -fno-threadsafe-statics

DEBUG ?= 0
SMP ?= 4

ifeq ($(DEBUG), 1)
DEBUG_FLAG = -s -S
endif

QEMU_CMD = qemu-system-x86_64 -M q35 -drive if=pflash,format=raw,file=OVMF_CODE.fd,readonly=on -m 8192 -smp $(SMP) -net none -drive if=none,id=harddisk,file=XJ380.img,index=0,format=raw -device ahci,id=ahci -device nvme,serial=NVMEDISK,drive=harddisk -serial file:serial.log $(DEBUG_FLAG)

COMPILER_PREFIX = x86_64-linux-gnu-

# FILES
ASM_FILES := $(shell find ./kernel ./graphics ./driver ./lib ./font -name "*.S")
C_FILES := $(shell find ./kernel ./graphics ./driver ./lib ./font -name "*.c")
CPP_FILES := $(shell find ./kernel ./graphics ./driver ./lib ./font -name "*.cpp")

export CC = $(COMPILER_PREFIX)gcc
export CPP = $(CC)
export LD = $(COMPILER_PREFIX)ld
export NM = $(COMPILER_PREFIX)nm
export OBJD = $(COMPILER_PREFIX)objdump

ASM_OBJS := $(sort $(patsubst ./%.S, ./out/%.o, $(ASM_FILES)))
C_OBJS := $(sort $(patsubst ./%.c, ./out/%.o, $(C_FILES)))
CPP_OBJS := $(sort $(patsubst ./%.cpp, ./out/%.o, $(CPP_FILES)))

out/%.o: %.S
	@mkdir -p $(@D)
	@$(CC) -fno-pic -fno-pie -mcmodel=kernel $(ASM_FLAGS) -c $< -o $@
	@echo "AS $< -> $@"

out/%.o: %.c
	@mkdir -p $(@D)
	@$(CC) -fno-pic -fno-pie -mcmodel=kernel $(C_FLAGS) -c $< -o $@
	@echo "CC $< -> $@"

out/%.o: %.cpp
	@mkdir -p $(@D)
	@$(CPP) -fno-pic -fno-pie -mcmodel=kernel $(CPP_FLAGS) -c $< -o $@
	@echo "CPP $< -> $@"

all: $(ASM_OBJS) $(C_OBJS) $(CPP_OBJS)
# 针对BOOT和KERNEL进行特殊处理
	@x86_64-w64-mingw32-gcc -Wall -Wextra -e efi_main -nostdinc -nostdlib -fno-builtin -Wl,--subsystem,10 -fshort-wchar -o out/BOOTX64.efi boot/bootx64.c

	@$(LD) -z muldefs -e KernelMain -T linker.ld --static -o ./out/kernel.elf $(shell find ./out -name "*.o") ./font/hankaku.o
	@gcc kallsyms/kallsyms.c -o ./out/kallsyms
	@$(NM) -n ./out/kernel.elf | ./out/kallsyms >./out/kallsyms.S
	@$(CC) $(C_FLAGS) -c ./out/kallsyms.S -o ./out/kallsyms.o
	@$(LD) -z muldefs -e KernelMain -T linker.ld --static -o ./out/kernel.elf $(shell find ./out -name "*.o") ./font/hankaku.o ./out/kallsyms.o
	@$(OBJD) -x -d -s ./out/kernel.elf > ./out/kernel.txt

.PHONY: clean
clean:
	@rm -rf ./out ./nul ./XJ380.img

vdisk: all
	@rm -rf XJ380.img

	@dd if=/dev/zero bs=1M count=0 seek=64 of=XJ380.img
	@sgdisk XJ380.img -n 1:2048 -t 1:ef00

	@mformat -s 32 -c 1 -v "NO NAME" -i XJ380.img@@1M
	@mmd -i XJ380.img@@1M ::/EFI ::/EFI/BOOT ::/usr
	@mcopy -i XJ380.img@@1M out/kernel.elf ::/
	@mcopy -i XJ380.img@@1M out/BOOTX64.efi ::/EFI/BOOT

	@$(MAKE) -C user build_xlibc
	@$(MAKE) -C user

	@mcopy -i XJ380.img@@1M out/init.elf ::/usr
	@mcopy -i XJ380.img@@1M out/shell.elf ::/usr

run: vdisk
	@$(QEMU_CMD)
