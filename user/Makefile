all: init.elf

init.elf: init.c
	@$(CC) $(C_FLAGS) -c $< -o ../out/$@.o
	@$(LD) ../out/$@.o -o ../out/$@
	@echo "CCLD $< -> ../out/$@"
