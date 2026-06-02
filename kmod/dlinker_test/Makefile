DEP_TARGET := ../../out/00_dlinker_dep.sys
TEST_TARGET := ../../out/01_dlinker_test.sys

CXX ?= clang++

CXX_FLAGS = -O0 -g -mno-red-zone -mstackrealign -nostdlib -ffreestanding -fno-builtin -m64 -fno-stack-protector -fno-exceptions -fno-rtti -std=gnu++17 -fshort-wchar -nostdinc -fno-use-cxa-atexit -fno-threadsafe-statics -mno-80387 -Wno-int-to-pointer-cast -Wno-macro-redefined -Wno-c11-extensions -Wno-c99-extensions -Wno-gnu-statement-expression-from-macro-expansion -Wno-writable-strings -Wno-c++11-narrowing -fPIC -fvisibility=hidden
LDFLAGS := -nostdlib -Wl,-e,dlmain

all: $(DEP_TARGET) $(TEST_TARGET)

$(DEP_TARGET): dep.o
	$(CXX) -shared dep.o $(LDFLAGS) -Wl,-soname,00_dlinker_dep.sys -o $@

$(TEST_TARGET): test.o $(DEP_TARGET)
	$(CXX) -shared test.o -L../../out -l:00_dlinker_dep.sys $(LDFLAGS) -Wl,-soname,01_dlinker_test.sys -o $@

%.o: %.cpp
	$(CXX) $(CXX_FLAGS) -I../../include/ -c $< -o $@

.PHONY: all
