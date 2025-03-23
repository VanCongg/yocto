# CC = gcc
# CFLAGS = -Wall -Iinclude -g `pkg-config --cflags gtk+-3.0`
# LDFLAGS = `pkg-config --libs gtk+-3.0` -lpthread

# SRC_DIR = src
# OBJ_DIR = obj
# BIN_DIR = bin

# # Tạo thư mục nếu chưa tồn tại
# $(OBJ_DIR):
# 	mkdir -p $(OBJ_DIR)

# $(BIN_DIR):
# 	mkdir -p $(BIN_DIR)

# # Biên dịch AES thành thư viện tĩnh
# $(OBJ_DIR)/aes.o: $(SRC_DIR)/aes.c include/aes.h | $(OBJ_DIR)
# 	$(CC) $(CFLAGS) -c $< -o $@

# # Biên dịch mã nguồn của client
# $(OBJ_DIR)/client.o: $(SRC_DIR)/client.c include/aes.h | $(OBJ_DIR)
# 	$(CC) $(CFLAGS) -c $< -o $@

# # Biên dịch mã nguồn của server
# $(OBJ_DIR)/server.o: $(SRC_DIR)/server.c include/aes.h | $(OBJ_DIR)
# 	$(CC) $(CFLAGS) -c $< -o $@

# # Biên dịch encrypt và decrypt
# $(OBJ_DIR)/encrypt.o: $(SRC_DIR)/encrypt.c include/aes.h | $(OBJ_DIR)
# 	$(CC) $(CFLAGS) -c $< -o $@

# $(OBJ_DIR)/decrypt.o: $(SRC_DIR)/decrypt.c include/aes.h | $(OBJ_DIR)
# 	$(CC) $(CFLAGS) -c $< -o $@

# # Build client (có GUI)
# client: $(BIN_DIR) $(OBJ_DIR)/client.o $(OBJ_DIR)/aes.o
# 	$(CC) $(CFLAGS) $^ -o $(BIN_DIR)/client $(LDFLAGS)

# # Build server (có GUI)
# server: $(BIN_DIR) $(OBJ_DIR)/server.o $(OBJ_DIR)/aes.o
# 	$(CC) $(CFLAGS) $^ -o $(BIN_DIR)/server $(LDFLAGS)

# # Build encrypt
# encrypt: $(BIN_DIR) $(OBJ_DIR)/encrypt.o $(OBJ_DIR)/aes.o
# 	$(CC) $(CFLAGS) $^ -o $(BIN_DIR)/encrypt $(LDFLAGS)

# # Build decrypt
# decrypt: $(BIN_DIR) $(OBJ_DIR)/decrypt.o $(OBJ_DIR)/aes.o
# 	$(CC) $(CFLAGS) $^ -o $(BIN_DIR)/decrypt $(LDFLAGS)

# # Build tất cả
# all: client server encrypt decrypt

# # Dọn dẹp file biên dịch
# clean:
# 	rm -rf $(OBJ_DIR)/*.o $(BIN_DIR)/*

# # Cài đặt vào thư mục Yocto
# install: all | $(BIN_DIR)
# 	install -d $(DESTDIR)/usr/bin
# 	install -m 0755 $(BIN_DIR)/* $(DESTDIR)/usr/bin

# # Chạy chương trình client
# run-client: client
# 	./bin/client

# # Chạy chương trình server
# run-server: server
# 	./bin/server
# Sử dụng cross-compiler của Yocto nếu có, nếu không sẽ dùng gcc mặc định
# Định nghĩa trình biên dịch
CC ?= $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Iinclude -g
LDFLAGS = -lpthread

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
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: all clean install
