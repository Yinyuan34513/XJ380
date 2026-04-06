LWIP_CORE_C := \
	lwip/core/def.c \
	lwip/core/dns.c \
	lwip/core/init.c \
	lwip/core/inet_chksum.c \
	lwip/core/ip.c \
	lwip/core/mem.c \
	lwip/core/memp.c \
	lwip/core/netif.c \
	lwip/core/pbuf.c \
	lwip/core/stats.c \
	lwip/core/sys.c \
	lwip/core/tcp.c \
	lwip/core/tcp_in.c \
	lwip/core/tcp_out.c \
	lwip/core/udp.c \
	lwip/core/timeouts.c
LWIP_IPV4_C := \
	lwip/core/ipv4/dhcp.c \
	lwip/core/ipv4/etharp.c \
	lwip/core/ipv4/icmp.c \
	lwip/core/ipv4/ip4.c \
	lwip/core/ipv4/ip4_addr.c
LWIP_IPV6_C :=
LWIP_NETIF_C := lwip/netif/ethernet.c
LWIP_API_C := \
	lwip/api/api_lib.c \
	lwip/api/api_msg.c \
	lwip/api/err.c \
	lwip/api/netbuf.c \
	lwip/api/netifapi.c \
	lwip/api/tcpip.c

C_SOURCES := $(LWIP_CORE_C) $(LWIP_IPV4_C) $(LWIP_IPV6_C) $(LWIP_NETIF_C) $(LWIP_API_C)
CPP_SOURCES := \
	netserver.cpp \
	arch/sys_arch.cpp \
	arch/utils.cpp

C_OBJECTS := $(C_SOURCES:.c=.o)
CPP_OBJECTS := $(CPP_SOURCES:.cpp=.o)

CC ?= clang
CXX ?= clang++

C_FLAGS = -O0 -g -mno-red-zone -mstackrealign -nostdlib -ffreestanding -fno-builtin -m64 -fno-stack-protector -fno-exceptions -std=gnu11 -fshort-wchar -nostdinc -mno-80387 -fPIC -fvisibility=hidden -MMD -MP
CPP_FLAGS = -O0 -g -mno-red-zone -mstackrealign -nostdlib -ffreestanding -fno-builtin -m64 -fno-stack-protector -fno-exceptions -fno-rtti -std=gnu++17 -fshort-wchar -nostdinc -fno-use-cxa-atexit -fno-threadsafe-statics -mno-80387 -Wno-int-to-pointer-cast -Wno-macro-redefined -Wno-c11-extensions -Wno-c99-extensions -Wno-gnu-statement-expression-from-macro-expansion -Wno-writable-strings -Wno-c++11-narrowing -fPIC -fvisibility=hidden -MMD -MP
INCLUDES = -I../../include -I./lwip/include -I./
LDFLAGS := -nostdlib -Wl,-e,dlmain -Wl,-z,muldefs

all: ../../out/netserver.km

../../out/netserver.km: $(C_OBJECTS) $(CPP_OBJECTS)
	$(CXX) -shared $(CPP_OBJECTS) $(C_OBJECTS) $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(C_FLAGS) $(INCLUDES) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CPP_FLAGS) $(INCLUDES) -c $< -o $@

.PHONY: all

-include $(C_OBJECTS:.o=.d) $(CPP_OBJECTS:.o=.d)
