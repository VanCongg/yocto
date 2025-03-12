# AES File Encryption Library

## 📌 Giới thiệu
Đây là một thư viện mã hóa tập tin sử dụng **AES (Advanced Encryption Standard)** theo chế độ **CBC (Cipher Block Chaining)**. Thư viện hỗ trợ mã hóa và giải mã với các độ dài khóa **128-bit, 196-bit, và 256-bit**.

## 🔥 Tính năng
- **Mã hóa AES-CBC** với các độ dài khóa **128-bit, 196-bit, 256-bit**.
- **Giải mã AES-CBC** tập tin đã mã hóa.
- **Tự động bổ sung khóa** nếu khóa đầu vào ngắn hơn yêu cầu.
- **Không sử dụng thư viện bên ngoài** (thuần C).

## 📂 Cấu trúc thư mục
```
├── include/        # Chứa file header (.h)
│   ├── aes.h       # Định nghĩa API của thư viện
├── src/            # Chứa mã nguồn của thư viện
│   ├── aes.c       # Triển khai AES
├── encrypt.c       # Chương trình mã hóa file
├── decrypt.c       # Chương trình giải mã file
├── LICENSE         # Giấy phép MIT
├── README.md       # Hướng dẫn sử dụng
└── Makefile        # Tệp build với Makefile
```

## 🚀 Cách sử dụng
### 1️⃣ Biên dịch thư viện
```sh
make
// Optional: make decrypt
// make encrypt
```
Điều này sẽ tạo ra thư viện `libaes.a` trong thư mục `build/`.

### 2️⃣ Mã hóa tập tin
```sh
./encrypt input.txt output.enc my_secret_key 256 
```
- `input.txt`: Tệp đầu vào cần mã hóa
- `output.enc`: Tệp đầu ra đã mã hóa
- `256`: Độ dài khóa AES (128, 196, hoặc 256)
- `my_secret_key`: Khóa bí mật

### 3️⃣ Giải mã tập tin
```sh
./decrypt output.enc decrypted.txt my_secret_key 256
```
- `output.enc`: Tệp đã mã hóa
- `decrypted.txt`: Tệp đầu ra sau khi giải mã
- `256`: Độ dài khóa AES
- `my_secret_key`: Khóa bí mật

## 🛠️ API của Thư viện
### Mã hóa tệp tin
```c
int aes_encrypt_file(const uint8_t *input_file, const uint8_t *output_file, const uint8_t *key, AESKeyLength key_size);

```
### Giải mã tệp tin
```c
int aes_decrypt_file(const uint8_t *input_file, const uint8_t *output_file, const uint8_t *key, AESKeyLength key_size);
```
(Ép kiểu về char nhé)
## 📝 Giấy phép
Dự án này được phát hành theo giấy phép **MIT**. Xem tệp `LICENSE` để biết thêm chi tiết.

## ✨ Đóng góp
Mọi đóng góp đều được hoan nghênh! Hãy tạo một **Pull Request** hoặc mở một **Issue** nếu bạn có ý tưởng cải thiện.

---
✨ **Thanks for reading! By Horob1** 🔐

