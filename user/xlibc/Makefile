SRC_FILES := $(shell find . -name "*.[Sc]")
OBJ_FILES := $(patsubst ./%, ../../out/xlibc/%.o, $(SRC_FILES))

all: $(OBJ_FILES)

../../out/xlibc/%.S.o: ./%.S
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "AS $<"

../../out/xlibc/%.c.o: ./%.c
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "CC $<"
