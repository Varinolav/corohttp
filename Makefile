SRC_DIR   = source
BUILD_DIR = build
OBJ_DIR   = $(BUILD_DIR)/obj
BIN_DIR   = $(BUILD_DIR)/bin

COMPILER           ?= clang
OPTIMIZATION_LEVEL ?= -O3
SANITIZERS         ?=

CC        = $(COMPILER)

CFLAGS    = -g -I $(SRC_DIR)
CFLAGS   += -Wall -Werror
CFLAGS   += -fno-omit-frame-pointer
CFLAGS   += $(OPTIMIZATION_LEVEL)
CFLAGS   += $(SANITIZERS)

LDFLAGS   = $(CFLAGS)

SRCS     := $(shell find source -iname '*.c') $(shell find source -iname '*.S')
SRCS     := $(filter-out $(SRC_DIR)/server.c $(SRC_DIR)/async_server.c,$(SRCS))

OBJS     := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))
OBJS     := $(patsubst $(SRC_DIR)/%.S, $(OBJ_DIR)/%.o, $(OBJS))

TARGET    = app
.DEFAULT_GOAL := run

.PHONY: run compile clean threaded-server async-server

threaded-server: $(BIN_DIR)/threaded_server

async-server: $(BIN_DIR)/async_server

$(BIN_DIR)/threaded_server: $(SRC_DIR)/server.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -pthread $< -o $@ $(LDFLAGS)

CORO_OBJS := $(filter $(OBJ_DIR)/coroed/%,$(OBJS))

$(BIN_DIR)/async_server: $(OBJ_DIR)/async_server.o $(CORO_OBJS) | $(BIN_DIR)
	$(CC) $^ -o $@ $(LDFLAGS)

run: $(BIN_DIR)/$(TARGET)
	./$(BIN_DIR)/$(TARGET)

compile: clean $(BIN_DIR)/$(TARGET)

clean:
	rm -rf $(BUILD_DIR)
	rm -rf ./**/*.o

$(BIN_DIR)/$(TARGET): $(BIN_DIR) $(OBJ_DIR) $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(shell dirname $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.S
	mkdir -p $(shell dirname $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR): $(BUILD_DIR)
	mkdir -p $(OBJ_DIR)

$(BIN_DIR): $(BUILD_DIR)
	mkdir -p $(BIN_DIR)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

check-style:
	find source -iname '*.h' -o -iname '*.c' \
	| xargs clang-tidy -p compile_commands.json

check-format:
	find source -iname '*.h' -o -iname '*.c' \
	| xargs clang-format -Werror --dry-run --fallback-style=Google --verbose

fix-format:
	find source -iname '*.h' -o -iname '*.c' \
	| xargs clang-format -i --fallback-style=Google --verbose

info:
	$(info CC        = $(CC))
	$(info CFLAGS    = $(CFLAGS))
	$(info LDFLAGS   = $(LDFLAGS))
	$(info SRC_DIR   = $(SRC_DIR))
	$(info BUILD_DIR = $(BUILD_DIR))
	$(info SRCS      = $(SRCS))
	$(info OBJS      = $(OBJS))
