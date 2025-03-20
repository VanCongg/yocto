#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "../include/aes.h"
#include <unistd.h> // Cho ftruncate() trên Linux/macOS
// #include <io.h>      // Cho _chsize_s() trên Windows
/**
 * This section contains the AES S-Box and Inverse S-Box
 * The S-Box is used in the encryption process to substitute bytes
 * The Inverse S-Box is used in the decryption process to substitute bytes
 */

static const uint8_t Sbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16};

static const uint8_t InvSbox[256] = {
    0x52, 0x09, 0x6A, 0xD5, 0x30, 0x36, 0xA5, 0x38, 0xBF, 0x40, 0xA3, 0x9E, 0x81, 0xF3, 0xD7, 0xFB,
    0x7C, 0xE3, 0x39, 0x82, 0x9B, 0x2F, 0xFF, 0x87, 0x34, 0x8E, 0x43, 0x44, 0xC4, 0xDE, 0xE9, 0xCB,
    0x54, 0x7B, 0x94, 0x32, 0xA6, 0xC2, 0x23, 0x3D, 0xEE, 0x4C, 0x95, 0x0B, 0x42, 0xFA, 0xC3, 0x4E,
    0x08, 0x2E, 0xA1, 0x66, 0x28, 0xD9, 0x24, 0xB2, 0x76, 0x5B, 0xA2, 0x49, 0x6D, 0x8B, 0xD1, 0x25,
    0x72, 0xF8, 0xF6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xD4, 0xA4, 0x5C, 0xCC, 0x5D, 0x65, 0xB6, 0x92,
    0x6C, 0x70, 0x48, 0x50, 0xFD, 0xED, 0xB9, 0xDA, 0x5E, 0x15, 0x46, 0x57, 0xA7, 0x8D, 0x9D, 0x84,
    0x90, 0xD8, 0xAB, 0x00, 0x8C, 0xBC, 0xD3, 0x0A, 0xF7, 0xE4, 0x58, 0x05, 0xB8, 0xB3, 0x45, 0x06,
    0xD0, 0x2C, 0x1E, 0x8F, 0xCA, 0x3F, 0x0F, 0x02, 0xC1, 0xAF, 0xBD, 0x03, 0x01, 0x13, 0x8A, 0x6B,
    0x3A, 0x91, 0x11, 0x41, 0x4F, 0x67, 0xDC, 0xEA, 0x97, 0xF2, 0xCF, 0xCE, 0xF0, 0xB4, 0xE6, 0x73,
    0x96, 0xAC, 0x74, 0x22, 0xE7, 0xAD, 0x35, 0x85, 0xE2, 0xF9, 0x37, 0xE8, 0x1C, 0x75, 0xDF, 0x6E,
    0x47, 0xF1, 0x1A, 0x71, 0x1D, 0x29, 0xC5, 0x89, 0x6F, 0xB7, 0x62, 0x0E, 0xAA, 0x18, 0xBE, 0x1B,
    0xFC, 0x56, 0x3E, 0x4B, 0xC6, 0xD2, 0x79, 0x20, 0x9A, 0xDB, 0xC0, 0xFE, 0x78, 0xCD, 0x5A, 0xF4,
    0x1F, 0xDD, 0xA8, 0x33, 0x88, 0x07, 0xC7, 0x31, 0xB1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xEC, 0x5F,
    0x60, 0x51, 0x7F, 0xA9, 0x19, 0xB5, 0x4A, 0x0D, 0x2D, 0xE5, 0x7A, 0x9F, 0x93, 0xC9, 0x9C, 0xEF,
    0xA0, 0xE0, 0x3B, 0x4D, 0xAE, 0x2A, 0xF5, 0xB0, 0xC8, 0xEB, 0xBB, 0x3C, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2B, 0x04, 0x7E, 0xBA, 0x77, 0xD6, 0x26, 0xE1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0C, 0x7D};

/**
 * Normalize the key to the target length
 * - If the key is too long, truncate it
 * - If the key is too short, pad it with 0x00
 * @param key The key to normalize
 * @param keyLen The original length of the key
 * @param targetLen The target length (AES_128, AES_192, or AES_256)
 */
void NormalizeKey(uint8_t *key, int keyLen, AESKeyLength targetLen)
{
    if (keyLen > (int)targetLen)
    {
        // Cắt bớt khóa nếu dài hơn targetLen
        memset(key + (int)targetLen, 0, keyLen - (int)targetLen);
    }
    else if (keyLen < (int)targetLen)
    {
        // Padding với 0x00 nếu ngắn hơn targetLen
        memset(key + keyLen, 0, (int)targetLen - keyLen);
    }
}

/**
 * Substitute bytes in the state using the S-Box
 * @param state The state to substitute
 */
void SubBytes(uint8_t *state)
{
    for (int i = 0; i < 16; i++)
    {
        state[i] = Sbox[state[i]];
    }
}

/**
 * Invert the SubBytes operation by substituting bytes in the state using the Inverse S-Box
 * @param state The state to substitute
 */
void InvSubBytes(uint8_t *state)
{
    for (int i = 0; i < 16; i++)
    {
        state[i] = InvSbox[state[i]];
    }
}

/**
 * Shift the rows of the state
 * @param state The state to shift
 */
void ShiftRows(uint8_t *state)
{
    uint8_t tmp[16];
    for (int i = 0; i < 16; i++)
    {
        tmp[i] = state[i];
    }

    state[0] = tmp[0];
    state[1] = tmp[5];
    state[2] = tmp[10];
    state[3] = tmp[15];

    state[4] = tmp[4];
    state[5] = tmp[9];
    state[6] = tmp[14];
    state[7] = tmp[3];

    state[8] = tmp[8];
    state[9] = tmp[13];
    state[10] = tmp[2];
    state[11] = tmp[7];

    state[12] = tmp[12];
    state[13] = tmp[1];
    state[14] = tmp[6];
    state[15] = tmp[11];
}

/**
 * Invert the ShiftRows operation by shifting the rows of the state in the opposite direction
 * @param state The state to shift
 */
void InvShiftRows(uint8_t *state)
{
    uint8_t tmp[16];
    for (int i = 0; i < 16; i++)
    {
        tmp[i] = state[i];
    }

    state[0] = tmp[0];
    state[1] = tmp[13];
    state[2] = tmp[10];
    state[3] = tmp[7];

    state[4] = tmp[4];
    state[5] = tmp[1];
    state[6] = tmp[14];
    state[7] = tmp[11];

    state[8] = tmp[8];
    state[9] = tmp[5];
    state[10] = tmp[2];
    state[11] = tmp[15];

    state[12] = tmp[12];
    state[13] = tmp[9];
    state[14] = tmp[6];
    state[15] = tmp[3];
}

/**
 * Multiply two numbers in the Galois Field with "Russian Peasant Multiplication" algorithm
 * @param a The first number
 * @param b The second number
 * @return The product of a and b in the Galois Field
 */
uint8_t gmul(uint8_t a, uint8_t b)
{
    uint8_t p = 0;
    for (int i = 0; i < 8; i++)
    {
        if (b & 1)
        {
            p ^= a;
        }
        uint8_t hi_bit_set = (a & 0x80);
        a <<= 1;
        if (hi_bit_set)
        {
            a ^= 0x1b; // XOR với đa thức sinh của AES
        }
        b >>= 1;
    }
    return p;
}

/**
 * Mix the columns of the state
 * @param state The state to mix
 */
void MixColumn(uint8_t *state)
{
    for (int i = 0; i < 4; i++)
    {
        uint8_t col[4];
        for (int j = 0; j < 4; j++)
        {
            col[j] = state[j * 4 + i];
        }

        state[0 * 4 + i] = gmul(col[0], 2) ^ gmul(col[1], 3) ^ col[2] ^ col[3];
        state[1 * 4 + i] = col[0] ^ gmul(col[1], 2) ^ gmul(col[2], 3) ^ col[3];
        state[2 * 4 + i] = col[0] ^ col[1] ^ gmul(col[2], 2) ^ gmul(col[3], 3);
        state[3 * 4 + i] = gmul(col[0], 3) ^ col[1] ^ col[2] ^ gmul(col[3], 2);
    }
}

/**
 * Invert the MixColumns operation by mixing the columns of the state in the opposite direction
 * @param state The state to mix
 */
void InvMixColumn(uint8_t *state)
{
    for (int i = 0; i < 4; i++)
    {
        uint8_t col[4];
        for (int j = 0; j < 4; j++)
        {
            col[j] = state[j * 4 + i];
        }

        state[0 * 4 + i] = gmul(col[0], 0x0E) ^ gmul(col[1], 0x0B) ^ gmul(col[2], 0x0D) ^ gmul(col[3], 0x09);
        state[1 * 4 + i] = gmul(col[0], 0x09) ^ gmul(col[1], 0x0E) ^ gmul(col[2], 0x0B) ^ gmul(col[3], 0x0D);
        state[2 * 4 + i] = gmul(col[0], 0x0D) ^ gmul(col[1], 0x09) ^ gmul(col[2], 0x0E) ^ gmul(col[3], 0x0B);
        state[3 * 4 + i] = gmul(col[0], 0x0B) ^ gmul(col[1], 0x0D) ^ gmul(col[2], 0x09) ^ gmul(col[3], 0x0E);
    }
}

/**
 * Add the round key to the state
 * @param state The state to add the round key to
 * @param roundKey The round key to add
 */
void AddRoundKey(uint8_t *state, uint8_t *roundKey)
{
    for (int i = 0; i < 16; i++)
    {
        state[i] ^= roundKey[i];
    }
}

/**
 * Rotate the word by one byte
 * @param state The state to rotate
 */
void RotWord(uint8_t *word)
{
    uint8_t tmp = word[0];
    word[0] = word[1];
    word[1] = word[2];
    word[2] = word[3];
    word[3] = tmp;
}

/**
 * Perform the key expansion to generate the round keys
 * @param inputKey The input key
 * @param expandedKeys The expanded keys
 * @param targetLen The target length of the key (AES_128, AES_192, or AES_256)
 */
void KeyExpansion(uint8_t *inputKey, uint8_t *expandedKeys, AESKeyLength targetLen)
{
    int Nk = targetLen / 4;
    int Nr = Nk + 6;
    int Nb = 4;

    int i = 0;
    uint8_t temp[4];
    uint8_t Rcon[4] = {0x01, 0x00, 0x00, 0x00};

    for (i = 0; i < Nk; i++)
    {
        expandedKeys[(i * 4) + 0] = inputKey[(i * 4) + 0];
        expandedKeys[(i * 4) + 1] = inputKey[(i * 4) + 1];
        expandedKeys[(i * 4) + 2] = inputKey[(i * 4) + 2];
        expandedKeys[(i * 4) + 3] = inputKey[(i * 4) + 3];
    }

    for (i = Nk; i < Nb * (Nr + 1); i++)
    {
        // **Lấy 4 byte cuối cùng của expandedKeys**
        temp[0] = expandedKeys[(i - 1) * 4 + 0];
        temp[1] = expandedKeys[(i - 1) * 4 + 1];
        temp[2] = expandedKeys[(i - 1) * 4 + 2];
        temp[3] = expandedKeys[(i - 1) * 4 + 3];

        // **Xử lý Rcon khi i chia hết cho Nk**
        if (i % Nk == 0)
        {
            RotWord(temp);
            // substitute bytes
            temp[0] = Sbox[temp[0]];
            temp[1] = Sbox[temp[1]];
            temp[2] = Sbox[temp[2]];
            temp[3] = Sbox[temp[3]];

            // XOR với Rcon
            temp[0] = temp[0] ^ Rcon[0];

            // Cập nhật Rcon
            Rcon[0] = (Rcon[0] << 1) ^ ((Rcon[0] & 0x80) ? 0x1B : 0x00);
        }

        else if (Nk > 6 && i % Nk == 4)
        {
            temp[0] = Sbox[temp[0]];
            temp[1] = Sbox[temp[1]];
            temp[2] = Sbox[temp[2]];
            temp[3] = Sbox[temp[3]];
        }

        // **Cập nhật expandedKeys**
        expandedKeys[i * 4 + 0] = expandedKeys[(i - Nk) * 4 + 0] ^ temp[0];
        expandedKeys[i * 4 + 1] = expandedKeys[(i - Nk) * 4 + 1] ^ temp[1];
        expandedKeys[i * 4 + 2] = expandedKeys[(i - Nk) * 4 + 2] ^ temp[2];
        expandedKeys[i * 4 + 3] = expandedKeys[(i - Nk) * 4 + 3] ^ temp[3];
    }
};

/**
 * Encrypt a block of data using AES
 * @param block The block of data to encrypt
 * @param expandedKeys The expanded keys
 * @param keySize The size of the key (AES_128, AES_192, or AES_256)
 */
void AES_EncryptBlock(uint8_t *block, uint8_t *expandedKeys, AESKeyLength keySize)
{
    int Nk = keySize / 4;
    int Nr = Nk + 6;
    int Nb = 4;

    AddRoundKey(block, expandedKeys);

    for (int round = 1; round < Nr; round++)
    {
        SubBytes(block);
        ShiftRows(block);
        MixColumn(block);
        AddRoundKey(block, expandedKeys + round * Nb * 4);
    }

    SubBytes(block);
    ShiftRows(block);
    AddRoundKey(block, expandedKeys + Nr * Nb * 4);
}

/**
 * Decrypt a block of data using AES
 * @param block The block of data to decrypt
 * @param expandedKeys The expanded keys
 * @param keySize The size of the key (AES_128, AES_192, or AES_256)
 */
void AES_DecryptBlock(uint8_t *block, uint8_t *expandedKeys, AESKeyLength keySize)
{
    int Nk = keySize / 4;
    int Nr = Nk + 6;
    int Nb = 4;

    AddRoundKey(block, expandedKeys + Nr * Nb * 4);

    for (int round = Nr - 1; round > 0; round--)
    {
        InvShiftRows(block);
        InvSubBytes(block);
        AddRoundKey(block, expandedKeys + round * Nb * 4);
        InvMixColumn(block);
    }

    InvShiftRows(block);
    InvSubBytes(block);
    AddRoundKey(block, expandedKeys);
}

/**
 * Encrypt a file using AES
 * @param input_file The input file to encrypt
 * @param output_file The output file to write the encrypted data
 * @param key The key to use for encryption
 * @param key_size The size of the key (AES_128, AES_192, or AES_256)
 * @return 0 if the encryption is successful, -1 otherwise
 */
int aes_encrypt_file(const uint8_t *input_file, const uint8_t *output_file, const uint8_t *key, AESKeyLength key_size)
{
    uint8_t expandedKey[240];
    uint8_t normalizedKey[32];
    memcpy(normalizedKey, key, key_size);
    NormalizeKey(normalizedKey, key_size, key_size);
    KeyExpansion(normalizedKey, expandedKey, key_size);

    FILE *in = fopen((const char *)input_file, "rb");
    FILE *out = fopen((const char *)output_file, "wb");
    if (in == NULL || out == NULL)
    {
        if (out)
            fclose(out);
        return -1;
    }

    uint8_t iv[16];
    srand(time(NULL));
    for (int i = 0; i < 16; i++)
        iv[i] = rand() % 256;
    fwrite(iv, 1, 16, out);

    uint8_t block[16];
    size_t bytesRead;
    while ((bytesRead = fread(block, 1, 16, in)) == 16)
    {
        for (int i = 0; i < 16; i++)
            block[i] ^= iv[i];
        AES_EncryptBlock(block, expandedKey, key_size);
        fwrite(block, 1, 16, out);
        memcpy(iv, block, 16);
    }

    // Thêm padding chuẩn PKCS#7 nếu cần
    uint8_t padding = 16 - bytesRead;
    for (size_t i = bytesRead; i < 16; i++)
    {
        block[i] = padding;
    }
    for (int i = 0; i < 16; i++)
        block[i] ^= iv[i];
    AES_EncryptBlock(block, expandedKey, key_size);
    fwrite(block, 1, 16, out);

    fclose(in);
    fclose(out);
    return 0;
}

/**
 * Decrypt a file using AES
 * @param input_file The input file to decrypt
 * @param output_file The output file to write the decrypted data
 * @param key The key to use for decryption
 * @param key_size The size of the key (AES_128, AES_192, or AES_256)
 * @return 0 if the decryption is successful, -1 otherwise
 */
int aes_decrypt_file(const uint8_t *input_file, const uint8_t *output_file, const uint8_t *key, AESKeyLength key_size)
{
    printf("🔄 Bắt đầu giải mã file: %s\n", input_file);

    uint8_t expandedKey[240];
    uint8_t normalizedKey[32];

    // Chuẩn hóa khóa
    memcpy(normalizedKey, key, key_size);
    printf("🔑 Key nhập vào: %s (size: %d)\n", key, key_size);

    NormalizeKey(normalizedKey, key_size, key_size);
    KeyExpansion(normalizedKey, expandedKey, key_size);

    // Mở file
    FILE *in = fopen((const char *)input_file, "rb");
    FILE *out = fopen((const char *)output_file, "wb");

    if (in == NULL)
    {
        perror("❌ Lỗi mở file đầu vào");
        return -1;
    }
    if (out == NULL)
    {
        perror("❌ Lỗi mở file đầu ra");
        fclose(in);
        return -1;
    }

    // Đọc IV từ đầu file mã hóa
    uint8_t iv[16];
    size_t iv_read = fread(iv, 1, 16, in);
    if (iv_read != 16)
    {
        printf("❌ Lỗi đọc IV! Chỉ đọc được %zu byte\n", iv_read);
        fclose(in);
        fclose(out);
        return -1;
    }
    printf("🛠️ IV đọc được: ");
    for (int i = 0; i < 16; i++)
    {
        printf("%02X ", iv[i]);
    }
    printf("\n");

    uint8_t block[16], prevCipher[16];

    size_t bytesRead;
    while ((bytesRead = fread(block, 1, 16, in)) == 16)
    {
        memcpy(prevCipher, block, 16); // Lưu ciphertext trước khi giải mã

        AES_DecryptBlock(block, expandedKey, key_size); // Giải mã AES

        // XOR với IV để khôi phục plaintext
        for (int i = 0; i < 16; i++)
        {
            block[i] ^= iv[i];
        }

        fwrite(block, 1, 16, out); // Ghi dữ liệu đã giải mã ra file

        memcpy(iv, prevCipher, 16); // Cập nhật IV từ ciphertext gốc
    }

    // Kiểm tra padding
    printf("📌 Kiểm tra padding...\n");
    int i = 15;
    while (block[i] != ' ' && i >= 0)
    {
        i--;
    }
    if (i < 0)
    {
        printf("⚠️ Không tìm thấy padding hợp lệ!\n");
    }
    else
    {
        size_t bytesToTruncate = 16 - i;
        printf("🛠️ Cắt bỏ %zu byte padding\n", bytesToTruncate);
        ftruncate(fileno(out), ftell(out) - bytesToTruncate);
    }

    fclose(in);
    fclose(out);
    printf("✅ Giải mã hoàn tất!\n");
    return 0;
}
