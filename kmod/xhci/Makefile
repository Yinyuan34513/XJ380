SOURCES := $(wildcard *.cpp)
OBJECTS := $(SOURCES:.cpp=.o)

CXX ?= clang++

CXX_FLAGS = -O0 -g -mno-red-zone -mstackrealign -nostdlib -ffreestanding -fno-builtin -m64 -fno-stack-protector -fno-exceptions -fno-rtti -std=gnu++17 -fshort-wchar -nostdinc -fno-use-cxa-atexit -fno-threadsafe-statics -mno-80387 -Wno-int-to-pointer-cast -Wno-macro-redefined -Wno-c11-extensions -Wno-c99-extensions -Wno-gnu-statement-expression-from-macro-expansion -Wno-writable-strings -Wno-c++11-narrowing -fPIC -fvisibility=hidden

LDFLAGS := -nostdlib -Wl,-e,dlmain -Wl,-Bsymbolic -zmuldefs

all: ../../out/xhci.sys

../../out/xhci.sys: $(OBJECTS)
	$(CXX) -shared usb_core.o usb_hub.o usb_msc.o xhci.o $(LDFLAGS) -o $@

%.o: %.cpp
	$(CXX) $(CXX_FLAGS) -I../../include/ -I./ -c $< -o $@

.PHONY: all
