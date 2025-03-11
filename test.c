#include <gtk/gtk.h>
#include <gio/gio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <sys/time.h>
#include <string.h>
#include <sys/select.h>
#include <pthread.h>
#include <errno.h>

#define BUFFER_SIZE 4096
int sockfd = -1;
GtkWidget *window_login, *window_main;
GtkWidget *entry_ip, *entry_port, *entry_username;
GtkWidget *window_send_file;
GSocketConnection *client_connection = NULL;
char received_filename[256];
GtkWidget *entry_filename;
GtkWidget *combo_clients;
GtkWidget *entry_filename;
GtkWidget *window_received, *file_list;
GtkWidget *window_decrypt, *entry_key, *combobox_key_size;
char selected_file[256] = "";
void select_file(GtkWidget *widget, gpointer data);
void open_decrypt_window(GtkWidget *widget, gpointer data);
void decrypt_file(GtkWidget *widget, gpointer data);
typedef struct
{
    char filename[256];
    long file_size;
} FileInfo;


void fetch_clients_from_server(int sockfd, GtkWidget *combo_clients)
{
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(combo_clients));
    char request[] = "GET_CLIENTS";
    send(sockfd, request, strlen(request), 0);

    char buffer[BUFFER_SIZE] = {0};
    int bytes_received = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);

    if (bytes_received > 0)
    {
        buffer[bytes_received] = '\0';

        // Tách danh sách client (giả sử mỗi client cách nhau bằng '\n')
        char *token = strtok(buffer, "\n");
        while (token != NULL)
        {
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_clients), token);
            token = strtok(NULL, "\n");
        }
    }
}

void on_open_folder_button_clicked(GtkButton *button, gpointer user_data)
{
    const char *folder_path = "de";
    open_directory(folder_path);
}

// gửi file
void sendfile_back_to_main(GtkWidget *widget, gpointer data)
{
    gtk_widget_hide(window_send_file);
    gtk_widget_show_all(window_main);
}
char *selected_filepath = NULL;
void on_choose_file_clicked(GtkWidget *widget, gpointer data)
{
    GtkWidget *dialog;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
    dialog = gtk_file_chooser_dialog_new("Chọn file", GTK_WINDOW(window_send_file),
                                         action, "_Hủy", GTK_RESPONSE_CANCEL,
                                         "_Chọn", GTK_RESPONSE_ACCEPT, NULL);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    {
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
        if (selected_filepath)
            g_free(selected_filepath);
        selected_filepath = gtk_file_chooser_get_filename(chooser);

        if (entry_filename)
        {
            const char *filename = g_path_get_basename(selected_filepath);
            gtk_entry_set_text(GTK_ENTRY(entry_filename), filename);
            gtk_editable_set_editable(GTK_EDITABLE(entry_filename), FALSE);
            g_free((gpointer)filename);
        }
    }
    gtk_widget_destroy(dialog);
}
void sendfile_to_server(GtkWidget *widget, gpointer data)
{
    if (!selected_filepath)
    {
        g_print("⚠️ Chưa chọn file!\n");
        return;
    }

    const char *new_filename = gtk_entry_get_text(GTK_ENTRY(entry_filename));
    if (strlen(new_filename) == 0)
    {
        g_print("⚠️ Chưa nhập tên file mới!\n");
        return;
    }

    GtkComboBoxText *combo = GTK_COMBO_BOX_TEXT(combo_clients);
    const char *receiver = gtk_combo_box_text_get_active_text(combo);
    if (receiver == NULL)
    {
        g_print("⚠️ Chưa chọn người nhận!\n");
        return;
    }

    FILE *file = fopen(selected_filepath, "rb");
    if (!file)
    {
        g_print("❌ Không thể mở file: %s\n", selected_filepath);
        return;
    }

    // Lấy kích thước file
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char command[512];
    snprintf(command, sizeof(command), "SEND_FILE|%s|%s", new_filename, receiver);
    send(sockfd, command, strlen(command), 0);
    g_print("📤 Đã gửi yêu cầu gửi file: %s đến %s\n", new_filename, receiver);

    send(sockfd, &file_size, sizeof(file_size), 0);

    char buffer[BUFFER_SIZE];
    int bytes_read;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0)
    {
        send(sockfd, buffer, bytes_read, 0);
    }
    fclose(file);
    printf("Đã gửi file: %s\n", new_filename);
    gtk_widget_destroy(window_send_file);
    window_send_file = NULL;
}

// nhận file
void *receive_file_from_server(void *arg)
{
    int sockfd = *(int *)arg;
    if (sockfd < 0)
    {
        perror("❌ Lỗi: Socket không hợp lệ");
        return NULL;
    }
    struct stat st = {0};
    if (stat("received_files", &st) == -1)
        mkdir("received_files", 0700);

    while (1) // 🔴 Vòng lặp chính nhận nhiều file
    {
        char filename[BUFFER_SIZE], buffer[BUFFER_SIZE];
        memset(filename, 0, BUFFER_SIZE);

        // Nhận tên file
        int recv_bytes = recv(sockfd, filename, BUFFER_SIZE, 0);
        if (recv_bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            sleep(1);
            continue;
        }
        else if (recv_bytes <= 0)
        {
            printf("[DEBUG] Kết nối đóng, dừng nhận file.\n");
            break; // 🚨 Thoát vòng lặp ngoài cùng khi kết nối bị đóng
        }

        filename[strcspn(filename, "\n")] = 0;
        printf("\n📥 Nhận file mới: %s\n", filename);
        // Xây dựng đường dẫn file
        char filepath[BUFFER_SIZE];
        snprintf(filepath, sizeof(filepath), "received_files/%s", filename);

        FILE *fp = fopen(filepath, "wb");
        if (!fp)
        {
            perror("Lỗi mở file để lưu");
            continue;
        }
        long filesize;
        int received_bytes = 0;

        while (received_bytes < sizeof(filesize))
        {
            int bytes = recv(sockfd, ((char *)&filesize) + received_bytes, sizeof(filesize) - received_bytes, 0);

            if (bytes == -1)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    printf("[DEBUG] Chưa có dữ liệu kích thước file, chờ tiếp...\n");
                    sleep(1); // Chờ 1 giây trước khi thử lại
                    continue;
                }
                else
                {
                    perror("Lỗi nhận kích thước file");
                    return NULL;
                }
            }
            else if (bytes == 0)
            {
                printf("[DEBUG] Kết nối bị đóng khi nhận kích thước file.\n");
                return NULL;
            }

            received_bytes += bytes;
            printf("[DEBUG] Đã nhận %d bytes kích thước file\n", received_bytes);
        }
        printf("📦 Kích thước file: %ld bytes\n", filesize);

        // Nhận dữ liệu file
        // Nhận dữ liệu file
        long total_received = 0;
        while (total_received < filesize)
        {
            int bytes_received = recv(sockfd, buffer, BUFFER_SIZE, 0);

            if (bytes_received == -1)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    printf("[DEBUG] Chưa có dữ liệu file, chờ tiếp...\n");
                    sleep(1); // Chờ 1 giây rồi thử lại
                    continue;
                }
                else
                {
                    perror("Lỗi nhận dữ liệu file");
                    break;
                }
            }
            else if (bytes_received == 0)
            {
                printf("[DEBUG] Kết nối bị đóng khi nhận file.\n");
                break;
            }

            // Ghi đúng số lượng byte nhận được
            fwrite(buffer, 1, bytes_received, fp);
            total_received += bytes_received;
        }

        if (total_received == filesize)
            printf("✅ File đã được lưu thành công tại: %s\n", filepath);
        else
            printf("⚠️ Cảnh báo: File nhận được bị thiếu dữ liệu (%ld/%ld bytes)\n", total_received, filesize);

        fclose(fp);
    }

    printf("📴 Đóng kết nối socket\n");
    close(sockfd); // Đóng socket khi thoát
    pthread_exit(NULL);
}

// Xử lý đăng nhập
void login(GtkWidget *widget, gpointer data)
{
    const gchar *ip = gtk_entry_get_text(GTK_ENTRY(entry_ip));
    const gchar *port = gtk_entry_get_text(GTK_ENTRY(entry_port));
    const gchar *username = gtk_entry_get_text(GTK_ENTRY(entry_username));

    if (g_strcmp0(username, "") == 0)
    {
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window_login),
                                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   GTK_MESSAGE_WARNING,
                                                   GTK_BUTTONS_OK,
                                                   "Vui lòng nhập tên đăng nhập!");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }

    GSocketClient *client = g_socket_client_new();
    GSocketConnection *connection;
    GError *error = NULL;

    gchar *address = g_strdup_printf("%s:%s", ip, port);
    connection = g_socket_client_connect_to_host(client, address, 5000, NULL, &error);
    g_free(address);
    g_object_unref(client);

    if (connection)
    {
        sockfd = g_socket_get_fd(g_socket_connection_get_socket(connection));
        GOutputStream *ostream = g_io_stream_get_output_stream(G_IO_STREAM(connection));

        // Thêm '\n' để server đọc dễ hơn
        gchar *message = g_strdup_printf("%s", username);
        gssize bytes_written = g_output_stream_write(ostream, message, strlen(message), NULL, &error);
        g_free(message);

        if (bytes_written < 0)
        {
            g_warning("Lỗi gửi username: %s", error->message);
            g_error_free(error);
            g_object_unref(connection);
            return;
        }

        // Lưu connection để sử dụng sau này
        client_connection = connection;

        pthread_t recv_thread;
        pthread_create(&recv_thread, NULL, receive_file_from_server, (void *)&sockfd);
        gtk_widget_hide(window_login);
        open_main_window();
    }
    else
    {
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window_login),
                                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   GTK_MESSAGE_ERROR,
                                                   GTK_BUTTONS_OK,
                                                   "Không thể kết nối đến server!");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }
}


// Hàm main
int main(int argc, char **argv)
{
    create_directories();
    GtkApplication *app = gtk_application_new("com.example.client", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
