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
#include <limits.h>
#include "../include/aes.h"

#define BUFFER_SIZE 4096
GtkWidget *window_login, *window_main;
GtkWidget *entry_ip, *entry_port, *entry_username;
GtkWidget *window_send_file;
GSocketConnection *client_connection = NULL;
char received_filename[256];
int sockfd;
GtkWidget *entry_filename;
GtkWidget *combo_clients;
GtkWidget *entry_filename;
GtkWidget *window_received, *file_list;
GtkWidget *window_decrypt, *entry_key, *combobox_key_size;
GtkWidget *combo_keysize;
GtkWidget *entry_key;

char selected_file[256] = "";
void select_file(GtkWidget *widget, gpointer data);
void open_decrypt_window(GtkWidget *widget, gpointer data);
void decrypt_file(GtkWidget *widget, gpointer data);
typedef struct
{
    char filename[256];
    long file_size;
} FileInfo;

gboolean check_server_connection(const gchar *ip, const gchar *port)
{
    GSocketClient *client = g_socket_client_new();
    GSocketConnection *connection;
    GError *error = NULL;

    gchar *address = g_strdup_printf("%s:%s", ip, port);
    connection = g_socket_client_connect_to_host(client, address, 5000, NULL, &error);

    g_free(address);
    g_object_unref(client);

    if (connection)
    {
        g_object_unref(connection);
        return TRUE;
    }
    else
    {
        if (error)
        {
            g_warning("Lỗi kết nối: %s", error->message);
            g_error_free(error);
        }
        return FALSE;
    }
}
void copy_file(const char *source_path, const char *dest_dir)
{
    char dest_path[512];
    const char *filename = strrchr(source_path, '/');
    if (!filename)
    {
        filename = source_path;
    }
    else
    {
        filename++;
    }
    snprintf(dest_path, sizeof(dest_path), "%s/%s", dest_dir, filename);
    FILE *source = fopen(source_path, "rb");
    if (!source)
    {
        perror("Lỗi mở file nguồn");
        return;
    }
    FILE *dest = fopen(dest_path, "wb");
    if (!dest)
    {
        perror("Lỗi mở file đích");
        fclose(source);
        return;
    }
    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), source)) > 0)
    {
        fwrite(buffer, 1, bytes, dest);
    }
    fclose(source);
    fclose(dest);
}

void logout(GtkWidget *widget, gpointer data)
{
    int *sockfd = (int *)data; // Lấy socket từ data truyền vào

    if (sockfd && *sockfd > 0)
    {
        char message[] = "LOGOUT";
        if (send(*sockfd, message, strlen(message), 0) == -1)
        {
            perror("send failed");
        }

        shutdown(*sockfd, SHUT_WR); // Đóng ghi trước
        sleep(1);                   // Chờ dữ liệu gửi đi trước khi đóng hoàn toàn
        close(*sockfd);
        *sockfd = -1; // Đánh dấu socket đã bị đóng
    }

    gtk_widget_hide(window_main);
    gtk_widget_show_all(window_login);
}
void create_directories()
{
    struct stat st = {0};
    if (stat("en", &st) == -1)
    {
        mkdir("en", 0700);
        printf("Đã tạo thư mục 'en'\n");
    }
    if (stat("de", &st) == -1)
    {
        mkdir("de", 0700);
        printf("Đã tạo thư mục 'de'\n");
    }
    if (stat("received_files", &st) == -1)
    {
        mkdir("received_files", 0700);
        printf("Đã tạo thư mục 'received_files'\n");
    }
}

void open_directory(const char *folder_name)
{
    char *current_dir = g_get_current_dir();
    char *folder_path = g_build_filename(current_dir, folder_name, NULL);
    char *uri = g_filename_to_uri(folder_path, NULL, NULL);
    GError *error = NULL;
    if (!g_app_info_launch_default_for_uri(uri, NULL, &error))
    {
        g_warning("Không thể mở thư mục: %s", error->message);
        g_error_free(error);
    }
    g_free(current_dir);
    g_free(folder_path);
    g_free(uri);
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

    GtkEntry *entry_receiver = GTK_ENTRY(data);
    const char *receiver = gtk_entry_get_text(entry_receiver);
    if (strlen(receiver) == 0)
    {
        g_print("⚠️ Chưa nhập tên người nhận!\n");
        return;
    }

    // Lấy key từ entry_key
    const char *key = gtk_entry_get_text(GTK_ENTRY(entry_key));
    if (strlen(key) == 0)
    {
        g_print("⚠️ Chưa nhập key mã hóa!\n");
        return;
    }

    // Lấy độ dài key từ combo_keysize
    int key_size;
    const char *key_size_str = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo_keysize));
    if (key_size_str)
    {
        key_size = atoi(key_size_str);
    }
    else
    {
        g_print("⚠️ Lỗi khi lấy độ dài key!\n");
        return;
    }
    g_print("🔍 File nguồn: %s\n", selected_filepath);
    g_print("🔍 Tên file mới: %s\n", new_filename);
    g_print("🔍 Người nhận: %s\n", receiver);
    g_print("🔍 Key: %s\n", key);
    g_print("🔍 Key Size: %d\n", key_size);

    // --- XỬ LÝ TÊN FILE ---
    char filename_no_ext[PATH_MAX];
    strncpy(filename_no_ext, selected_filepath, sizeof(filename_no_ext) - 1);
    filename_no_ext[sizeof(filename_no_ext) - 1] = '\0';

    char *dot = strrchr(filename_no_ext, '.');
    if (dot)
    {
        *dot = '\0'; // Loại bỏ phần mở rộng
    }

    // --- TẠO THƯ MỤC 'en/' (NẾU CHƯA TỒN TẠI) ---
    struct stat st = {0};
    if (stat("en", &st) == -1)
    {
        mkdir("en", 0700);
    }

    // --- TẠO TÊN FILE MÃ HÓA ---
    char encrypted_file[PATH_MAX];
    snprintf(encrypted_file, sizeof(encrypted_file), "en/%s.enc", filename_no_ext);

    // --- MÃ HÓA FILE ---
    int result = aes_encrypt_file((const uint8_t *)selected_filepath,
                                  (const uint8_t *)encrypted_file,
                                  (const uint8_t *)key, (AESKeyLength)key_size);

    if (result != 0)
    {
        g_print("❌ Lỗi khi mã hóa file!\n");
        return;
    }
    g_print("🔒 File đã mã hóa: %s\n", encrypted_file);

    // --- MỞ FILE MÃ HÓA ĐỂ GỬI ---
    FILE *file = fopen(encrypted_file, "rb");
    if (!file)
    {
        g_print("❌ Không thể mở file đã mã hóa: %s\n", encrypted_file);
        return;
    }

    // Lấy kích thước file
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // --- GỬI THÔNG TIN FILE ---
    char command[512];
    snprintf(command, sizeof(command), "SEND_FILE|%s|%s", new_filename, receiver);
    send(sockfd, command, strlen(command), 0);
    g_print("📤 Đã gửi yêu cầu gửi file: %s đến %s\n", new_filename, receiver);

    send(sockfd, &file_size, sizeof(file_size), 0);

    // --- GỬI NỘI DUNG FILE ---
    char buffer[BUFFER_SIZE];
    int bytes_read;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0)
    {
        send(sockfd, buffer, bytes_read, 0);
    }
    fclose(file);
    g_print("✅ Đã gửi file đã mã hóa: %s\n", encrypted_file);

    // Đóng cửa sổ gửi file
    gtk_widget_destroy(window_send_file);
    window_send_file = NULL;
}

void open_send_file_window(GtkWidget *widget, gpointer data)
{
    window_send_file = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window_send_file), "Gửi file");
    gtk_window_set_default_size(GTK_WINDOW(window_send_file), 400, 300);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(window_send_file), 20);
    gtk_container_add(GTK_CONTAINER(window_send_file), vbox);

    // Hàng 1: Nút back
    GtkWidget *btn_back = gtk_button_new_with_label("Quay lại");
    g_signal_connect(btn_back, "clicked", G_CALLBACK(sendfile_back_to_main), NULL);
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
    combo_keysize = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo_keysize), NULL, "128");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo_keysize), NULL, "192");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo_keysize), NULL, "256");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo_keysize), 0);
    gtk_box_pack_start(GTK_BOX(hbox_key), combo_keysize, TRUE, TRUE, 5);

    GtkWidget *btn_choose_file = gtk_button_new_with_label("Chọn file");
    g_signal_connect(btn_choose_file, "clicked", G_CALLBACK(on_choose_file_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_key), btn_choose_file, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox_key, FALSE, FALSE, 5);

    GtkWidget *entry_receiver = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_receiver), "Nhập tên người nhận");
    gtk_box_pack_start(GTK_BOX(vbox), entry_receiver, FALSE, FALSE, 5);
    // Hàng 6: Nút gửi
    GtkWidget *btn_send = gtk_button_new_with_label("Gửi");
    g_signal_connect(btn_send, "clicked", G_CALLBACK(sendfile_to_server), entry_receiver);
    gtk_box_pack_start(GTK_BOX(vbox), btn_send, FALSE, FALSE, 5);

    g_signal_connect(window_send_file, "destroy", G_CALLBACK(gtk_widget_hide), NULL);
    gtk_widget_show_all(window_send_file);
}

// Mở cửa sổ danh sách file nhận
void open_received_file_window(GtkWidget *widget, gpointer data)
{
    window_received = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window_received), "File nhận");
    gtk_window_set_default_size(GTK_WINDOW(window_received), 400, 300);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(window_received), 15);
    gtk_container_add(GTK_CONTAINER(window_received), vbox);

    // Danh sách file
    file_list = gtk_list_box_new();
    gtk_box_pack_start(GTK_BOX(vbox), file_list, TRUE, TRUE, 5);

    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir("received_files")) != NULL)
    {
        while ((ent = readdir(dir)) != NULL)
        {
            if (ent->d_name[0] != '.')
            { // Bỏ qua file ẩn
                GtkWidget *row = gtk_button_new_with_label(ent->d_name);
                g_signal_connect(row, "clicked", G_CALLBACK(select_file), ent->d_name);
                gtk_list_box_insert(GTK_LIST_BOX(file_list), row, -1);
            }
        }
        closedir(dir);
    }

    GtkWidget *btn_decrypt = gtk_button_new_with_label("Giải mã");
    gtk_widget_set_size_request(btn_decrypt, 250, 50);
    g_signal_connect(btn_decrypt, "clicked", G_CALLBACK(open_decrypt_window), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), btn_decrypt, FALSE, FALSE, 5);

    gtk_widget_show_all(window_received);
}

// Chọn file từ danh sách
void select_file(GtkWidget *widget, gpointer data)
{
    strcpy(selected_file, (char *)data);
}

// Mở cửa sổ giải mã
void open_decrypt_window(GtkWidget *widget, gpointer data)
{
    if (strlen(selected_file) == 0)
    {
        g_print("Hãy chọn một file!\n");
        return;
    }

    window_decrypt = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window_decrypt), "Giải mã");
    gtk_window_set_default_size(GTK_WINDOW(window_decrypt), 300, 200);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(window_decrypt), 15);
    gtk_container_add(GTK_CONTAINER(window_decrypt), vbox);

    // Nhập key
    entry_key = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_key), "Nhập key...");
    gtk_box_pack_start(GTK_BOX(vbox), entry_key, FALSE, FALSE, 5);

    // Combobox chọn độ dài key
    combobox_key_size = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combobox_key_size), "128");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combobox_key_size), "192");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combobox_key_size), "256");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combobox_key_size), 0);
    gtk_box_pack_start(GTK_BOX(vbox), combobox_key_size, FALSE, FALSE, 5);

    GtkWidget *btn_confirm = gtk_button_new_with_label("Xác nhận");
    g_signal_connect(btn_confirm, "clicked", G_CALLBACK(decrypt_file), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), btn_confirm, FALSE, FALSE, 5);

    gtk_widget_show_all(window_decrypt);
}

// Xử lý giải mã file
void decrypt_file(GtkWidget *widget, gpointer data)
{
    const char *key = gtk_entry_get_text(GTK_ENTRY(entry_key));
    if (strlen(key) == 0)
    {
        g_print("Vui lòng nhập key!\n");
        return;
    }

    int key_size_index = gtk_combo_box_get_active(GTK_COMBO_BOX(combobox_key_size));
    int key_sizes[] = {128, 192, 256};
    int key_size = key_sizes[key_size_index];

    // Giả lập giải mã
    g_print("Giải mã file %s với key: %s, độ dài: %d-bit\n", selected_file, key, key_size);

    // Lưu file vào thư mục `de`
    char decrypted_path[512];
    snprintf(decrypted_path, sizeof(decrypted_path), "de/%s", selected_file);
    g_print("Lưu file vào %s\n", decrypted_path);

    gtk_widget_destroy(window_decrypt);
}

// Mở cửa sổ chính
void open_main_window()
{
    window_main = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window_main), "Client");
    gtk_window_set_default_size(GTK_WINDOW(window_main), 400, 250);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_container_set_border_width(GTK_CONTAINER(window_main), 20);
    gtk_container_add(GTK_CONTAINER(window_main), vbox);

    GtkWidget *btn_files = gtk_button_new_with_label("File của tôi");
    g_signal_connect(btn_files, "clicked", G_CALLBACK(on_open_folder_button_clicked), NULL);

    GtkWidget *btn_send = gtk_button_new_with_label("Gửi file");
    g_signal_connect(btn_send, "clicked", G_CALLBACK(open_send_file_window), NULL);

    GtkWidget *btn_received = gtk_button_new_with_label("File nhận");
    g_signal_connect(btn_received, "clicked", G_CALLBACK(open_received_file_window), NULL);

    GtkWidget *btn_logout = gtk_button_new_with_label("Đăng xuất");

    gtk_widget_set_size_request(btn_files, 250, 50);
    gtk_widget_set_size_request(btn_send, 250, 50);
    gtk_widget_set_size_request(btn_received, 250, 50);
    gtk_widget_set_size_request(btn_logout, 250, 50);

    gtk_box_pack_start(GTK_BOX(vbox), btn_files, TRUE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), btn_send, TRUE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), btn_received, TRUE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), btn_logout, TRUE, TRUE, 5);

    g_signal_connect(btn_logout, "clicked", G_CALLBACK(logout), &sockfd);
    g_signal_connect(window_main, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    gtk_widget_show_all(window_main);
}
void receive_file(int sockfd, char *filename, long file_size)
{
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "./received_files/%s", filename);

    FILE *file = fopen(filepath, "wb");
    if (!file)
    {
        perror("Lỗi mở file");
        return;
    }

    printf("Đang nhận file '%s' (%ld bytes)...\n", filename, file_size);

    unsigned char buffer[BUFFER_SIZE];
    size_t bytes_received = 0;
    while (bytes_received < file_size)
    {
        ssize_t recv_bytes = recv(sockfd, buffer, BUFFER_SIZE, 0);
        if (recv_bytes <= 0)
        {
            perror("Lỗi khi nhận dữ liệu hoặc kết nối bị đóng");
            break;
        }
        fwrite(buffer, 1, recv_bytes, file);
        bytes_received += recv_bytes;
    }
    fclose(file);
    printf("Nhận file '%s' thành công!\n", filename);
}

void *receive_messages(void *arg)
{
    int sockfd = *(int *)arg;
    char buffer[BUFFER_SIZE];

    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
        buffer[bytes_received] = '\0';

        if (strncmp(buffer, "SEND_FILE", 9) == 0)
        {
            char filename[256];
            long file_size;
            sscanf(buffer, "SEND_FILE|%[^|]|%ld", filename, &file_size);
            receive_file(sockfd, filename, file_size);
        }
    }
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
        pthread_t receive_thread;
        pthread_create(&receive_thread, NULL, receive_messages, &sockfd);
        pthread_detach(receive_thread);
        // Lưu connection để sử dụng sau này
        client_connection = connection;
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

void activate(GtkApplication *app, gpointer user_data)
{
    window_login = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window_login), "Đăng nhập Client");
    gtk_window_set_default_size(GTK_WINDOW(window_login), 400, 300);
    gtk_container_set_border_width(GTK_CONTAINER(window_login), 20);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(window_login), vbox);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 15);
    gtk_widget_set_margin_start(grid, 30);
    gtk_widget_set_margin_end(grid, 30);
    gtk_widget_set_margin_top(grid, 20);
    gtk_widget_set_margin_bottom(grid, 20);
    gtk_box_pack_start(GTK_BOX(vbox), grid, TRUE, TRUE, 0);

    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale("logo.png", 100, 100, TRUE, NULL);
    GtkWidget *image_logo = gtk_image_new_from_pixbuf(pixbuf);
    g_object_unref(pixbuf);
    gtk_grid_attach(GTK_GRID(grid), image_logo, 0, 0, 2, 1);

    GtkWidget *label_ip = gtk_label_new("IP:");
    gtk_widget_set_halign(label_ip, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label_ip, 0, 1, 1, 1);

    entry_ip = gtk_entry_new();
    gtk_widget_set_size_request(entry_ip, 200, 30);
    gtk_widget_set_halign(entry_ip, GTK_ALIGN_FILL);
    gtk_grid_attach(GTK_GRID(grid), entry_ip, 1, 1, 1, 1);

    GtkWidget *label_port = gtk_label_new("PORT:");
    gtk_widget_set_halign(label_port, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label_port, 0, 2, 1, 1);

    entry_port = gtk_entry_new();
    gtk_widget_set_size_request(entry_port, 200, 30);
    gtk_widget_set_halign(entry_port, GTK_ALIGN_FILL);
    gtk_grid_attach(GTK_GRID(grid), entry_port, 1, 2, 1, 1);

    GtkWidget *label_username = gtk_label_new("Username:");
    gtk_widget_set_halign(label_username, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label_username, 0, 3, 1, 1);

    entry_username = gtk_entry_new();
    gtk_widget_set_size_request(entry_username, 200, 30);
    gtk_widget_set_halign(entry_username, GTK_ALIGN_FILL);
    gtk_grid_attach(GTK_GRID(grid), entry_username, 1, 3, 1, 1);

    GtkWidget *btn_login = gtk_button_new_with_label("Đăng nhập");
    gtk_widget_set_size_request(btn_login, 250, 40);
    gtk_grid_attach(GTK_GRID(grid), btn_login, 0, 4, 2, 1);
    g_signal_connect(btn_login, "clicked", G_CALLBACK(login), NULL);

    gtk_widget_set_halign(grid, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(grid, GTK_ALIGN_CENTER);

    gtk_widget_show_all(window_login);
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
