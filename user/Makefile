all: init.elf shell.elf

init.elf: init.c
	@$(CC) $(C_FLAGS) -c $< -o ../out/$@.o
	@$(LD) -Ttext=0x100000 ../out/$@.o -o ../out/$@
	@echo "CCLD $< -> ../out/$@"

shell.elf: shell.c
	@$(CC) $(C_FLAGS) -c $< -o ../out/$@.o
	@$(LD) -Ttext=0x200000 ../out/$@.o -o ../out/$@
	@echo "CCLD $< -> ../out/$@"
