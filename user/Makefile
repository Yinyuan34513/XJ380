all: init.elf shell.elf

init.elf: init.c
	@x86_64-linux-musl-gcc -static $< -o ../out/$@
	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "CC $< -> ../out/$@"

shell.elf: shell.c
	@x86_64-linux-musl-gcc -static $< -o ../out/$@
	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "CC $< -> ../out/$@"

.PHONY: clean
clean:
	rm -rf ../out/xlibc ../out/init.* ../out/shell.*
