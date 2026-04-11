all: init.elf shell.elf cp537.elf fmanager.elf bcms-sp.elf ctrlmenu.elf calc.elf texter.elf picturer.elf xmlviewer.elf browser.elf

export CFLAGS ?= -O0 -Wall -g -ffreestanding -fno-builtin -m64 -std=c++11 -fno-stack-protector -fno-exceptions -fshort-wchar -nostdinc -I ./xapi/include -Wno-writable-strings

MBEDTLS_LIBRARY_SOURCES := $(shell find ../third_party/mbedtls-src/library -maxdepth 1 -name "*.c" | sort)
MBEDTLS_LIBRARY_OBJECTS := $(patsubst ../third_party/mbedtls-src/library/%.c, ../out/browser_mbedtls/%.o, $(MBEDTLS_LIBRARY_SOURCES))
LEXBOR_INCLUDE := -I ../third_party/lexbor/source
WEBP_INCLUDE := -I ./browser/third_party/libwebp
MBEDTLS_CFLAGS := -O0 -Wall -g -ffreestanding -fno-builtin -m64 -std=c11 -fno-stack-protector -fshort-wchar -nostdinc -ffunction-sections -fdata-sections -I ./include/mbed_compat -I ./xapi/include -I ./include $(LEXBOR_INCLUDE) $(WEBP_INCLUDE) -I ../third_party/mbedtls-src/include -I ../third_party/mbedtls-src/library -DMBEDTLS_USER_CONFIG_FILE=\"xj380_mbedtls_browser_config.h\" -Wno-format
MBEDTLS_CXXFLAGS := -O0 -Wall -g -ffreestanding -fno-builtin -m64 -std=c++11 -fno-stack-protector -fno-exceptions -fshort-wchar -nostdinc -ffunction-sections -fdata-sections -I ./include/mbed_compat -I ./xapi/include -I ./include $(LEXBOR_INCLUDE) $(WEBP_INCLUDE) -I ../third_party/mbedtls-src/include -I ../third_party/mbedtls-src/library -DMBEDTLS_USER_CONFIG_FILE=\"xj380_mbedtls_browser_config.h\" -Wno-writable-strings -Wno-format
LEXBOR_CORE_SOURCES := $(shell find ../third_party/lexbor/source/lexbor/core -maxdepth 1 -name "*.c" | sort)
LEXBOR_CSS_SOURCES := $(shell find ../third_party/lexbor/source/lexbor/css -maxdepth 3 -name "*.c" | sort)
LEXBOR_PORT_SOURCES := ../third_party/lexbor/source/lexbor/ports/posix/lexbor/core/memory.c
LEXBOR_SOURCES := $(LEXBOR_CORE_SOURCES) $(LEXBOR_CSS_SOURCES) $(LEXBOR_PORT_SOURCES)
LEXBOR_OBJECTS := $(patsubst ../third_party/lexbor/source/lexbor/%.c, ../out/browser_lexbor/%.o, $(LEXBOR_SOURCES))
LEXBOR_CFLAGS := -O0 -Wall -g -ffreestanding -fno-builtin -m64 -std=c11 -fno-stack-protector -fshort-wchar -nostdinc -ffunction-sections -fdata-sections -I ./include/mbed_compat -I ./xapi/include -I ./include $(LEXBOR_INCLUDE) -Wno-format
WEBP_DEC_SOURCES := ./browser/third_party/libwebp/src/dec/alpha_dec.c \
	./browser/third_party/libwebp/src/dec/buffer_dec.c \
	./browser/third_party/libwebp/src/dec/frame_dec.c \
	./browser/third_party/libwebp/src/dec/idec_dec.c \
	./browser/third_party/libwebp/src/dec/io_dec.c \
	./browser/third_party/libwebp/src/dec/quant_dec.c \
	./browser/third_party/libwebp/src/dec/tree_dec.c \
	./browser/third_party/libwebp/src/dec/vp8_dec.c \
	./browser/third_party/libwebp/src/dec/vp8l_dec.c \
	./browser/third_party/libwebp/src/dec/webp_dec.c
WEBP_DSP_SOURCES := ./browser/third_party/libwebp/src/dsp/alpha_processing.c \
	./browser/third_party/libwebp/src/dsp/cpu.c \
	./browser/third_party/libwebp/src/dsp/dec.c \
	./browser/third_party/libwebp/src/dsp/dec_clip_tables.c \
	./browser/third_party/libwebp/src/dsp/filters.c \
	./browser/third_party/libwebp/src/dsp/lossless.c \
	./browser/third_party/libwebp/src/dsp/rescaler.c \
	./browser/third_party/libwebp/src/dsp/upsampling.c \
	./browser/third_party/libwebp/src/dsp/yuv.c
WEBP_UTIL_SOURCES := ./browser/third_party/libwebp/src/utils/bit_reader_utils.c \
	./browser/third_party/libwebp/src/utils/color_cache_utils.c \
	./browser/third_party/libwebp/src/utils/filters_utils.c \
	./browser/third_party/libwebp/src/utils/huffman_utils.c \
	./browser/third_party/libwebp/src/utils/quant_levels_dec_utils.c \
	./browser/third_party/libwebp/src/utils/random_utils.c \
	./browser/third_party/libwebp/src/utils/rescaler_utils.c \
	./browser/third_party/libwebp/src/utils/thread_utils.c \
	./browser/third_party/libwebp/src/utils/utils.c
WEBP_SOURCES := $(WEBP_DEC_SOURCES) $(WEBP_DSP_SOURCES) $(WEBP_UTIL_SOURCES)
WEBP_OBJECTS := $(patsubst ./browser/third_party/libwebp/src/%.c, ../out/browser_webp/%.o, $(WEBP_SOURCES))
WEBP_CFLAGS := -O0 -Wall -g -ffreestanding -fno-builtin -m64 -std=c11 -fno-stack-protector -fshort-wchar -nostdinc -ffunction-sections -fdata-sections -I ./include/mbed_compat -I ./xapi/include -I ./include $(WEBP_INCLUDE) -include ./webp_compat.h -DHAVE_CONFIG_H -Wno-format

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

init.elf: init.cpp $(XAPI_HEADERS) $(USER_HEADERS)
	@$(CXX) $(CFLAGS) -c $< -o ../out/$@.o
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp.o") ../out/xapi/liballoc-x86_64.a ../out/$@.o -o ../out/$@
# 	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "CPPLD $< -> ../out/$@"

cp537.elf: cp537.cpp $(XAPI_HEADERS) $(USER_HEADERS)
	@$(CXX) $(CFLAGS) -c $< -o ../out/$@.o
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp.o") ../out/xapi/liballoc-x86_64.a ../out/$@.o -o ../out/$@
#	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "CPPLD $< -> ../out/$@"

picturer.elf: picturer/picturer.cpp
	@$(CPP) $(CFLAGS) -c $< -o ../out/$@.o
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp.o") ../out/xapi/liballoc-x86_64.a ../out/$@.o -o ../out/$@
#	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "CPPLD $< -> ../out/$@"

xmlviewer.elf: xmlviewer.cpp
	@$(CPP) $(CFLAGS) -c $< -o ../out/$@.o
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp.o") ../out/xapi/liballoc-x86_64.a ../out/$@.o -o ../out/$@
#	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "CPPLD $< -> ../out/$@"

browser.elf: browser_nostl.cpp browser_css_nostl.cpp browser_css_nostl_render.cpp browser_webp_nostl.cpp https_client.cpp http_tls_compat.cpp $(MBEDTLS_LIBRARY_OBJECTS) $(LEXBOR_OBJECTS) $(WEBP_OBJECTS)
	@mkdir -p ../out/browser_mbedtls
	@$(CXX) $(MBEDTLS_CXXFLAGS) -c browser_nostl.cpp -o ../out/browser_mbedtls/browser_nostl.o
	@$(CXX) $(MBEDTLS_CXXFLAGS) -c browser_css_nostl.cpp -o ../out/browser_mbedtls/browser_css_nostl.o
	@$(CXX) $(MBEDTLS_CXXFLAGS) -c browser_css_nostl_render.cpp -o ../out/browser_mbedtls/browser_css_nostl_render.o
	@$(CXX) $(MBEDTLS_CXXFLAGS) -c browser_webp_nostl.cpp -o ../out/browser_mbedtls/browser_webp_nostl.o
	@$(CXX) $(MBEDTLS_CXXFLAGS) -c https_client.cpp -o ../out/browser_mbedtls/https_client.o
	@$(CXX) $(MBEDTLS_CXXFLAGS) -c http_tls_compat.cpp -o ../out/browser_mbedtls/http_tls_compat.o
	@$(LD) --gc-sections -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp.o") ../out/browser_mbedtls/browser_nostl.o ../out/browser_mbedtls/browser_css_nostl.o ../out/browser_mbedtls/browser_css_nostl_render.o ../out/browser_mbedtls/browser_webp_nostl.o ../out/browser_mbedtls/https_client.o ../out/browser_mbedtls/http_tls_compat.o $(MBEDTLS_LIBRARY_OBJECTS) $(LEXBOR_OBJECTS) $(WEBP_OBJECTS) ../out/xapi/liballoc-x86_64.a -o ../out/$@
#	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "CPPLD $< -> ../out/$@"

../out/browser_mbedtls/%.o: ../third_party/mbedtls-src/library/%.c
	@mkdir -p $(@D)
	@$(CC) $(MBEDTLS_CFLAGS) -c $< -o $@
	@echo "CC $< -> $@"

../out/browser_lexbor/%.o: ../third_party/lexbor/source/lexbor/%.c
	@mkdir -p $(@D)
	@$(CC) $(LEXBOR_CFLAGS) -c $< -o $@
	@echo "CC $< -> $@"

../out/browser_webp/%.o: ./browser/third_party/libwebp/src/%.c
	@mkdir -p $(@D)
	@$(CC) $(WEBP_CFLAGS) -c $< -o $@
	@echo "CC $< -> $@"

bcms-sp.elf: bcms-sp.cpp $(XAPI_HEADERS) $(USER_HEADERS)
	@$(CXX) $(CFLAGS) -c $< -o ../out/$@.o
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

../out/calc/%.o: ./calc/%.cpp $(XAPI_HEADERS) $(USER_HEADERS)
	@mkdir -p $(@D)
	@$(CXX) $(CFLAGS) -c $< -o $@
	@echo "CPP $< -> $@"

shell.elf: $(SHELL_OBJECTS)
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp.o") ../out/xapi/liballoc-x86_64.a $(SHELL_OBJECTS) -o ../out/$@
#	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "LINK shell objects -> ../out/$@"

# 为每个 shell 源文件定义编译规则
../out/shell/%.o: ./shell/%.cpp $(XAPI_HEADERS) $(USER_HEADERS)
	@mkdir -p $(@D)
	@$(CXX) $(CFLAGS) -c $< -o $@
	@echo "CPP $< -> $@"

fmanager.elf: $(FMANAGER_OBJECTS)
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp.o") ../out/xapi/liballoc-x86_64.a $(FMANAGER_OBJECTS) -o ../out/$@
#	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "LINK fmanager objects -> ../out/$@"

calc.elf: $(CALC_OBJECTS)
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp.o") ../out/xapi/liballoc-x86_64.a $(CALC_OBJECTS) -o ../out/$@
#	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "LINK clac objects -> ../out/$@"

../out/fmanager/%.o: ./fmanager/%.cpp $(XAPI_HEADERS) $(USER_HEADERS)
	@mkdir -p $(@D)
	@$(CXX) $(CFLAGS) -c $< -o $@
	@echo "CPP $< -> $@"



ctrlmenu.elf: $(CTRLMENU_OBJECTS)
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp.o") ../out/xapi/liballoc-x86_64.a $(CTRLMENU_OBJECTS) -o ../out/$@
#	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "LINK ctrlmenu objects -> ../out/$@"

../out/ctrlmenu/%.o: ./ctrlmenu/%.cpp $(XAPI_HEADERS) $(USER_HEADERS)
	@mkdir -p $(@D)
	@$(CXX) $(CFLAGS) -c $< -o $@
	@echo "CPP $< -> $@"


texter.elf: $(TEXTER_OBJECTS)
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp.o") ../out/xapi/liballoc-x86_64.a $(TEXTER_OBJECTS) -o ../out/$@
#	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "LINK texter objects -> ../out/$@"

../out/texter/%.o: ./texter/%.cpp $(XAPI_HEADERS) $(USER_HEADERS)
	@mkdir -p $(@D)
	@$(CXX) $(CFLAGS) -c $< -o $@
	@echo "CPP $< -> $@"

.PHONY: clean
clean:
	rm -rf ../out/xapi ../out/init.* ../out/shell.* ../out/shell/*.* ../out/cp537.* ../out/fmanager/*.* ../out/bcms-sp.* ../out/ctrlmenu/*.* ../out/texter/*.* ../out/picturer.* ../out/xmlviewer.* ../out/browser.* ../out/browser_mbedtls ../out/browser_litehtml ../out/browser_lexbor
