# CC = gcc
# CFLAGS = -Wall -Wextra -O2 `pkg-config --cflags gtk+-3.0`
# LDFLAGS = `pkg-config --libs gtk+-3.0`

# SRC_DIR = src
# BUILD_DIR = build

# CLIENT_SRC = $(SRC_DIR)/client.c $(SRC_DIR)/aes.c
# SERVER_SRC = $(SRC_DIR)/server.c $(SRC_DIR)/aes.c

# CLIENT_BIN = $(BUILD_DIR)/client
# SERVER_BIN = $(BUILD_DIR)/server

# all: $(CLIENT_BIN) $(SERVER_BIN)

# $(BUILD_DIR):
# 	mkdir -p $(BUILD_DIR)

# $(CLIENT_BIN): $(CLIENT_SRC) | $(BUILD_DIR)
# 	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# $(SERVER_BIN): $(SERVER_SRC) | $(BUILD_DIR)
# 	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# clean:
# 	rm -rf $(BUILD_DIR)

# run-server: $(SERVER_BIN)
# 	./$(SERVER_BIN)

# run-client: $(CLIENT_BIN)
# 	./$(CLIENT_BIN)

# .PHONY: all clean run-server run-client



# Sử dụng cross-compiler của Yocto nếu có, nếu không sẽ dùng gcc mặc định
# Định nghĩa trình biên dịch
CC ?= $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Iinclude -g $(shell pkg-config --cflags gtk+-3.0)
LDFLAGS += -lpthread $(shell pkg-config --libs gtk+-3.0) -Wl,--hash-style=gnu

# Định nghĩa thư mục
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

# Danh sách file nguồn
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

# Danh sách binary
BINARIES = client server encrypt decrypt

# Tạo thư mục nếu chưa tồn tại
$(OBJ_DIR) $(BIN_DIR):
	mkdir -p $@

# Biên dịch file .c thành .o
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Tạo file thực thi
$(BIN_DIR)/%: $(OBJ_DIR)/%.o $(OBJ_DIR)/aes.o | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Build tất cả
all: $(BIN_DIR) $(OBJ_DIR) $(BINARIES:%=$(BIN_DIR)/%)

# Cài đặt vào thư mục Yocto
install: all
	install -d $(DESTDIR)/usr/bin
	install -m 0755 $(BIN_DIR)/* $(DESTDIR)/usr/bin/

# Dọn dẹp
clean:
	rm -rf $(OBJ_DIR)/* $(BIN_DIR)/*

.PHONY: all clean install
