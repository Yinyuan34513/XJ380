OS := $(shell uname)

ASM_FLAGS = -O0 -Wall -g -nostdlib -m64 -fno-stack-protector -ffreestanding -fno-exceptions -mcmodel=large
C_FLAGS = -O0 -Wall -g -ffreestanding -m64 -fno-stack-protector -fno-exceptions -fno-rtti -std=c11 -fshort-wchar -nostdinc -mcmodel=large
CPP_FLAGS = -O0 -Wall -g -ffreestanding -m64 -fno-stack-protector -fno-exceptions -fno-rtti -std=c++17 -fshort-wchar -nostdinc -fno-use-cxa-atexit -fno-threadsafe-statics -mcmodel=large

ifeq ($(OS), Linux)
COMPILER_PREFIX = x86_64-linux-gnu-
RMDIR_CMD = rm -rf
MKDIR_CMD = mkdir -p
# FILES
ASM_FILES := $(shell find ./kernel ./graphics ./driver ./lib ./font -name "*.S")
C_FILES := $(shell find ./kernel ./graphics ./driver ./lib ./font -name "*.c")
CPP_FILES := $(shell find ./kernel ./graphics ./driver ./lib ./font -name "*.cpp")
else ifeq ($(OS), Darwin)
COMPILER_PREFIX = x86_64-elf-
RMDIR_CMD = rm -rf
MKDIR_CMD = mkdir -p
# FILES
ASM_FILES := $(shell find ./kernel ./graphics ./driver ./lib ./font -name "*.S")
C_FILES := $(shell find ./kernel ./graphics ./driver ./lib ./font -name "*.c")
CPP_FILES := $(shell find ./kernel ./graphics ./driver ./lib ./font -name "*.cpp")
else
COMPILER_PREFIX = x86_64-elf-
RMDIR_CMD = rmdir /s /q
MKDIR_CMD = mkdir
# FILES
ASM_FILES := $(shell dir /B /S .\kernel\*.S .\graphics\*.S .\driver\*.S .\lib\*.S .\font\*.S)
C_FILES := $(shell dir /B /S .\kernel\*.c .\graphics\*.c .\driver\*.c .\lib\*.c .\font\*.c)
CPP_FILES := $(shell dir /B /S .\kernel\*.cpp .\graphics\*.cpp .\driver\*.cpp .\lib\*.cpp .\font\*.cpp)
endif

CPP_VERSION ?= 14

CC = $(COMPILER_PREFIX)gcc
CPP = $(COMPILER_PREFIX)g++-$(CPP_VERSION)
LD = $(COMPILER_PREFIX)ld
NM = $(COMPILER_PREFIX)nm
OBJD = $(COMPILER_PREFIX)objdump

ASM_OBJS := $(patsubst %.S, ./out/%.o, $(ASM_FILES))
C_OBJS := $(patsubst %.c, ./out/%.o, $(C_FILES))
CPP_OBJS := $(patsubst %.cpp, ./out/%.o, $(CPP_FILES))

out/%.o: %.S
	@mkdir -p $(@D)
	@$(CC) $(ASM_FLAGS) -c $< -o $@
	@echo "AS $< -> $@"

out/%.o: %.c
	@mkdir -p $(@D)
	@$(CC) $(C_FLAGS) -c $< -o $@
	@echo "CC $< -> $@"

out/%.o: %.cpp
	@mkdir -p $(@D)
	@$(CPP) $(CPP_FLAGS) -c $< -o $@
	@echo "CPP $< -> $@"

all: $(ASM_OBJS) $(C_OBJS) $(CPP_OBJS)
	@$(MKDIR_CMD) -p ./esp/EFI/BOOT

# 针对BOOT和KERNEL进行特殊处理
	@x86_64-w64-mingw32-gcc -Wall -Wextra -e efi_main -nostdinc -nostdlib -fno-builtin -Wl,--subsystem,10 -fshort-wchar -o out/BOOTX64.efi boot/bootx64.c

ifeq ($(OS), Linux)
	$(LD) -z muldefs -e KernelMain -T linker.ld --static -o ./out/kernel.elf $(shell find ./out -name "*.o") ./font/hankaku.o
	gcc kallsyms/kallsyms.c -o ./out/kallsyms
	$(NM) -n ./out/kernel.elf | ./out/kallsyms >./out/kallsyms.S
	$(CC) $(C_FLAGS) -c ./out/kallsyms.S -o ./out/kallsyms.o
	$(LD) -z muldefs -e KernelMain -T linker.ld --static -o ./out/kernel.elf $(shell find ./out -name "*.o") ./font/hankaku.o ./out/kallsyms.o
	$(OBJD) -x -d -s ./out/kernel.elf >./out/kernel.txt
else ifeq ($(OS), Darwin)
	$(LD) -z muldefs -e KernelMain -T linker.ld --static -o ./out/kernel.elf $(shell find ./out -name "*.o") ./font/hankaku.o
	gcc kallsyms/kallsyms.c -o ./out/kallsyms
	$(NM) -n ./out/kernel.elf | ./out/kallsyms >./out/kallsyms.S
	$(CC) $(C_FLAGS) -c ./out/kallsyms.S -o ./out/kallsyms.o
	$(LD) -z muldefs -e KernelMain -T linker.ld --static -o ./out/kernel.elf $(shell find ./out -name "*.o") ./font/hankaku.o ./out/kallsyms.o
	$(OBJD) -x -d -s ./out/kernel.elf >./out/kernel.txt
else
	$(LD) -z muldefs -e KernelMain -T linker.ld --static -o ./out/kernel.elf $(shell find ./out -name "*.o") ./font/hankaku.o
	gcc kallsyms/kallsyms.c -o ./out/kallsyms
	$(NM) -n ./out/kernel.elf | ./out/kallsyms >./out/kallsyms.S
	$(CC) $(C_FLAGS) -c ./out/kallsyms.S -o ./out/kallsyms.o
	$(LD) -z muldefs -e KernelMain -T linker.ld --static -o ./out/kernel.elf $(shell find ./out -name "*.o") ./font/hankaku.o ./out/kallsyms.o
	$(OBJD) -x -d -s ./out/kernel.elf >./out/kernel.txt
endif
	cp ./out/BOOTX64.efi ./esp/EFI/BOOT
	cp ./out/kernel.elf ./esp

clean:
	@$(RMDIR_CMD) ./out ./esp

run: all
	qemu-system-x86_64 -bios OVMF.fd -m 4096 -smp 4 -net none -drive if=none,id=harddisk,file=fat:rw:esp,index=0,format=vvfat -device ahci,id=ahci -device ide-hd,bus=ahci.0,drive=harddisk -serial file:serial.log
