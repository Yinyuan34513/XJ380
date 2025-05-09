SRC_FILES := $(shell find . -name "*.[Sc]")
OBJ_FILES := $(patsubst ./%, ../../out/xapi/%.o, $(SRC_FILES))

all: $(OBJ_FILES)

../../out/xapi/%.S.o: ./%.S
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "AS $<"

../../out/xapi/%.c.o: ./%.c
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "CC $<"
