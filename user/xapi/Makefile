SRC_FILES := $(shell find . \( -name "*.cpp" -o -name "*.S" \) |grep -v "start.cpp" |grep -v "constart.cpp")
OBJ_FILES := $(patsubst ./%, ../../out/xapi/%.o, $(SRC_FILES))
XAPI_HEADERS := $(shell find ./include -type f \( -name "*.h" -o -name "*.hpp" \))
XAPI_IMPL_INCLUDES := $(shell find . -type f -name "*.inc")

export CXX ?= g++
export CFLAGS ?= -O0 -Wall $(TARGET_FLAG) -g -ffreestanding -fno-builtin -m64 -mstackrealign -std=c++11 -fno-stack-protector -fshort-wchar -nostdinc -I ./include -Wno-write-strings

compile_head: start.cpp $(XAPI_HEADERS)
	@mkdir -p ../../out/xapi
	@$(CC) $(CFLAGS) -c start.cpp -o ../../out/xapi/start.cpp.o
	@echo "SP-CPP start.cpp"

compile_headcon: constart.cpp $(XAPI_HEADERS)
	@mkdir -p ../../out/xapi
	@$(CC) $(CFLAGS) -c constart.cpp -o ../../out/xapi/constart.cpp.o
	@echo "SP-CPP constart.cpp"

all: $(OBJ_FILES)

build_all_api: all compile_head compile_headcon
	@find ../../out/xapi -maxdepth 1 -name "*.o" | while read f; do \
		rel=$${f#../../out/xapi/}; \
		case "$$rel" in \
			start.cpp.o|constart.cpp.o) continue ;; \
		esac; \
		src=./$${rel%.o}; \
		if [ ! -f "$$src" ]; then rm -f "$$f" "$${f%.o}.d"; fi; \
	done
	@rm -f ../../out/xapi/httpclient.cpp.o
	@cp -a ../../liballoc-x86_64.a ../../out/xapi

../../out/xapi/%.S.o: ./%.S $(XAPI_HEADERS)
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "AS $<"

../../out/xapi/%.cpp.o: ./%.cpp $(XAPI_HEADERS) $(XAPI_IMPL_INCLUDES)
	@mkdir -p $(@D)
	@$(CXX) $(CFLAGS) -c $< -o $@
	@echo "CPP $<"
