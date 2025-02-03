ASM_FLAGS = -O0 -Wall -g -nostdlib -m64 -fno-stack-protector -ffreestanding -fno-exceptions -mcmodel=large
C_FLAGS = -O0 -Wall -g -ffreestanding -m64 -fno-stack-protector -fno-exceptions -fno-rtti -std=c11 -fshort-wchar -nostdinc -mcmodel=large
CPP_FLAGS = -O0 -Wall -g -ffreestanding -m64 -fno-stack-protector -fno-exceptions -fno-rtti -std=c++17 -fshort-wchar -nostdinc -fno-use-cxa-atexit -fno-threadsafe-statics -mcmodel=large

CPP_VERSION ?= 14

DEBUG ?= 0

ifeq ($(DEBUG), 1)
DEBUG_FLAG = -s -S
endif

QEMU_CMD = qemu-system-x86_64 -M q35 -drive if=pflash,format=raw,file=OVMF_CODE.fd,readonly=on -m 4096 -smp 4 -net none -drive if=none,id=harddisk,file=fat:rw:esp,index=0,format=vvfat -device nvme,serial=NVMEDISK,drive=harddisk -serial file:serial.log $(DEBUG_FLAG)

ifneq ($(OS), Windows_NT)
COMPILER_PREFIX = x86_64-linux-gnu-
RMDIR_CMD = rm -rf
MKDIR_CMD = mkdir -p
COPY_CMD = cp -r
# FILES
ASM_FILES := $(shell find ./kernel ./graphics ./driver ./lib ./font -name "*.S")
C_FILES := $(shell find ./kernel ./graphics ./driver ./lib ./font -name "*.c")
CPP_FILES := $(shell find ./kernel ./graphics ./driver ./lib ./font -name "*.cpp")

CPP = $(COMPILER_PREFIX)g++-$(CPP_VERSION)
else
COMPILER_PREFIX =  powershell -Command x86_64-elf-
RMDIR_CMD = powershell -Command Remove-Item -Force
MKDIR_CMD = powershell -Command New-Item -ItemType Directory -Force
COPY_CMD = powershell -Command Copy-Item
# FILES
ASM_FILES := $(addprefix ./kernel/, $(patsubst \, /, $(shell powershell -Command "Get-ChildItem -Name -Filter *.S -Recurse -Path kernel")))
ASM_FILES += $(addprefix ./graphics/, $(patsubst \, /, $(shell powershell -Command "Get-ChildItem -Name -Filter *.S -Recurse -Path graphics")))
ASM_FILES += $(addprefix ./driver/, $(patsubst \, /, $(shell powershell -Command "Get-ChildItem -Name -Filter *.S -Recurse -Path driver")))
ASM_FILES += $(addprefix ./font/, $(patsubst \, /, $(shell powershell -Command "Get-ChildItem -Name -Filter *.S -Recurse -Path font")))
ASM_FILES += $(addprefix ./lib/, $(patsubst \, /, $(shell powershell -Command "Get-ChildItem -Name -Filter *.S -Recurse -Path lib")))

C_FILES := $(addprefix ./kernel/, $(patsubst \, /, $(shell powershell -Command "Get-ChildItem -Name -Filter *.c -Recurse -Path kernel")))
C_FILES += $(addprefix ./graphics/, $(patsubst \, /, $(shell powershell -Command "Get-ChildItem -Name -Filter *.c -Recurse -Path graphics")))
C_FILES += $(addprefix ./driver/, $(patsubst \, /, $(shell powershell -Command "Get-ChildItem -Name -Filter *.c -Recurse -Path driver")))
C_FILES += $(addprefix ./font/, $(patsubst \, /, $(shell powershell -Command "Get-ChildItem -Name -Filter *.c -Recurse -Path font")))
C_FILES += $(addprefix ./lib/, $(patsubst \, /, $(shell powershell -Command "Get-ChildItem -Name -Filter *.c -Recurse -Path lib")))

CPP_FILES := $(addprefix ./kernel/, $(patsubst \, /, $(shell powershell -Command "Get-ChildItem -Name -Filter *.cpp -Recurse -Path kernel")))
CPP_FILES += $(addprefix ./graphics/, $(patsubst \, /, $(shell powershell -Command "Get-ChildItem -Name -Filter *.cpp -Recurse -Path graphics")))
CPP_FILES += $(addprefix ./driver/, $(patsubst \, /, $(shell powershell -Command "Get-ChildItem -Name -Filter *.cpp -Recurse -Path driver")))
CPP_FILES += $(addprefix ./font/, $(patsubst \, /, $(shell powershell -Command "Get-ChildItem -Name -Filter *.cpp -Recurse -Path font")))
CPP_FILES += $(addprefix ./lib/, $(patsubst \, /, $(shell powershell -Command "Get-ChildItem -Name -Filter *.cpp -Recurse -Path lib")))

CPP = $(COMPILER_PREFIX)g++
endif

CC = $(COMPILER_PREFIX)gcc
LD = $(COMPILER_PREFIX)ld
NM = $(COMPILER_PREFIX)nm
OBJD = $(COMPILER_PREFIX)objdump

ASM_OBJS := $(sort $(patsubst ./%.S, ./out/%.o, $(ASM_FILES)))
C_OBJS := $(sort $(patsubst ./%.c, ./out/%.o, $(C_FILES)))
CPP_OBJS := $(sort $(patsubst ./%.cpp, ./out/%.o, $(CPP_FILES)))

out/%.o: %.S
	@$(MKDIR_CMD) $(@D) > nul
	@$(CC) $(ASM_FLAGS) -c $< -o $@
	@echo "AS $< -> $@"

out/%.o: %.c
	@$(MKDIR_CMD) $(@D) > nul
	@$(CC) $(C_FLAGS) -c $< -o $@
	@echo "CC $< -> $@"

out/%.o: %.cpp
	@$(MKDIR_CMD) $(@D) > nul
	@$(CPP) $(CPP_FLAGS) -c $< -o $@
	@echo "CPP $< -> $@"

all: $(ASM_OBJS) $(C_OBJS) $(CPP_OBJS)
	@$(MKDIR_CMD) ./esp/EFI/BOOT > nul

# 针对BOOT和KERNEL进行特殊处理
	@x86_64-w64-mingw32-gcc -Wall -Wextra -e efi_main -nostdinc -nostdlib -fno-builtin -Wl,--subsystem,10 -fshort-wchar -o out/BOOTX64.efi boot/bootx64.c

ifneq ($(OS), Windows_NT)
	@$(LD) -z muldefs -e KernelMain -T linker.ld --static -o ./out/kernel.elf $(shell find ./out -name "*.o") ./font/hankaku.o
	@gcc kallsyms/kallsyms.c -o ./out/kallsyms
	@$(NM) -n ./out/kernel.elf | ./out/kallsyms >./out/kallsyms.S
	@$(CC) $(C_FLAGS) -c ./out/kallsyms.S -o ./out/kallsyms.o
	@$(LD) -z muldefs -e KernelMain -T linker.ld --static -o ./out/kernel.elf $(shell find ./out -name "*.o") ./font/hankaku.o ./out/kallsyms.o
	@$(OBJD) -x -d -s ./out/kernel.elf >./out/kernel.txt
else
	@$(LD) -z muldefs -e KernelMain -T linker.ld --static -o ./out/kernel.elf $(addprefix ./out/, $(shell powershell -Command Get-ChildItem -Name -Filter *.o -Recurse -Path ./out)) ./font/hankaku.o
	@powershell -Command gcc kallsyms/kallsyms.c -o ./out/kallsyms.exe
	@$(NM) -n ./out/kernel.elf | powershell -Command ./out/kallsyms.exe >./out/kallsyms.S
	@$(CC) $(C_FLAGS) -c ./out/kallsyms.S -o ./out/kallsyms.o
	@$(LD) -z muldefs -e KernelMain -T linker.ld --static -o ./out/kernel.elf $(addprefix ./out/, $(shell powershell -Command Get-ChildItem -Name -Filter *.o -Recurse -Path ./out)) ./font/hankaku.o ./out/kallsyms.o
	@$(OBJD) -x -d -s ./out/kernel.elf >./out/kernel.txt
endif

	@$(COPY_CMD) ./out/BOOTX64.efi ./esp/EFI/BOOT/BOOTX64.efi
	@$(COPY_CMD) ./out/kernel.elf ./esp/kernel.elf

clean:
	@$(RMDIR_CMD) ./out ./esp ./nul

run: all
ifneq ($(OS), Windows_NT)
	$(QEMU_CMD)
else
	powershell -Command $(QEMU_CMD)
endif
