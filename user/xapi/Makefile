SRC_FILES := $(shell find . \( -name "*.cpp" -o -name "*.S" \) |grep -v "start.cpp" |grep -v "constart.cpp")
OBJ_FILES := $(patsubst ./%, ../../out/xapi/%.o, $(SRC_FILES))

export CXX ?= g++
export CFLAGS ?= -O0 -Wall -g -ffreestanding -fno-builtin -m64 -mstackrealign -std=c++11 -fno-stack-protector -fshort-wchar -nostdinc -I ./include -Wno-write-strings

compile_head:
	@mkdir -p ../../out/xapi
	@$(CC) $(CFLAGS) -c start.cpp -o ../../out/xapi/start.cpp.o
	@echo "SP-CPP start.cpp"

compile_headcon:
	@mkdir -p ../../out/xapi
	@$(CC) $(CFLAGS) -c constart.cpp -o ../../out/xapi/constart.cpp.o
	@echo "SP-CPP constart.cpp"

all: $(OBJ_FILES)

build_all_api: all compile_head compile_headcon
	@rm -f ../../out/xapi/httpclient.cpp.o
	@cp -a ../../liballoc-x86_64.a ../../out/xapi

../../out/xapi/%.S.o: ./%.S
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "AS $<"

../../out/xapi/%.cpp.o: ./%.cpp
	@mkdir -p $(@D)
	@$(CXX) $(CFLAGS) -c $< -o $@
	@echo "CPP $<"
