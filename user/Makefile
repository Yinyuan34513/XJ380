all: init.elf shell.elf cp537.elf

export CFLAGS ?= -O0 -Wall -g -ffreestanding -fno-builtin -m64 -std=c++11 -fno-stack-protector -fno-exceptions -fshort-wchar -nostdinc

build_xapi:
	make -C xapi

init.elf: init.c
	@$(CC) -static $< -o ../out/$@
	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "CC $< -> ../out/$@"

shell.elf: shell.cpp
	@$(CPP) $(CFLAGS) -c $< -o ../out/$@.o
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o") ../out/$@.o -o ../out/$@
	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "CPPLD $< -> ../out/$@"

cp537.elf: cp537.cpp
	@$(CPP) $(CFLAGS) -c $< -o ../out/$@.o
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o") ../out/$@.o -o ../out/$@
	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "CPPLD $< -> ../out/$@"

.PHONY: clean
clean:
	rm -rf ../out/xapi ../out/init.* ../out/shell.* ../out/cp537.*
