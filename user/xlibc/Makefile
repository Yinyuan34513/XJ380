SRC_FILES := $(shell find . -name "*.[Sc]")
OBJ_FILES := $(patsubst %, ../out/xlibc/%.o, $(SRC_FILES))

all: $(OBJ_FILES)

%.S.o: %.S
	@$(CC) -c $< -o $@
	@echo "AS $<"

%.c.o: %.c
	@$(CC) -c $< -o $@
	@echo "CC $<"
