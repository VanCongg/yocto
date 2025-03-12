#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "../include/aes.h"

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Usage: %s <input_file> <output_file> <key> <key_size>\n", argv[0]);
        return 1;
    }

    const char *input_file = argv[1];
    const char *output_file = argv[2];
    const char *key = argv[3];
    int key_size = atoi(argv[4]);

    if (key_size != 128 && key_size != 192 && key_size != 256) {
        printf("Invalid key size. Use 128, 192, or 256.\n");
        return 1;
    }

    // Mở hoặc tạo tệp đầu ra
    FILE *out = fopen(output_file, "wb");
    if (!out) {
        perror("Failed to create output file");
        return 1;
    }
    fclose(out); // Đóng lại ngay để đảm bảo tệp tồn tại

    AESKeyLength aesKeyLen;
    switch (key_size) {
        case 128: aesKeyLen = AES_128; break;
        case 192: aesKeyLen = AES_192; break;
        case 256: aesKeyLen = AES_256; break;
        default: return 1;
    }

    // Mã hóa file
    int result =aes_encrypt_file(
      (const uint8_t *)input_file, 
      (const uint8_t *)output_file, 
      (const uint8_t *)key, 
      aesKeyLen
    );
    if (result == 0) {
        printf("Encryption successful: %s\n", output_file);
    } else {
        printf("Encryption failed. Error code: %d\n", result);
    }

    return result;
}
