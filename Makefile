# Compiler settings
CC = gcc
CFLAGS = -Iinclude -Wall -Wextra -std=c11
LDFLAGS =

# Directories
SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
TEST_DIR = tests

# Source files & object files
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
DEPS = $(OBJS:.o=.d)

# Output library
TARGET = libaes.a

.PHONY: all clean test encrypt decrypt

# Default build target
all: $(BUILD_DIR)/$(TARGET)

# Build static library
$(BUILD_DIR)/$(TARGET): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	ar rcs $@ $^

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -MMD -c $< -o $@

# Include dependency files
-include $(DEPS)

# Clean build files
clean:
	rm -rf $(BUILD_DIR) $(TEST_DIR)/*.o $(TEST_DIR)/*.d $(TEST_DIR)/*.exe encrypt decrypt

# Run unit tests
test: $(BUILD_DIR)/$(TARGET)
	$(CC) $(CFLAGS) -o $(TEST_DIR)/test_aes $(TEST_DIR)/test_aes.c -L$(BUILD_DIR) -laes
	$(TEST_DIR)/test_aes

# Build encryption program
encrypt: $(BUILD_DIR)/$(TARGET)
	$(CC) $(CFLAGS) -o encrypt encrypt.c -L$(BUILD_DIR) -laes

# Build decryption program
decrypt: $(BUILD_DIR)/$(TARGET)
	$(CC) $(CFLAGS) -o decrypt decrypt.c -L$(BUILD_DIR) -laes