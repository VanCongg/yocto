#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>

#define SERVER_IP "192.168.1.14"
#define PORT 8080
#define BUFFER_SIZE 1024

void *receive_file(void *arg) {
    int sockfd = *(int *)arg;
    char filename[BUFFER_SIZE], buffer[BUFFER_SIZE];

    // Tạo thư mục 'saved/' nếu chưa có
    struct stat st = {0};
    if (stat("saved", &st) == -1) {
        mkdir("saved", 0700);
    }

    while (1) {
        // Nhận tên file
        memset(filename, 0, BUFFER_SIZE);
        int recv_bytes = recv(sockfd, filename, BUFFER_SIZE, 0);
        if (recv_bytes <= 0) break;

        filename[strcspn(filename, "\n")] = 0;
        printf("\n📥 Nhận file mới: %s\n", filename);

        // Thêm đường dẫn `saved/` trước tên file
        char filepath[BUFFER_SIZE];
        snprintf(filepath, sizeof(filepath), "saved/%s", filename);

        FILE *fp = fopen(filepath, "wb");
        if (!fp) {
            perror("Lỗi mở file để lưu");
            continue;
        }
        // Nhận kích thước file
        long filesize;
        recv(sockfd, &filesize, sizeof(filesize), 0);
        printf("📦 Kích thước file: %ld bytes\n", filesize);

        // Nhận dữ liệu file theo kích thước
        long total_received = 0;
        int bytes_received;
        while (total_received < filesize) {
            bytes_received = recv(sockfd, buffer, BUFFER_SIZE, 0);
            if (bytes_received <= 0) break;
            fwrite(buffer, 1, bytes_received, fp);
            total_received += bytes_received;
        }
        fclose(fp);
        printf("✅ File đã được lưu thành công tại: %s\n", filepath);
    }

    return NULL;
}

int main() {
    int sockfd;
    struct sockaddr_in server_addr;

    // Tạo socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Lỗi tạo socket");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    // Kết nối đến server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Lỗi kết nối server");
        return 1;
    }

    printf("🔗 Kết nối đến server thành công!\n");

    // Tạo luồng nhận file từ server
    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receive_file, (void *)&sockfd);

    while (1) {
        char filename[BUFFER_SIZE];
        printf("\nNhập tên file cần gửi: ");
        scanf("%s", filename);

        // Mở file đọc dữ liệu
        FILE *fp = fopen(filename, "rb");
        if (!fp) {
            perror("Lỗi mở file");
            continue;
        }

        // Lấy kích thước file
        fseek(fp, 0, SEEK_END);
        long filesize = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        send(sockfd, filename, strlen(filename), 0); // Gửi tên file
        sleep(1);

        send(sockfd, &filesize, sizeof(filesize), 0); // Gửi kích thước file

        char buffer[BUFFER_SIZE];
        int bytes_read;
        while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
            send(sockfd, buffer, bytes_read, 0);
        }

        fclose(fp);
        printf("📤 Đã gửi file: %s\n", filename);
    }

    close(sockfd);
    return 0;
}
