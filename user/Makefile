all: init.elf shell.elf cp537.elf fmanager.elf

export CFLAGS ?= -O0 -Wall -g -ffreestanding -fno-builtin -m64 -std=c++11 -fno-stack-protector -fno-exceptions -fshort-wchar -nostdinc -I ./xapi/include -Wno-writable-strings

build_xapi:
	make -C xapi build_all_api

# init.elf: init.c
# 	@$(CC) -static $< -o ../out/$@
# 	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
# 	@echo "CC $< -> ../out/$@"

# 图形化程序用：
# $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp")
# 命令行程序用：
# $(shell find ../out/xapi -name "*.o" |grep -v "start.cpp")

init.elf: init.cpp
	@$(CPP) $(CFLAGS) -c $< -o ../out/$@.o
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp") ../out/$@.o -o ../out/$@
# 	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "CPPLD $< -> ../out/$@"

cp537.elf: cp537.cpp
	@$(CPP) $(CFLAGS) -c $< -o ../out/$@.o
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp") ../out/$@.o -o ../out/$@
#	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "CPPLD $< -> ../out/$@"


SHELL_SOURCES := ./shell/shell_main.cpp \
                ./shell/shell_ui.cpp \
                ./shell/shell_cmd.cpp \
                ./shell/shell_fs.cpp \
                ./shell/shell_editor.cpp

SHELL_OBJECTS := $(patsubst ./shell/%.cpp, ../out/shell/%.o, $(SHELL_SOURCES))

shell.elf: $(SHELL_OBJECTS)
	@$(LD) -z muldefs -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp") $(SHELL_OBJECTS) -o ../out/$@
#	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "LINK shell objects -> ../out/$@"

# 为每个 shell 源文件定义编译规则
../out/shell/%.o: ./shell/%.cpp
	@mkdir -p $(@D)
	@$(CPP) $(CFLAGS) -c $< -o $@
	@echo "CPP $< -> $@"

fmanager.elf: ./fmanager/fmanager.cpp
	@$(CPP) $(CFLAGS) -c $< -o ../out/$@.o
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp") ../out/$@.o -o ../out/$@
#	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "CPPLD $< -> ../out/$@"

.PHONY: clean
clean:
	rm -rf ../out/xapi ../out/init.* ../out/shell.* ../out/cp537.* ../out/fmanager.*