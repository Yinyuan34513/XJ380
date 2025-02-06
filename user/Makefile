all: init.elf shell.elf

init.elf: init.c
	@$(CC) $(C_FLAGS) -c $< -o ../out/$@.o
	@$(LD) ../out/$@.o -o ../out/$@
	@echo "CCLD $< -> ../out/$@"

shell.elf: shell.c
	@$(CC) $(C_FLAGS) -c $< -o ../out/$@.o
	@$(LD) ../out/$@.o -o ../out/$@
	@echo "CCLD $< -> ../out/$@"
