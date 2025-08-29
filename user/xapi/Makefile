SRC_FILES := $(shell find . \( -name "*.cpp" -o -name "*.S" \))
OBJ_FILES := $(patsubst ./%, ../../out/xapi/%.o, $(SRC_FILES))

all: $(OBJ_FILES)

../../out/xapi/%.S.o: ./%.S
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "AS $<"

../../out/xapi/%.cpp.o: ./%.cpp
	@mkdir -p $(@D)
	@$(CPP) $(CFLAGS) -c $< -o $@
	@echo "CPP $<"
