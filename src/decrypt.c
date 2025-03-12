#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "../include/aes.h"

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <input_file> <output_file> <key> <key_size>\n", argv[0]);
        return 1;
    }

    const char *input_file = argv[1];
    const char *output_file = argv[2];
    const char *key = argv[3];
    int key_size = atoi(argv[4]);

    AESKeyLength aes_key_size;
    switch (key_size) {
        case 128: aes_key_size = AES_128; break;
        case 192: aes_key_size = AES_192; break;
        case 256: aes_key_size = AES_256; break;
        default:
            fprintf(stderr, "Invalid key size. Use 128, 192, or 256.\n");
            return 1;
    }

    if (aes_decrypt_file(
      (const uint8_t *)input_file, 
      (const uint8_t *)output_file, 
      (const uint8_t *)key,
      aes_key_size) == 0) {
        printf("Decryption successful: %s -> %s\n", input_file, output_file);
    } else {
        fprintf(stderr, "Decryption failed.\n");
    }

    return 0;
}
