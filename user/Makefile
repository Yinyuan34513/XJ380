all: init.elf shell.elf

export CFLAGS ?= -O0 -Wall -g -ffreestanding -fno-builtin -mno-sse -mno-mmx -mno-sse2 -m64 -fno-stack-protector -fno-exceptions -std=c99 -fshort-wchar -nostdinc

build_xapi:
	make -C xapi

init.elf: init.c
	@x86_64-linux-musl-gcc -static $< -o ../out/$@
	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "CC $< -> ../out/$@"

shell.elf: shell.c
	@$(CC) $(CFLAGS) -c $< -o ../out/$@.o
	@$(LD) -Ttext=0x200000 $(shell find ../out/xlibc -name "*.o") ../out/$@.o -o ../out/$@
	@echo "CCLD $< -> ../out/$@"

.PHONY: clean
clean:
	rm -rf ../out/xlibc ../out/init.* ../out/shell.*
