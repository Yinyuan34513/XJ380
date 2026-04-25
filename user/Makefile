all: init.elf shell.elf cp537.elf fmanager.elf bcms-sp.elf ctrlmenu.elf calc.elf texter.elf picturer.elf chat.elf httpget.elf browser.elf netcfg.elf taskmgr.elf sigtest.elf

export CXX ?= g++
export CC ?= gcc
export CFLAGS ?= -O0 -Wall -g -ffreestanding -fno-builtin -m64 -mstackrealign -std=c++11 -fno-stack-protector -fno-exceptions -fshort-wchar -nostdinc -I ./xapi/include -I ./include -Wno-write-strings
XAPI_HEADERS := $(shell find ./xapi/include -type f \( -name "*.h" -o -name "*.hpp" \))
USER_HEADERS := $(shell find ./include -type f \( -name "*.h" -o -name "*.hpp" \))
BROWSER_HEADERS := $(shell find ./browser -type f \( -name "*.h" -o -name "*.hpp" \))

MBEDTLS_SRC_DIR := ../third_party/mbedtls-src
MBEDTLS_CONFIG_HEADER := mbed_compat/xj380_mbedtls_user_config.h
MBEDTLS_CFLAGS := -I ./include/mbed_compat -I $(MBEDTLS_SRC_DIR)/include -DMBEDTLS_USER_CONFIG_FILE='"$(MBEDTLS_CONFIG_HEADER)"'
MBEDTLS_CCFLAGS := -O0 -Wall -g -ffreestanding -fno-builtin -m64 -mstackrealign -std=c11 -fno-stack-protector -fshort-wchar -nostdinc -I ./include/mbed_compat -I ./xapi/include -I ./include -I $(MBEDTLS_SRC_DIR)/include -DMBEDTLS_USER_CONFIG_FILE='"$(MBEDTLS_CONFIG_HEADER)"' -DMBEDTLS_NO_UDBL_DIVISION
MBEDTLS_LIBRARY_SOURCES := \
	$(MBEDTLS_SRC_DIR)/library/aes.c \
	$(MBEDTLS_SRC_DIR)/library/aria.c \
	$(MBEDTLS_SRC_DIR)/library/asn1parse.c \
	$(MBEDTLS_SRC_DIR)/library/asn1write.c \
	$(MBEDTLS_SRC_DIR)/library/base64.c \
	$(MBEDTLS_SRC_DIR)/library/bignum.c \
	$(MBEDTLS_SRC_DIR)/library/bignum_core.c \
	$(MBEDTLS_SRC_DIR)/library/bignum_mod.c \
	$(MBEDTLS_SRC_DIR)/library/bignum_mod_raw.c \
	$(MBEDTLS_SRC_DIR)/library/block_cipher.c \
	$(MBEDTLS_SRC_DIR)/library/camellia.c \
	$(MBEDTLS_SRC_DIR)/library/ccm.c \
	$(MBEDTLS_SRC_DIR)/library/chacha20.c \
	$(MBEDTLS_SRC_DIR)/library/chachapoly.c \
	$(MBEDTLS_SRC_DIR)/library/cipher.c \
	$(MBEDTLS_SRC_DIR)/library/cipher_wrap.c \
	$(MBEDTLS_SRC_DIR)/library/cmac.c \
	$(MBEDTLS_SRC_DIR)/library/constant_time.c \
	$(MBEDTLS_SRC_DIR)/library/ctr_drbg.c \
	$(MBEDTLS_SRC_DIR)/library/debug.c \
	$(MBEDTLS_SRC_DIR)/library/des.c \
	$(MBEDTLS_SRC_DIR)/library/dhm.c \
	$(MBEDTLS_SRC_DIR)/library/ecdh.c \
	$(MBEDTLS_SRC_DIR)/library/ecdsa.c \
	$(MBEDTLS_SRC_DIR)/library/ecjpake.c \
	$(MBEDTLS_SRC_DIR)/library/ecp.c \
	$(MBEDTLS_SRC_DIR)/library/ecp_curves.c \
	$(MBEDTLS_SRC_DIR)/library/ecp_curves_new.c \
	$(MBEDTLS_SRC_DIR)/library/error.c \
	$(MBEDTLS_SRC_DIR)/library/gcm.c \
	$(MBEDTLS_SRC_DIR)/library/hkdf.c \
	$(MBEDTLS_SRC_DIR)/library/hmac_drbg.c \
	$(MBEDTLS_SRC_DIR)/library/md.c \
	$(MBEDTLS_SRC_DIR)/library/md5.c \
	$(MBEDTLS_SRC_DIR)/library/mps_reader.c \
	$(MBEDTLS_SRC_DIR)/library/mps_trace.c \
	$(MBEDTLS_SRC_DIR)/library/nist_kw.c \
	$(MBEDTLS_SRC_DIR)/library/oid.c \
	$(MBEDTLS_SRC_DIR)/library/pem.c \
	$(MBEDTLS_SRC_DIR)/library/pk.c \
	$(MBEDTLS_SRC_DIR)/library/pk_ecc.c \
	$(MBEDTLS_SRC_DIR)/library/pk_wrap.c \
	$(MBEDTLS_SRC_DIR)/library/pkcs12.c \
	$(MBEDTLS_SRC_DIR)/library/pkcs5.c \
	$(MBEDTLS_SRC_DIR)/library/pkparse.c \
	$(MBEDTLS_SRC_DIR)/library/pkwrite.c \
	$(MBEDTLS_SRC_DIR)/library/platform.c \
	$(MBEDTLS_SRC_DIR)/library/platform_util.c \
	$(MBEDTLS_SRC_DIR)/library/poly1305.c \
	$(MBEDTLS_SRC_DIR)/library/psa_crypto.c \
	$(MBEDTLS_SRC_DIR)/library/psa_crypto_aead.c \
	$(MBEDTLS_SRC_DIR)/library/psa_crypto_cipher.c \
	$(MBEDTLS_SRC_DIR)/library/psa_crypto_driver_wrappers_no_static.c \
	$(MBEDTLS_SRC_DIR)/library/psa_crypto_ecp.c \
	$(MBEDTLS_SRC_DIR)/library/psa_crypto_ffdh.c \
	$(MBEDTLS_SRC_DIR)/library/psa_crypto_hash.c \
	$(MBEDTLS_SRC_DIR)/library/psa_crypto_mac.c \
	$(MBEDTLS_SRC_DIR)/library/psa_crypto_pake.c \
	$(MBEDTLS_SRC_DIR)/library/psa_crypto_rsa.c \
	$(MBEDTLS_SRC_DIR)/library/psa_crypto_slot_management.c \
	$(MBEDTLS_SRC_DIR)/library/psa_util.c \
	$(MBEDTLS_SRC_DIR)/library/ripemd160.c \
	$(MBEDTLS_SRC_DIR)/library/rsa.c \
	$(MBEDTLS_SRC_DIR)/library/rsa_alt_helpers.c \
	$(MBEDTLS_SRC_DIR)/library/sha1.c \
	$(MBEDTLS_SRC_DIR)/library/sha256.c \
	$(MBEDTLS_SRC_DIR)/library/sha3.c \
	$(MBEDTLS_SRC_DIR)/library/sha512.c \
	$(MBEDTLS_SRC_DIR)/library/ssl_ciphersuites.c \
	$(MBEDTLS_SRC_DIR)/library/ssl_client.c \
	$(MBEDTLS_SRC_DIR)/library/ssl_debug_helpers_generated.c \
	$(MBEDTLS_SRC_DIR)/library/ssl_msg.c \
	$(MBEDTLS_SRC_DIR)/library/ssl_tls.c \
	$(MBEDTLS_SRC_DIR)/library/ssl_tls12_client.c \
	$(MBEDTLS_SRC_DIR)/library/ssl_tls13_client.c \
	$(MBEDTLS_SRC_DIR)/library/ssl_tls13_generic.c \
	$(MBEDTLS_SRC_DIR)/library/ssl_tls13_keys.c \
	$(MBEDTLS_SRC_DIR)/library/version.c \
	$(MBEDTLS_SRC_DIR)/library/version_features.c \
	$(MBEDTLS_SRC_DIR)/library/x509.c \
	$(MBEDTLS_SRC_DIR)/library/x509_create.c \
	$(MBEDTLS_SRC_DIR)/library/x509_crl.c \
	$(MBEDTLS_SRC_DIR)/library/x509_crt.c \
	$(MBEDTLS_SRC_DIR)/library/x509_csr.c \
	$(MBEDTLS_SRC_DIR)/library/x509write.c \
	$(MBEDTLS_SRC_DIR)/library/x509write_crt.c \
	$(MBEDTLS_SRC_DIR)/library/x509write_csr.c
MBEDTLS_LIBRARY_OBJECTS := $(patsubst $(MBEDTLS_SRC_DIR)/library/%.c, ../out/httpget/mbedtls/%.o, $(MBEDTLS_LIBRARY_SOURCES))

LITEHTML_SRC_DIR := ../third_party/litehtml
LITEHTML_INCLUDE_DIRS := -I $(LITEHTML_SRC_DIR)/include -I $(LITEHTML_SRC_DIR)/include/litehtml -I $(LITEHTML_SRC_DIR)/src -I $(LITEHTML_SRC_DIR)/src/gumbo -I $(LITEHTML_SRC_DIR)/src/gumbo/include -I $(LITEHTML_SRC_DIR)/src/gumbo/include/gumbo
LITEHTML_SOURCES := $(shell find $(LITEHTML_SRC_DIR)/src -maxdepth 1 -name "*.cpp" | sort)
LITEHTML_OBJECTS := $(patsubst $(LITEHTML_SRC_DIR)/src/%.cpp, ../out/browser/litehtml/%.o, $(LITEHTML_SOURCES))
GUMBO_SOURCES := $(shell find $(LITEHTML_SRC_DIR)/src/gumbo -maxdepth 1 -name "*.c" | sort)
GUMBO_OBJECTS := $(patsubst $(LITEHTML_SRC_DIR)/src/gumbo/%.c, ../out/browser/gumbo/%.o, $(GUMBO_SOURCES))
WEBP_SRC_DIR := ./browser/third_party/libwebp/src
WEBP_INCLUDE_DIRS := -I ./browser/third_party/libwebp
WEBP_DEC_SOURCES := \
	$(WEBP_SRC_DIR)/dec/alpha_dec.c \
	$(WEBP_SRC_DIR)/dec/buffer_dec.c \
	$(WEBP_SRC_DIR)/dec/frame_dec.c \
	$(WEBP_SRC_DIR)/dec/idec_dec.c \
	$(WEBP_SRC_DIR)/dec/io_dec.c \
	$(WEBP_SRC_DIR)/dec/quant_dec.c \
	$(WEBP_SRC_DIR)/dec/tree_dec.c \
	$(WEBP_SRC_DIR)/dec/vp8_dec.c \
	$(WEBP_SRC_DIR)/dec/vp8l_dec.c \
	$(WEBP_SRC_DIR)/dec/webp_dec.c \
	$(WEBP_SRC_DIR)/dsp/alpha_processing.c \
	$(WEBP_SRC_DIR)/dsp/cpu.c \
	$(WEBP_SRC_DIR)/dsp/dec.c \
	$(WEBP_SRC_DIR)/dsp/dec_clip_tables.c \
	$(WEBP_SRC_DIR)/dsp/filters.c \
	$(WEBP_SRC_DIR)/dsp/lossless.c \
	$(WEBP_SRC_DIR)/dsp/rescaler.c \
	$(WEBP_SRC_DIR)/dsp/upsampling.c \
	$(WEBP_SRC_DIR)/dsp/yuv.c \
	$(WEBP_SRC_DIR)/utils/bit_reader_utils.c \
	$(WEBP_SRC_DIR)/utils/color_cache_utils.c \
	$(WEBP_SRC_DIR)/utils/filters_utils.c \
	$(WEBP_SRC_DIR)/utils/huffman_utils.c \
	$(WEBP_SRC_DIR)/utils/palette.c \
	$(WEBP_SRC_DIR)/utils/quant_levels_dec_utils.c \
	$(WEBP_SRC_DIR)/utils/random_utils.c \
	$(WEBP_SRC_DIR)/utils/rescaler_utils.c \
	$(WEBP_SRC_DIR)/utils/thread_utils.c \
	$(WEBP_SRC_DIR)/utils/utils.c
WEBP_DEC_OBJECTS := $(patsubst $(WEBP_SRC_DIR)/%.c, ../out/browser/libwebp/%.o, $(WEBP_DEC_SOURCES))

BROWSER_HOSTED_CXXFLAGS := -O0 -Wall -g -std=c++17 -m64 -mstackrealign -fno-stack-protector -fno-threadsafe-statics -fno-use-cxa-atexit -ffunction-sections -fdata-sections -I ./browser $(LITEHTML_INCLUDE_DIRS) $(WEBP_INCLUDE_DIRS) -DLITEHTML_NO_THREADS -DLITEHTML_KEEP_GUMBO_OUTPUT
BROWSER_HOSTED_CFLAGS := -O0 -Wall -g -std=c11 -m64 -mstackrealign -fno-stack-protector -ffunction-sections -fdata-sections $(LITEHTML_INCLUDE_DIRS) $(WEBP_INCLUDE_DIRS) -DNDEBUG -DHAVE_CONFIG_H
BROWSER_START_CFLAGS := $(CFLAGS)

BROWSER_SOURCES := ./browser/browser_app.cpp ./browser/browser_fetch.cpp ./browser/browser_alloc_guard.cpp ./browser/browser_image_decode.cpp
BROWSER_OBJECTS := $(patsubst ./browser/%.cpp, ../out/browser/%.o, $(BROWSER_SOURCES))
BROWSER_START_OBJECT := ../out/browser/browser_start.o
BROWSER_TLS_OBJECTS := ../out/httpget/https_client.o ../out/httpget/http_tls_compat.o $(MBEDTLS_LIBRARY_OBJECTS)
BROWSER_STDCPP := $(shell $(CXX) -print-file-name=libstdc++.a)
BROWSER_SUPCPP := $(shell $(CXX) -print-file-name=libsupc++.a)
BROWSER_LIBGCC := $(shell $(CXX) -print-libgcc-file-name)
BROWSER_LIBGCC_EH := $(shell $(CXX) -print-file-name=libgcc_eh.a)
BROWSER_BUILTINS ?= $(shell runtime_dir="$$(clang --print-runtime-dir 2>/dev/null || true)"; \
	if [ -n "$$runtime_dir" ] && [ -f "$$runtime_dir/libclang_rt.builtins-x86_64.a" ]; then \
		echo "$$runtime_dir/libclang_rt.builtins-x86_64.a"; \
	else \
		find /usr/lib/clang -type f -name 'libclang_rt.builtins-x86_64.a' 2>/dev/null | sort -V | tail -n 1; \
	fi)

build_xapi:
	$(MAKE) -C xapi build_all_api

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

sigtest.elf: sigtest.cpp $(XAPI_HEADERS) $(USER_HEADERS)
	@$(CXX) $(CFLAGS) -c $< -o ../out/$@.o
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp.o") ../out/xapi/liballoc-x86_64.a ../out/$@.o -o ../out/$@
	@echo "CPPLD $< -> ../out/$@"

cp537.elf: cp537.cpp $(XAPI_HEADERS) $(USER_HEADERS)
	@$(CXX) $(CFLAGS) -c $< -o ../out/$@.o
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp.o") ../out/xapi/liballoc-x86_64.a ../out/$@.o -o ../out/$@
#	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "CPPLD $< -> ../out/$@"

picturer.elf: picturer/picturer.cpp $(XAPI_HEADERS) $(USER_HEADERS)
	@$(CXX) $(CFLAGS) -c $< -o ../out/$@.o
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp.o") ../out/xapi/liballoc-x86_64.a ../out/$@.o -o ../out/$@
#	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "CPPLD $< -> ../out/$@"

chat.elf: chat.cpp $(XAPI_HEADERS) $(USER_HEADERS)
	@$(CXX) $(CFLAGS) -c $< -o ../out/$@.o
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp.o") ../out/xapi/liballoc-x86_64.a ../out/$@.o -o ../out/$@
	@echo "CPPLD $< -> ../out/$@"

netcfg.elf: netcfg.cpp $(XAPI_HEADERS) $(USER_HEADERS)
	@$(CXX) $(CFLAGS) -c $< -o ../out/$@.o
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp.o") ../out/xapi/liballoc-x86_64.a ../out/$@.o -o ../out/$@
	@echo "CPPLD $< -> ../out/$@"

HTTPGET_SOURCES := ./httpget.cpp ./https_client.cpp ./http_tls_compat.cpp
HTTPGET_OBJECTS := $(patsubst ./%.cpp, ../out/httpget/%.o, $(HTTPGET_SOURCES))

httpget.elf: $(HTTPGET_OBJECTS) $(MBEDTLS_LIBRARY_OBJECTS)
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp.o") ../out/xapi/liballoc-x86_64.a $(HTTPGET_OBJECTS) $(MBEDTLS_LIBRARY_OBJECTS) $(BROWSER_BUILTINS) -o ../out/$@
	@echo "LINK httpget objects -> ../out/$@"

../out/httpget/%.o: ./%.cpp $(XAPI_HEADERS) $(USER_HEADERS)
	@mkdir -p $(@D)
	@$(CXX) $(MBEDTLS_CFLAGS) $(CFLAGS) -c $< -o $@
	@echo "CPP $< -> $@"

../out/httpget/mbedtls/%.o: $(MBEDTLS_SRC_DIR)/library/%.c
	@mkdir -p $(@D)
	@$(CC) $(MBEDTLS_CCFLAGS) -c $< -o $@
	@echo "CC $< -> $@"

browser.elf: $(BROWSER_START_OBJECT) $(BROWSER_OBJECTS) $(LITEHTML_OBJECTS) $(GUMBO_OBJECTS) $(WEBP_DEC_OBJECTS) $(BROWSER_TLS_OBJECTS)
	@$(LD) -Ttext=0x200000 --gc-sections --wrap=free $(shell find ../out/xapi -name "*.o" |grep -v "start.cpp.o" |grep -v "constart.cpp.o") ../out/xapi/liballoc-x86_64.a $(BROWSER_START_OBJECT) $(BROWSER_OBJECTS) $(LITEHTML_OBJECTS) $(GUMBO_OBJECTS) $(WEBP_DEC_OBJECTS) $(BROWSER_TLS_OBJECTS) --start-group $(BROWSER_STDCPP) $(BROWSER_SUPCPP) $(BROWSER_LIBGCC_EH) $(BROWSER_LIBGCC) $(BROWSER_BUILTINS) --end-group -o ../out/$@
	@echo "LINK browser objects -> ../out/$@"

../out/browser/browser_start.o: ./browser/browser_start.cpp $(XAPI_HEADERS) $(USER_HEADERS) $(BROWSER_HEADERS)
	@mkdir -p $(@D)
	@$(CXX) $(BROWSER_START_CFLAGS) -c $< -o $@
	@echo "CPP $< -> $@"

../out/browser/%.o: ./browser/%.cpp $(XAPI_HEADERS) $(USER_HEADERS) $(BROWSER_HEADERS)
	@mkdir -p $(@D)
	@$(CXX) $(BROWSER_HOSTED_CXXFLAGS) -c $< -o $@
	@echo "CPP $< -> $@"

../out/browser/litehtml/encodings.o: $(LITEHTML_SRC_DIR)/src/encodings.cpp
	@mkdir -p $(@D)
	@$(CXX) $(BROWSER_HOSTED_CXXFLAGS) -fexceptions -c $< -o $@
	@echo "CPP $< -> $@"

../out/browser/litehtml/%.o: $(LITEHTML_SRC_DIR)/src/%.cpp
	@mkdir -p $(@D)
	@$(CXX) $(BROWSER_HOSTED_CXXFLAGS) -fno-exceptions -c $< -o $@
	@echo "CPP $< -> $@"

../out/browser/gumbo/%.o: $(LITEHTML_SRC_DIR)/src/gumbo/%.c
	@mkdir -p $(@D)
	@$(CC) $(BROWSER_HOSTED_CFLAGS) -c $< -o $@
	@echo "CC $< -> $@"

../out/browser/libwebp/%.o: $(WEBP_SRC_DIR)/%.c
	@mkdir -p $(@D)
	@$(CC) $(BROWSER_HOSTED_CFLAGS) -c $< -o $@
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
TASKMGR_SOURCES := ./taskmgr/taskmgr.cpp


SHELL_OBJECTS := $(patsubst ./shell/%.cpp, ../out/shell/%.o, $(SHELL_SOURCES))
FMANAGER_OBJECTS := $(patsubst ./fmanager/%.cpp, ../out/fmanager/%.o, $(FMANAGER_SOURCES))
CTRLMENU_OBJECTS := $(patsubst ./ctrlmenu/%.cpp, ../out/ctrlmenu/%.o, $(CTRLMENU_SOURCES))
CALC_OBJECTS := $(patsubst ./calc/%.cpp, ../out/calc/%.o, $(CALC_SOURCES))
TEXTER_OBJECTS := $(patsubst ./texter/%.cpp, ../out/texter/%.o, $(TEXTER_SOURCES))
TASKMGR_OBJECTS := $(patsubst ./taskmgr/%.cpp, ../out/taskmgr/%.o, $(TASKMGR_SOURCES))

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

taskmgr.elf: $(TASKMGR_OBJECTS)
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp.o") ../out/xapi/liballoc-x86_64.a $(TASKMGR_OBJECTS) -o ../out/$@
	@echo "LINK taskmgr objects -> ../out/$@"

../out/taskmgr/%.o: ./taskmgr/%.cpp $(XAPI_HEADERS) $(USER_HEADERS)
	@mkdir -p $(@D)
	@$(CXX) $(CFLAGS) -c $< -o $@
	@echo "CPP $< -> $@"

.PHONY: clean
clean:
	rm -rf ../out/xapi ../out/init.* ../out/shell.* ../out/shell/*.* ../out/cp537.* ../out/fmanager/*.* ../out/bcms-sp.* ../out/ctrlmenu/*.* ../out/texter/*.* ../out/taskmgr.* ../out/taskmgr/*.* ../out/picturer.* ../out/netcfg.* ../out/sigtest.*
