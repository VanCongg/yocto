CC = gcc
CFLAGS = -Wall -Iinclude -g `pkg-config --cflags gtk+-3.0`
LDFLAGS = `pkg-config --libs gtk+-3.0` -lpthread

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

# Tạo thư mục obj và bin nếu chưa có
$(shell mkdir -p $(OBJ_DIR) $(BIN_DIR))

# Biên dịch AES thành thư viện tĩnh
$(OBJ_DIR)/aes.o: $(SRC_DIR)/aes.c include/aes.h
	$(CC) $(CFLAGS) -c $< -o $@

# Biên dịch mã nguồn của client
$(OBJ_DIR)/client.o: $(SRC_DIR)/client.c include/aes.h
	$(CC) $(CFLAGS) -c $< -o $@

# Biên dịch mã nguồn của server
$(OBJ_DIR)/server.o: $(SRC_DIR)/server.c include/aes.h
	$(CC) $(CFLAGS) -c $< -o $@

# Biên dịch encrypt và decrypt
$(OBJ_DIR)/encrypt.o: $(SRC_DIR)/encrypt.c include/aes.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/decrypt.o: $(SRC_DIR)/decrypt.c include/aes.h
	$(CC) $(CFLAGS) -c $< -o $@

# Build client (có GUI)
client: $(OBJ_DIR)/client.o $(OBJ_DIR)/aes.o
	$(CC) $(CFLAGS) $^ -o $(BIN_DIR)/client $(LDFLAGS)

# Build server (có GUI)
server: $(OBJ_DIR)/server.o $(OBJ_DIR)/aes.o
	$(CC) $(CFLAGS) $^ -o $(BIN_DIR)/server $(LDFLAGS)

# Build encrypt
encrypt: $(OBJ_DIR)/encrypt.o $(OBJ_DIR)/aes.o
	$(CC) $(CFLAGS) $^ -o $(BIN_DIR)/encrypt $(LDFLAGS)

# Build decrypt
decrypt: $(OBJ_DIR)/decrypt.o $(OBJ_DIR)/aes.o
	$(CC) $(CFLAGS) $^ -o $(BIN_DIR)/decrypt $(LDFLAGS)

# Build tất cả
all: client server encrypt decrypt

# Dọn dẹp file biên dịch
clean:
	rm -rf $(OBJ_DIR)/*.o $(BIN_DIR)/*

# Chạy chương trình client
run-client: client
	gdb --args ./bin/client

# Chạy chương trình server
run-server: server
	./bin/server
