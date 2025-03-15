SRC_FILES := $(shell find . -name "*.[Sc]")
OBJ_FILES := $(patsubst ./%, ../../xlibc/%.o, $(SRC_FILES))

all: $(OBJ_FILES)

../../xlibc/%.S.o: ./%.S
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "AS $<"

../../xlibc/%.c.o: ./%.c
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "CC $<"
