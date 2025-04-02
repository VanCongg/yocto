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
int sockfd;
GtkWidget *combo_clients;
GtkWidget *entry_filename;
GtkWidget *file_list;
GtkWidget *window_decrypt, *entry_key, *combobox_key_size;
GtkWidget *combo_keysize;
GtkWidget *entry_key;
GtkWidget *comboBox_keysize;
char selected_file[256] = "";
void select_file(GtkWidget *widget, gpointer data);
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
    const char *folder_path = "en";
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
        g_print("Chưa chọn file!\n");
        return;
    }

    const char *new_filename = gtk_entry_get_text(GTK_ENTRY(entry_filename));
    if (strlen(new_filename) == 0)
    {
        g_print("Chưa nhập tên file mới!\n");
        return;
    }

    const char *key = gtk_entry_get_text(GTK_ENTRY(entry_key));
    if (strlen(key) == 0)
    {
        g_print("Chưa nhập key mã hóa!\n");
        return;
    }

    AESKeyLength key_size;
    const char *key_size_str = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo_keysize));

    if (strcmp(key_size_str, "128") == 0)
    {
        key_size = AES_128;
    }
    else if (strcmp(key_size_str, "192") == 0)
    {
        key_size = AES_192;
    }
    else if (strcmp(key_size_str, "256") == 0)
    {
        key_size = AES_256;
    }
    else
    {
        printf("Lỗi: Giá trị key_size không hợp lệ!\n");
        return;
    }

    g_print("File nguồn: %s\n", selected_filepath);
    g_print("Tên file mới: %s\n", new_filename);
    g_print("Key: %s\n", key);
    g_print("Key Size: %d\n", key_size);

    // Tạo thư mục en/ nếu chưa tồn tại
    struct stat st = {0};
    if (stat("en", &st) == -1)
    {
        if (mkdir("en", 0700) != 0)
        {
            perror("Lỗi khi tạo thư mục 'en/'");
            return;
        }
        g_print("Đã tạo thư mục 'en/'\n");
    }

    // Lấy tên file từ đường dẫn
    const char *basename = g_path_get_basename(selected_filepath);
    char filename_no_ext[PATH_MAX];
    char *dot = strrchr(basename, '.');
    if (dot)
    {
        strncpy(filename_no_ext, basename, dot - basename);
        filename_no_ext[dot - basename] = '\0';
    }
    else
    {
        strcpy(filename_no_ext, basename);
    }
    g_free(basename);
    // Giữ nguyên tên file sau khi mã hóa
    char encrypted_file[PATH_MAX];
    snprintf(encrypted_file, sizeof(encrypted_file), "en/%s.enc", filename_no_ext);
    g_print("File mã hóa sẽ được lưu tại: %s\n", encrypted_file);

    int result = aes_encrypt_file((const uint8_t *)selected_filepath,
                                  (const uint8_t *)encrypted_file,
                                  (const uint8_t *)key, (AESKeyLength)key_size);

    if (result != 0)
    {
        g_print("Lỗi khi mã hóa file!\n");
        return;
    }
    g_print("File đã mã hóa thành công: %s\n", encrypted_file);

    struct stat encrypted_stat;
    if (stat(encrypted_file, &encrypted_stat) != 0)
    {
        perror("Không thể lấy thông tin file mã hóa");
        return;
    }
    g_print("Kích thước file mã hóa: %ld bytes\n", encrypted_stat.st_size);

    FILE *file = fopen(encrypted_file, "rb");
    if (!file)
    {
        perror("Không thể mở file đã mã hóa");
        return;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    g_print("Kích thước file thực tế trước khi gửi: %ld bytes\n", file_size);

    char command[512];
    if (strlen(filename_no_ext) >= 500)
    {
        g_print("Lỗi: Tên file quá dài!\n");
        return;
    }
    snprintf(command, sizeof(command), "SEND_FILE|%s.enc", filename_no_ext);
    if (send(sockfd, command, strlen(command), 0) == -1)
    {
        perror("Lỗi khi gửi thông tin file");
        fclose(file);
        return;
    }
    g_print("Đã gửi yêu cầu gửi file mã hóa: %s.enc\n", filename_no_ext);

    if (send(sockfd, &file_size, sizeof(file_size), 0) == -1)
    {
        perror("Lỗi khi gửi kích thước file");
        fclose(file);
        return;
    }

    char buffer[BUFFER_SIZE];
    int bytes_read;
    long total_bytes_sent = 0;

    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0)
    {
        int bytes_sent = send(sockfd, buffer, bytes_read, 0);
        if (bytes_sent == -1)
        {
            perror("Lỗi khi gửi nội dung file");
            fclose(file);
            return;
        }
        total_bytes_sent += bytes_sent;
    }
    fclose(file);

    g_print("Đã gửi file mã hóa: %s.enc\n", filename_no_ext);
    g_print("Tổng số bytes đã gửi: %ld / %ld\n", total_bytes_sent, file_size);
    gtk_widget_destroy(window_send_file);
    window_send_file = NULL;
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

    // Hàng 6: Nút gửi
    GtkWidget *btn_send = gtk_button_new_with_label("Gửi");
    g_signal_connect(btn_send, "clicked", G_CALLBACK(sendfile_to_server), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), btn_send, FALSE, FALSE, 5);

    g_signal_connect(window_send_file, "destroy", G_CALLBACK(gtk_widget_hide), NULL);
    gtk_widget_show_all(window_send_file);
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

    GtkWidget *btn_logout = gtk_button_new_with_label("Đăng xuất");

    gtk_widget_set_size_request(btn_files, 250, 50);
    gtk_widget_set_size_request(btn_send, 250, 50);
    gtk_widget_set_size_request(btn_logout, 250, 50);

    gtk_box_pack_start(GTK_BOX(vbox), btn_files, TRUE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), btn_send, TRUE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), btn_logout, TRUE, TRUE, 5);

    g_signal_connect(btn_logout, "clicked", G_CALLBACK(logout), &sockfd);
    g_signal_connect(window_main, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    gtk_widget_show_all(window_main);
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

    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale("resources/logo.png", 100, 100, TRUE, NULL);
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
