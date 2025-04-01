all: init.elf shell.elf

export CFLAGS ?= -O0 -Wall -g -ffreestanding -fno-builtin -mno-sse -mno-mmx -mno-sse2 -m64 -fno-stack-protector -fno-exceptions -std=c99 -fshort-wchar -nostdinc

build_xlibc:
	make -C xlibc

init.elf: init.c
	@$(CC) $(CFLAGS) -c $< -o ../out/$@.o
	@$(LD) $(shell find ../xlibc -name "*.o") ../out/$@.o -o ../out/$@
	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@rm -rf ../out/$@.o
	@echo "CCLD $< -> ../out/$@"

shell.elf: shell.c
	@$(CC) $(CFLAGS) -c $< -o ../out/$@.o
	@$(LD) $(shell find ../xlibc -name "*.o") ../out/$@.o -o ../out/$@
	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@rm -rf ../out/$@.o
	@echo "CCLD $< -> ../out/$@"

.PHONY: clean
clean:
	rm -rf ../out/xlibc ../out/init.* ../out/shell.*
