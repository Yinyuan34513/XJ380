SRC_FILES := $(shell find . \( -name "*.cpp" -o -name "*.S" \) |grep -v "start.cpp" |grep -v "constart.cpp")
OBJ_FILES := $(patsubst ./%, ../../out/xapi/%.o, $(SRC_FILES))

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

../../out/xapi/%.S.o: ./%.S
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "AS $<"

../../out/xapi/%.cpp.o: ./%.cpp
	@mkdir -p $(@D)
	@$(CPP) $(CFLAGS) -c $< -o $@
	@echo "CPP $<"
