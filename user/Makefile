all: init.elf shell.elf cp537.elf fmanager.elf bcms-sp.elf ctrlmenu.elf calc.elf texter.elf picturer.elf chat.elf

export CFLAGS ?= -O0 -Wall -g -ffreestanding -fno-builtin -m64 -std=c++11 -fno-stack-protector -fno-exceptions -fshort-wchar -nostdinc -I ./xapi/include -Wno-writable-strings

build_xapi:
	make -C xapi build_all_api

# init.elf: init.c
# 	@$(CC) -static $< -o ../out/$@
# 	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
# 	@echo "CC $< -> ../out/$@"

# 图形化程序用：
# $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp.o")
# 命令行程序用：
# $(shell find ../out/xapi -name "*.o" |grep -v "start.cpp.o")

init.elf: init.cpp
	@$(CPP) $(CFLAGS) -c $< -o ../out/$@.o
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp.o") ../out/xapi/liballoc-x86_64.a ../out/$@.o -o ../out/$@
# 	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "CPPLD $< -> ../out/$@"

cp537.elf: cp537.cpp
	@$(CPP) $(CFLAGS) -c $< -o ../out/$@.o
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp.o") ../out/xapi/liballoc-x86_64.a ../out/$@.o -o ../out/$@
#	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "CPPLD $< -> ../out/$@"

picturer.elf: picturer/picturer.cpp
	@$(CPP) $(CFLAGS) -c $< -o ../out/$@.o
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp.o") ../out/xapi/liballoc-x86_64.a ../out/$@.o -o ../out/$@
#	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "CPPLD $< -> ../out/$@"

chat.elf: chat.cpp
	@$(CPP) $(CFLAGS) -c $< -o ../out/$@.o
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp.o") ../out/xapi/liballoc-x86_64.a ../out/$@.o -o ../out/$@
	@echo "CPPLD $< -> ../out/$@"

bcms-sp.elf: bcms-sp.cpp
	@$(CPP) $(CFLAGS) -c $< -o ../out/$@.o
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "start.cpp.o" |grep -v "constart.cpp.o") ../out/xapi/constart.cpp.o ../out/xapi/liballoc-x86_64.a ../out/$@.o -o ../out/$@
#	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "CPPLD $< -> ../out/$@"


SHELL_SOURCES := ./shell/shell_main.cpp \
                ./shell/shell_ui.cpp \
                ./shell/shell_cmd.cpp \
                ./shell/shell_fs.cpp \
                ./shell/shell_editor.cpp \
                ./shell/shell_tam.cpp \
                ./shell/sxah.cpp 

FMANAGER_SOURCES := ./fmanager/fmanager.cpp \
                ./fmanager/fm_paint.cpp \
                ./fmanager/fm_pathp.cpp \
                ./fmanager/fm_processp.cpp 

CTRLMENU_SOURCES := ./ctrlmenu/ctrlmenu.cpp \
                ./ctrlmenu/cm_graph.cpp \
                ./ctrlmenu/cm_processp.cpp 

CALC_SOURCES := ./calc/calc.cpp

TEXTER_SOURCES := ./texter/texter.cpp


SHELL_OBJECTS := $(patsubst ./shell/%.cpp, ../out/shell/%.o, $(SHELL_SOURCES))
FMANAGER_OBJECTS := $(patsubst ./fmanager/%.cpp, ../out/fmanager/%.o, $(FMANAGER_SOURCES))
CTRLMENU_OBJECTS := $(patsubst ./ctrlmenu/%.cpp, ../out/ctrlmenu/%.o, $(CTRLMENU_SOURCES))
CALC_OBJECTS := $(patsubst ./calc/%.cpp, ../out/calc/%.o, $(CALC_SOURCES))
TEXTER_OBJECTS := $(patsubst ./texter/%.cpp, ../out/texter/%.o, $(TEXTER_SOURCES))

../out/calc/%.o: ./calc/%.cpp
	@mkdir -p $(@D)
	@$(CPP) $(CFLAGS) -c $< -o $@
	@echo "CPP $< -> $@"

shell.elf: $(SHELL_OBJECTS)
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp.o") ../out/xapi/liballoc-x86_64.a $(SHELL_OBJECTS) -o ../out/$@
#	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "LINK shell objects -> ../out/$@"

# 为每个 shell 源文件定义编译规则
../out/shell/%.o: ./shell/%.cpp
	@mkdir -p $(@D)
	@$(CPP) $(CFLAGS) -c $< -o $@
	@echo "CPP $< -> $@"

fmanager.elf: $(FMANAGER_OBJECTS)
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp.o") ../out/xapi/liballoc-x86_64.a $(FMANAGER_OBJECTS) -o ../out/$@
#	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "LINK fmanager objects -> ../out/$@"

calc.elf: $(CALC_OBJECTS)
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp.o") ../out/xapi/liballoc-x86_64.a $(CALC_OBJECTS) -o ../out/$@
#	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "LINK clac objects -> ../out/$@"

../out/fmanager/%.o: ./fmanager/%.cpp
	@mkdir -p $(@D)
	@$(CPP) $(CFLAGS) -c $< -o $@
	@echo "CPP $< -> $@"



ctrlmenu.elf: $(CTRLMENU_OBJECTS)
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp.o") ../out/xapi/liballoc-x86_64.a $(CTRLMENU_OBJECTS) -o ../out/$@
#	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "LINK ctrlmenu objects -> ../out/$@"

../out/ctrlmenu/%.o: ./ctrlmenu/%.cpp
	@mkdir -p $(@D)
	@$(CPP) $(CFLAGS) -c $< -o $@
	@echo "CPP $< -> $@"


texter.elf: $(TEXTER_OBJECTS)
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp.o") ../out/xapi/liballoc-x86_64.a $(TEXTER_OBJECTS) -o ../out/$@
#	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "LINK texter objects -> ../out/$@"

../out/texter/%.o: ./texter/%.cpp
	@mkdir -p $(@D)
	@$(CPP) $(CFLAGS) -c $< -o $@
	@echo "CPP $< -> $@"

.PHONY: clean
clean:
	rm -rf ../out/xapi ../out/init.* ../out/shell.* ../out/shell/*.* ../out/cp537.* ../out/fmanager/*.* ../out/bcms-sp.* ../out/ctrlmenu/*.* ../out/texter/*.* ../out/picturer.*
