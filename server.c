#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <sys/stat.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

int clients[MAX_CLIENTS]; // Danh sách client kết nối
int client_count = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

// Hàm gửi file đến tất cả client khác
void broadcast_file(const char *filepath, int sender_socket) {
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        perror("Lỗi mở file để gửi");
        return;
    }

    char buffer[BUFFER_SIZE];
    int bytes_read;
    
    // Tách riêng tên file từ đường dẫn
    const char *filename = strrchr(filepath, '/');
    if (filename) filename++; // Bỏ dấu '/'

    // Lấy kích thước file
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    rewind(fp);

    pthread_mutex_lock(&lock);
    for (int i = 0; i < client_count; i++) {
        if (clients[i] != sender_socket) { // Không gửi lại cho client đã gửi
            send(clients[i], filename, strlen(filename) + 1, 0); // Gửi tên file (null-terminated)
            sleep(1); // Tránh lỗi gửi quá nhanh

            send(clients[i], &file_size, sizeof(file_size), 0); // Gửi kích thước file
            sleep(1);

            // Gửi nội dung file
            while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
                send(clients[i], buffer, bytes_read, 0);
            }
        }
    }
    pthread_mutex_unlock(&lock);
    fclose(fp);
}

// Xử lý từng client kết nối
void *handle_client(void *arg) {
    int client_socket = *(int *)arg;
    char filename[BUFFER_SIZE], buffer[BUFFER_SIZE];
    long file_size, received_size = 0;

    // Nhận tên file
    recv(client_socket, filename, BUFFER_SIZE, 0);
    printf("Đang nhận file: %s\n", filename);

    // Nhận kích thước file
    recv(client_socket, &file_size, sizeof(file_size), 0);
    printf("Kích thước file: %ld bytes\n", file_size);

    // Tạo thư mục "saved" nếu chưa có
    mkdir("saved", 0777);
    char filepath[BUFFER_SIZE];
    sprintf(filepath, "saved/%s", filename);

    // Nhận nội dung file và lưu
    FILE *fp = fopen(filepath, "wb");
    if (!fp) {
        perror("Lỗi mở file để ghi");
        close(client_socket);
        return NULL;
    }

    int bytes_received;
    while (received_size < file_size) {
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) break;
        fwrite(buffer, 1, bytes_received, fp);
        received_size += bytes_received;
    }
    fclose(fp);
    printf("Nhận file %s thành công!\n", filename);

    // Gửi file cho các client khác
    broadcast_file(filepath, client_socket);

    close(client_socket);
    return NULL;
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size = sizeof(client_addr);

    // Tạo socket server
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Lỗi tạo socket");
        return 1;
    }

    // Cấu hình server
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Lỗi bind");
        return 1;
    }

    listen(server_socket, MAX_CLIENTS);
    printf("Server đang lắng nghe trên cổng %d...\n", PORT);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size);
        if (client_socket < 0) {
            perror("Lỗi accept");
            continue;
        }

        pthread_mutex_lock(&lock);
        clients[client_count++] = client_socket; // Lưu client vào danh sách
        pthread_mutex_unlock(&lock);

        pthread_t thread;
        pthread_create(&thread, NULL, handle_client, (void *)&client_socket);
        pthread_detach(thread);
    }

    close(server_socket);
    return 0;
}
