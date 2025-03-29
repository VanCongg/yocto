#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include "../include/aes.h"
#define BUFFER_SIZE 4096
#define MAX_CLIENTS 10

// Widgets
GtkWidget *entry_ip, *entry_port, *entry_name, *status_label, *log_view;
GtkWidget *window_main, *window_server;
GtkTextBuffer *log_buffer;
GtkWidget *clear_log_button;
GtkWidget *restart_button;
GtkWidget *window_decrypt;
GtkWidget *entry_filename;
GtkWidget *entry_key;
GtkWidget *comboBox_keysize;
GtkWidget *log_entry;

// Server variables
int server_socket, client_count = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

#define MAX_USERNAME_LENGTH 200
typedef struct
{
    int socket;
    char username[MAX_USERNAME_LENGTH];
} ClientInfo;
ClientInfo clients[MAX_CLIENTS];
// Cập nhật log
void append_log(const char *message)
{
    gtk_entry_set_text(GTK_ENTRY(log_entry), message);
}

void decrypt_file(GtkWidget *widget, gpointer data)
{
    g_print("🔄 Bat dau qua trinh giai ma...\n");

    if (!entry_filename)
    {
        g_print("Error: entry_filename chua duoc khoi tao!\n");
        return;
    }

    const char *selected_filename = gtk_entry_get_text(GTK_ENTRY(entry_filename));
    if (!selected_filename || strlen(selected_filename) == 0)
    {
        g_print("Error: chua chon file de giai ma!\n");
        return;
    }

    char input_filepath[512];
    snprintf(input_filepath, sizeof(input_filepath), "server_en/%s", selected_filename);
    g_print("File cần giải mã: %s\n", input_filepath);

    if (access(input_filepath, F_OK) == -1)
    {
        g_print("Lỗi mở file: %s không tồn tại!\n", input_filepath);
        return;
    }

    if (!entry_key)
    {
        g_print("Lỗi: entry_key chưa được khởi tạo!\n");
        return;
    }

    const char *key = gtk_entry_get_text(GTK_ENTRY(entry_key));
    if (!key || strlen(key) == 0)
    {
        g_print("Lỗi: Chưa nhập key!\n");
        return;
    }
    g_print("Key input: %s\n", key);

    if (!comboBox_keysize)
    {
        g_print("Lỗi: comboBox_keysize chưa được khởi tạo!\n");
        return;
    }

    gint active_index = gtk_combo_box_get_active(GTK_COMBO_BOX(comboBox_keysize));
    if (active_index == -1)
    {
        g_print("Lỗi: Chưa chọn độ dài key!\n");
        return;
    }

    const char *key_size_st = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(comboBox_keysize));
    if (!key_size_st)
    {
        g_print("Lỗi: Không thể lấy độ dài key!\n");
        return;
    }

    AESKeyLength key_size_enum;
    if (strcmp(key_size_st, "128") == 0)
        key_size_enum = AES_128;
    else if (strcmp(key_size_st, "192") == 0)
        key_size_enum = AES_192;
    else if (strcmp(key_size_st, "256") == 0)
        key_size_enum = AES_256;
    else
    {
        g_print("Lỗi: Giá trị key_size không hợp lệ!\n");
        return;
    }
    g_print("Keysize: %d-bit\n", key_size_enum);

    // Đổi phần mở rộng thành .txt sau khi giải mã
    char output_filepath[512];
    strncpy(output_filepath, input_filepath, sizeof(output_filepath));
    output_filepath[sizeof(output_filepath) - 1] = '\0';

    char *dot = strrchr(output_filepath, '.');
    if (dot && strcmp(dot, ".enc") == 0)
    {
        strcpy(dot, ".txt");
    }
    else
    {
        strncat(output_filepath, ".txt", sizeof(output_filepath) - strlen(output_filepath) - 1);
    }
    g_print("File output: %s\n", output_filepath);

    // Gọi hàm giải mã
    g_print("Dang tien hanh giai ma...\n");
    int decrypt_result = aes_decrypt_file((const uint8_t *)input_filepath,
                                          (const uint8_t *)output_filepath,
                                          (const uint8_t *)key,
                                          (AESKeyLength)key_size_enum);
    g_print("Ket qua giai ma: %d\n", decrypt_result);

    if (decrypt_result != 0)
    {
        g_print("Lỗi khi giải mã file!\n");
        return;
    }

    g_print("Giai ma thanh cong: %s\n", output_filepath);
    append_log("Giai ma thanh cong : % s\n ", output_filepath);
    gtk_widget_destroy(window_decrypt);
    ;
    // Mở file sau khi giải mã
    char command[600];
    snprintf(command, sizeof(command), "gio open \"%s\"", output_filepath);
    system(command);
}

void *client_handler(void *arg)
{
    int client_socket = *(int *)arg;
    free(arg);

    char username[BUFFER_SIZE] = {0};
    int bytes_received = recv(client_socket, username, BUFFER_SIZE - 1, 0);
    username[bytes_received] = '\0';
    username[strcspn(username, "\r\n")] = '\0';

    char log_message[200];
    snprintf(log_message, sizeof(log_message), "Client '%s' connect", username);
    append_log(log_message);

    pthread_mutex_lock(&lock);
    if (client_count < MAX_CLIENTS)
    {
        clients[client_count].socket = client_socket;
        strncpy(clients[client_count].username, username, MAX_USERNAME_LENGTH - 1);
        client_count++;
    }
    else
    {
        pthread_mutex_unlock(&lock);
        close(client_socket);
        pthread_exit(NULL);
    }
    pthread_mutex_unlock(&lock);

    while (1)
    {
        char buffer[BUFFER_SIZE] = {0};
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);

        if (bytes_received <= 0)
        {
            snprintf(log_message, sizeof(log_message), "Client '%s' disconnect.", username);
            append_log(log_message);
            break;
        }

        buffer[bytes_received] = '\0';
        char filename[256] = {0};
        if (sscanf(buffer, "SEND_FILE|%255[^|]", filename) == 1)
        {
            // Tạo đường dẫn lưu file
            char filepath[512];
            const char *dir_path = "./server_en";
            // Kiểm tra và tạo thư mục nếu chưa tồn tại
            struct stat st;
            if (stat(dir_path, &st) == -1)
            {
                mkdir(dir_path, 0777); // Tạo thư mục với quyền truy cập đầy đủ
            }
            snprintf(filepath, sizeof(filepath), "./server_en/%s", filename);
            FILE *file = fopen(filepath, "wb");
            if (!file)
            {
                perror("Lỗi mở file để ghi");
                close(client_socket);
                return NULL;
            }

            // Nhận kích thước file
            long file_size = 0;
            if (recv(client_socket, &file_size, sizeof(file_size), 0) <= 0)
            {
                perror("Lỗi nhận kích thước file");
                fclose(file);
                close(client_socket);
                return NULL;
            }
            printf("Dang nhan file '%s', kích thước: %ld bytes\n", filename, file_size);
            // Nhận file theo buffer tối ưu
            size_t total_received = 0;
            while (total_received < file_size)
            {
                int bytes_to_receive = (file_size - total_received) < BUFFER_SIZE ? (file_size - total_received) : BUFFER_SIZE;
                int bytes_received = recv(client_socket, buffer, bytes_to_receive, 0);
                if (bytes_received <= 0)
                {
                    perror("Lỗi nhận dữ liệu file");
                    break;
                }

                fwrite(buffer, 1, bytes_received, file);
                total_received += bytes_received;
            }
            fclose(file);
            printf("Nhan file '%s' thanh cong! (%ld/%ld bytes)\n", filename, total_received, file_size);
            append_log("Da nhan file thanh cong!");
        }
    }
    close(client_socket);
    pthread_exit(NULL);
}

// Luồng chính của server
void *server_thread(void *arg)
{
    const char *ip = gtk_entry_get_text(GTK_ENTRY(entry_ip));
    int port = atoi(gtk_entry_get_text(GTK_ENTRY(entry_port)));

    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size = sizeof(client_addr);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
        append_log("Lỗi tạo socket!");
        return NULL;
    }
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        append_log("Lỗi bind!");
        return NULL;
    }

    listen(server_socket, MAX_CLIENTS);
    append_log("Server dang chay...");

    while (1)
    {
        int *client_socket = malloc(sizeof(int));
        *client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size);
        if (*client_socket < 0)
        {
            free(client_socket);
            continue;
        }
        pthread_t client_thread;
        pthread_create(&client_thread, NULL, client_handler, client_socket);
        if (pthread_create(&client_thread, NULL, client_handler, client_socket) != 0)
        {
            append_log("Lỗi tạo luồng cho client!");
            free(client_socket);
        }
        pthread_detach(client_thread);
    }
}
void on_stop_server(GtkWidget *widget, gpointer data)
{
    if (server_socket > 0)
    {
        append_log("Đang tắt server...");
        pthread_mutex_lock(&lock);
        for (int i = 0; i < client_count; i++)
        {
            close(clients[i].socket);
        }
        client_count = 0;
        pthread_mutex_unlock(&lock);
        close(server_socket);
        server_socket = 0;
    }
    if (window_server)
    {
        gtk_widget_destroy(window_server);
    }
    gtk_widget_show_all(window_main);
}

void decrypt_back_to_main(GtkWidget *widget, gpointer data)
{
    gtk_widget_destroy(window_decrypt); // Đóng cửa sổ giải mã
}
void on_choose_file_decrypt(GtkWidget *widget, gpointer data)
{
    GtkWidget *dialog;
    dialog = gtk_file_chooser_dialog_new("Chọn file để giải mã",
                                         GTK_WINDOW(gtk_widget_get_toplevel(widget)),
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         "_Hủy", GTK_RESPONSE_CANCEL,
                                         "_Chọn", GTK_RESPONSE_ACCEPT,
                                         NULL);

    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), "server_en");
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    {
        char *filepath = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        char *filename = g_path_get_basename(filepath);
        gtk_entry_set_text(GTK_ENTRY(entry_filename), filename);
        g_free(filepath);
        g_free(filename);
    }

    gtk_widget_destroy(dialog);
}
void open_decrypt_window(GtkWidget *widget, gpointer data)
{
    window_decrypt = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window_decrypt), "Giải mã file");
    gtk_window_set_default_size(GTK_WINDOW(window_decrypt), 400, 250);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(window_decrypt), 20);
    gtk_container_add(GTK_CONTAINER(window_decrypt), vbox);

    // Hàng 1: Nút back
    GtkWidget *btn_back = gtk_button_new_with_label("Quay lại");
    g_signal_connect(btn_back, "clicked", G_CALLBACK(decrypt_back_to_main), NULL);
    gtk_widget_set_halign(btn_back, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), btn_back, FALSE, FALSE, 5);

    // Hàng 2: Hiển thị tên file (readonly)
    entry_filename = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_filename), "Chưa chọn file");
    gtk_editable_set_editable(GTK_EDITABLE(entry_filename), FALSE);
    gtk_box_pack_start(GTK_BOX(vbox), entry_filename, FALSE, FALSE, 5);

    // Hàng 3: Field nhập key
    entry_key = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_key), "Nhập key");
    gtk_box_pack_start(GTK_BOX(vbox), entry_key, FALSE, FALSE, 5);

    // Hàng 4: Dropdown chọn độ dài key + nút chọn file
    GtkWidget *hbox_key = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    comboBox_keysize = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(comboBox_keysize), NULL, "128");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(comboBox_keysize), NULL, "192");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(comboBox_keysize), NULL, "256");
    gtk_combo_box_set_active(GTK_COMBO_BOX(comboBox_keysize), 0);
    gtk_box_pack_start(GTK_BOX(hbox_key), comboBox_keysize, TRUE, TRUE, 5);

    GtkWidget *btn_choose_file = gtk_button_new_with_label("Chọn file");
    g_signal_connect(btn_choose_file, "clicked", G_CALLBACK(on_choose_file_decrypt), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_key), btn_choose_file, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox_key, FALSE, FALSE, 5);

    // Hàng 5: Nút giải mã
    GtkWidget *btn_decrypt = gtk_button_new_with_label("Giải mã");
    g_signal_connect(btn_decrypt, "clicked", G_CALLBACK(decrypt_file), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), btn_decrypt, FALSE, FALSE, 5);

    g_signal_connect(window_decrypt, "destroy", G_CALLBACK(gtk_widget_hide), NULL);
    gtk_widget_show_all(window_decrypt);
}
void open_server_window()
{
    // Tạo cửa sổ Server Logs
    window_server = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window_server), "Server Logs");
    gtk_window_set_default_size(GTK_WINDOW(window_server), 400, 300);
    gtk_container_set_border_width(GTK_CONTAINER(window_server), 15);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(window_server), vbox);

    // Nhãn trạng thái
    status_label = gtk_label_new("Server đang chạy...");
    gtk_box_pack_start(GTK_BOX(vbox), status_label, FALSE, FALSE, 5);

    // Ô nhập log thay vì GtkTextView
    log_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(log_entry), "Log hiển thị ở đây...");
    gtk_editable_set_editable(GTK_EDITABLE(log_entry), FALSE); // Không cho phép nhập
    gtk_widget_set_margin_start(log_entry, 20);
    gtk_widget_set_margin_end(log_entry, 20);
    gtk_widget_set_margin_top(log_entry, 10);
    gtk_widget_set_margin_bottom(log_entry, 10);
    gtk_box_pack_start(GTK_BOX(vbox), log_entry, FALSE, FALSE, 0);

    // Hộp chứa các nút (Căn chỉnh theo chiều dọc)
    GtkWidget *vbox_buttons = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(vbox_buttons, 100);
    gtk_widget_set_margin_end(vbox_buttons, 100);
    gtk_widget_set_margin_top(vbox_buttons, 10);
    gtk_widget_set_margin_bottom(vbox_buttons, 10);
    gtk_box_pack_start(GTK_BOX(vbox), vbox_buttons, FALSE, FALSE, 5);

    // Nút "Giải mã" (ở trên)
    GtkWidget *btn_decrypt = gtk_button_new_with_label("Giải mã");
    g_signal_connect(btn_decrypt, "clicked", G_CALLBACK(open_decrypt_window), NULL);
    gtk_box_pack_start(GTK_BOX(vbox_buttons), btn_decrypt, FALSE, FALSE, 5);

    // Nút "Dừng server" (ở dưới)
    GtkWidget *btn_stop_server = gtk_button_new_with_label("Dừng server");
    g_signal_connect(btn_stop_server, "clicked", G_CALLBACK(on_stop_server), NULL);
    gtk_box_pack_start(GTK_BOX(vbox_buttons), btn_stop_server, FALSE, FALSE, 5);

    gtk_widget_show_all(window_server);
}

// Start Server
void start_server(GtkWidget *widget, gpointer data)
{
    pthread_t thread;
    pthread_create(&thread, NULL, server_thread, NULL);
    pthread_detach(thread);

    gtk_widget_hide(window_main);
    open_server_window();
}

// Main GUI
void activate(GtkApplication *app, gpointer user_data)
{
    window_main = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window_main), "Server Setup");
    gtk_window_set_default_size(GTK_WINDOW(window_main), 400, 200);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window_main), vbox);

    // Tạo Grid và thêm padding
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_widget_set_margin_start(grid, 30);
    gtk_widget_set_margin_end(grid, 30);
    gtk_widget_set_margin_top(grid, 20);
    gtk_widget_set_margin_bottom(grid, 20);
    gtk_box_pack_start(GTK_BOX(vbox), grid, TRUE, TRUE, 0);

    // Load ảnh (đổi đường dẫn ảnh của bạn)
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale("resources/logo.png", 80, 80, TRUE, NULL);
    GtkWidget *image = gtk_image_new_from_pixbuf(pixbuf);

    // Ảnh nằm ở dòng 0, chiếm 2 cột
    gtk_grid_attach(GTK_GRID(grid), image, 0, 0, 2, 1);

    // Dòng 1: IP
    GtkWidget *label_ip = gtk_label_new("IP:");
    gtk_grid_attach(GTK_GRID(grid), label_ip, 0, 1, 1, 1);

    entry_ip = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), entry_ip, 1, 1, 1, 1);

    // Dòng 2: Port
    GtkWidget *label_port = gtk_label_new("PORT:");
    gtk_grid_attach(GTK_GRID(grid), label_port, 0, 2, 1, 1);

    entry_port = gtk_entry_new();
    gtk_grid_attach(GTK_GRID(grid), entry_port, 1, 2, 1, 1);

    // Dòng 3: Nút Start Server
    GtkWidget *button = gtk_button_new_with_label("Start Server");
    gtk_grid_attach(GTK_GRID(grid), button, 0, 3, 2, 1);
    g_signal_connect(button, "clicked", G_CALLBACK(start_server), NULL);

    // Căn giữa cửa sổ
    gtk_widget_set_halign(grid, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(grid, GTK_ALIGN_CENTER);

    gtk_widget_show_all(window_main);
}

int main(int argc, char **argv)
{
    GtkApplication *app;
    int status;

    app = gtk_application_new("com.example.ServerGUI", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}