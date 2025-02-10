all: init.elf shell.elf

export CFLAGS ?= -O0 -Wall -g -ffreestanding -fno-builtin -mno-sse -mno-mmx -mno-sse2 -m64 -fno-stack-protector -fno-exceptions -std=c99 -fshort-wchar -nostdinc

build_xlibc:
	make -C xlibc

init.elf: init.c
	@$(CC) $(CFLAGS) -c $< -o ../out/$@.o
	@$(LD) -Ttext=0x100000 $(shell find ../out/xlibc -name "*.o") ../out/$@.o -o ../out/$@
	@echo "CCLD $< -> ../out/$@"

shell.elf: shell.c
	@$(CC) $(CFLAGS) -c $< -o ../out/$@.o
	@$(LD) -Ttext=0x200000 $(shell find ../out/xlibc -name "*.o") ../out/$@.o -o ../out/$@
	@echo "CCLD $< -> ../out/$@"
