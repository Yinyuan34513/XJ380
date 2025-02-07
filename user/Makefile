all: init.elf shell.elf

build_xlibc:
	make -C xlibc

init.elf: init.c
	@$(CC) $(C_FLAGS) -c $< -o ../out/$@.o
	@$(LD) -Ttext=0x100000 $(shell find ../out/xlibc -name "*.o") ../out/$@.o -o ../out/$@
	@echo "CCLD $< -> ../out/$@"

shell.elf: shell.c
	@$(CC) $(C_FLAGS) -c $< -o ../out/$@.o
	@$(LD) -Ttext=0x200000 $(shell find ../out/xlibc -name "*.o") ../out/$@.o -o ../out/$@
	@echo "CCLD $< -> ../out/$@"
