all: init.elf shell.elf cp537.elf fmanager.elf bcms-sp.elf ctrlmenu.elf calc.elf texter.elf picturer.elf chat.elf httpget.elf

export CXX ?= g++
export CC ?= gcc
export CFLAGS ?= -O0 -Wall -g -ffreestanding -fno-builtin -m64 -mstackrealign -std=c++11 -fno-stack-protector -fno-exceptions -fshort-wchar -nostdinc -I ./xapi/include -I ./include -Wno-write-strings

MBEDTLS_SRC_DIR := ../third_party/mbedtls-src
MBEDTLS_CONFIG_HEADER := mbed_compat/xj380_mbedtls_user_config.h
MBEDTLS_CFLAGS := -I ./include/mbed_compat -I $(MBEDTLS_SRC_DIR)/include -DMBEDTLS_USER_CONFIG_FILE=\"$(MBEDTLS_CONFIG_HEADER)\"
MBEDTLS_CCFLAGS := -O0 -Wall -g -ffreestanding -fno-builtin -m64 -mstackrealign -std=c11 -fno-stack-protector -fshort-wchar -nostdinc -I ./include/mbed_compat -I ./xapi/include -I ./include -I $(MBEDTLS_SRC_DIR)/include -DMBEDTLS_USER_CONFIG_FILE=\"$(MBEDTLS_CONFIG_HEADER)\"
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
	@$(CXX) $(CFLAGS) -c $< -o ../out/$@.o
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp.o") ../out/xapi/liballoc-x86_64.a ../out/$@.o -o ../out/$@
# 	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "CPPLD $< -> ../out/$@"

cp537.elf: cp537.cpp
	@$(CXX) $(CFLAGS) -c $< -o ../out/$@.o
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp.o") ../out/xapi/liballoc-x86_64.a ../out/$@.o -o ../out/$@
#	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "CPPLD $< -> ../out/$@"

picturer.elf: picturer/picturer.cpp
	@$(CXX) $(CFLAGS) -c $< -o ../out/$@.o
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp.o") ../out/xapi/liballoc-x86_64.a ../out/$@.o -o ../out/$@
#	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "CPPLD $< -> ../out/$@"

chat.elf: chat.cpp
	@$(CXX) $(CFLAGS) -c $< -o ../out/$@.o
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp.o") ../out/xapi/liballoc-x86_64.a ../out/$@.o -o ../out/$@
	@echo "CPPLD $< -> ../out/$@"

HTTPGET_SOURCES := ./httpget.cpp ./https_client.cpp ./http_tls_compat.cpp
HTTPGET_OBJECTS := $(patsubst ./%.cpp, ../out/httpget/%.o, $(HTTPGET_SOURCES))

httpget.elf: $(HTTPGET_OBJECTS) $(MBEDTLS_LIBRARY_OBJECTS)
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp.o") ../out/xapi/liballoc-x86_64.a $(HTTPGET_OBJECTS) $(MBEDTLS_LIBRARY_OBJECTS) /usr/lib/clang/22/lib/linux/libclang_rt.builtins-x86_64.a -o ../out/$@
	@echo "LINK httpget objects -> ../out/$@"

../out/httpget/%.o: ./%.cpp
	@mkdir -p $(@D)
	@$(CXX) $(MBEDTLS_CFLAGS) $(CFLAGS) -c $< -o $@
	@echo "CPP $< -> $@"

../out/httpget/mbedtls/%.o: $(MBEDTLS_SRC_DIR)/library/%.c
	@mkdir -p $(@D)
	@$(CC) $(MBEDTLS_CCFLAGS) -c $< -o $@
	@echo "CC $< -> $@"

bcms-sp.elf: bcms-sp.cpp
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

../out/calc/%.o: ./calc/%.cpp
	@mkdir -p $(@D)
	@$(CXX) $(CFLAGS) -c $< -o $@
	@echo "CPP $< -> $@"

shell.elf: $(SHELL_OBJECTS)
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp.o") ../out/xapi/liballoc-x86_64.a $(SHELL_OBJECTS) -o ../out/$@
#	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "LINK shell objects -> ../out/$@"

# 为每个 shell 源文件定义编译规则
../out/shell/%.o: ./shell/%.cpp
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

../out/fmanager/%.o: ./fmanager/%.cpp
	@mkdir -p $(@D)
	@$(CXX) $(CFLAGS) -c $< -o $@
	@echo "CPP $< -> $@"



ctrlmenu.elf: $(CTRLMENU_OBJECTS)
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp.o") ../out/xapi/liballoc-x86_64.a $(CTRLMENU_OBJECTS) -o ../out/$@
#	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "LINK ctrlmenu objects -> ../out/$@"

../out/ctrlmenu/%.o: ./ctrlmenu/%.cpp
	@mkdir -p $(@D)
	@$(CXX) $(CFLAGS) -c $< -o $@
	@echo "CPP $< -> $@"


texter.elf: $(TEXTER_OBJECTS)
	@$(LD) -Ttext=0x200000 $(shell find ../out/xapi -name "*.o" |grep -v "constart.cpp.o") ../out/xapi/liballoc-x86_64.a $(TEXTER_OBJECTS) -o ../out/$@
#	@$(OBJD) -x -d -s ../out/$@ > ../out/$@.txt
	@echo "LINK texter objects -> ../out/$@"

../out/texter/%.o: ./texter/%.cpp
	@mkdir -p $(@D)
	@$(CXX) $(CFLAGS) -c $< -o $@
	@echo "CPP $< -> $@"

.PHONY: clean
clean:
	rm -rf ../out/xapi ../out/init.* ../out/shell.* ../out/shell/*.* ../out/cp537.* ../out/fmanager/*.* ../out/bcms-sp.* ../out/ctrlmenu/*.* ../out/texter/*.* ../out/picturer.*
