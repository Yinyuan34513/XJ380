SOURCES := $(shell find . -name "*.cpp")
OBJECTS := $(SOURCES:.cpp=.o)

CXX_FLAGS = -O0 -g -mno-red-zone -mstackrealign -nostdlib -ffreestanding -fno-builtin -m64 -fno-stack-protector -fno-exceptions -fno-rtti -std=gnu++17 -fshort-wchar -nostdinc -fno-use-cxa-atexit -fno-threadsafe-statics -mno-80387 -Wno-int-to-pointer-cast -Wno-macro-redefined -Wno-c11-extensions -Wno-c99-extensions -Wno-gnu-statement-expression-from-macro-expansion -Wno-writable-strings -Wno-c++11-narrowing -fPIC -fvisibility=hidden

LDFLAGS := -nostdlib -Wl,-e,dlmain 

all: ../../out/e1000.km

../../out/e1000.km: $(OBJECTS)
	$(CPP) -shared $(OBJECTS) $(LDFLAGS) -o $@

%.o: %.cpp
	$(CPP) $(CXX_FLAGS) -I../../include/ -c $< -o $@

.PHONY: all