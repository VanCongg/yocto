#ifndef AES_H
#define AES_H

typedef enum
{
  AES_128 = 16,
  AES_192 = 24,
  AES_256 = 32
} AESKeyLength;

int aes_encrypt_file(const uint8_t *input_file, const uint8_t *output_file, const uint8_t *key, AESKeyLength key_size);
int aes_decrypt_file(const uint8_t *input_file, const uint8_t *output_file, const uint8_t *key, AESKeyLength key_size);

#endif