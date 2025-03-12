#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#define BUFFER_SIZE 4096
#define MAX_CLIENTS 10

// Widgets
GtkWidget *entry_ip, *entry_port, *entry_name, *status_label, *log_view;
GtkWidget *window_main, *window_server;
GtkTextBuffer *log_buffer;
GtkWidget *clear_log_button;
GtkWidget *restart_button;
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
    if (log_buffer == NULL)
    {
        g_printerr("Lỗi: log_buffer chưa được khởi tạo!\n");
        return;
    }
    GtkTextIter end;
    time_t rawtime;
    struct tm *timeinfo;
    char time_str[20];

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(time_str, sizeof(time_str), "[%H:%M:%S]", timeinfo);
    char log_message[512];
    snprintf(log_message, sizeof(log_message), "%s %s", time_str, message);

    gtk_text_buffer_get_end_iter(log_buffer, &end);
    gtk_text_buffer_insert(log_buffer, &end, log_message, -1);
    gtk_text_buffer_insert(log_buffer, &end, "\n", -1);
}

void *client_handler(void *arg)
{
    int client_socket = *(int *)arg;
    free(arg);

    char username[BUFFER_SIZE] = {0};
    int bytes_received = recv(client_socket, username, BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0)
    {
        perror("recv username failed");
        close(client_socket);
        pthread_exit(NULL);
    }
    username[bytes_received] = '\0';
    username[strcspn(username, "\r\n")] = '\0';

    char log_message[200];
    snprintf(log_message, sizeof(log_message), "Client '%s' đã kết nối.", username);
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
            snprintf(log_message, sizeof(log_message), "Client '%s' mất kết nối bất thường.", username);
            append_log(log_message);
            break;
        }

        buffer[bytes_received] = '\0';

        if (strcmp(buffer, "LOGOUT") == 0)
        {
            snprintf(log_message, sizeof(log_message), "Client '%s' đã logout.", username);
            append_log(log_message);
            break;
        }
        char filename[256], receiver[256];
        if (sscanf(buffer, "SEND_FILE|%255[^|]|%255[^|]", filename, receiver) == 2)
        {
            receiver[strcspn(receiver, "\r\n ")] = '\0';
            printf("Nhận file '%s' từ '%s' gửi đến '%s'\n", filename, username, receiver);

            // Tìm socket của người nhận
            int receiver_socket;
            pthread_mutex_lock(&lock);
            for (int i = 0; i < client_count; i++)
            {
                if (strcmp(clients[i].username, receiver) == 0)
                {
                    receiver_socket = clients[i].socket;
                    break;
                }
            }
            pthread_mutex_unlock(&lock);

            if (receiver_socket == -1)
            {
                printf("Không tìm thấy người nhận '%s'\n", receiver);
                send(client_socket, "ERROR|User not found", 20, 0);
                continue;
            }

            // Tạo đường dẫn lưu file
            char filepath[512];
            const char *dir_path = "./received_files";
            // Kiểm tra và tạo thư mục nếu chưa tồn tại
            struct stat st;
            if (stat(dir_path, &st) == -1)
            {
                mkdir(dir_path, 0777); // Tạo thư mục với quyền truy cập đầy đủ
            }
            snprintf(filepath, sizeof(filepath), "./received_files/%s", filename);
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

            printf("Đang nhận file '%s', kích thước: %ld bytes\n", filename, file_size);

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
            printf("Nhận file '%s' thành công! (%ld/%ld bytes)\n", filename, total_received, file_size);
            // Mở file để đọc và gửi cho client nhận
            FILE *file_to_send = fopen(filepath, "rb");
            if (!file_to_send)
            {
                perror("Lỗi mở file để gửi");
                send(receiver_socket, "ERROR|Failed to open file", 25, 0);
                continue;
            }
            // Gửi tín hiệu bắt đầu gửi file
            char start_msg[512];
            snprintf(start_msg, sizeof(start_msg), "SEND_FILE|%s|%ld", filename, file_size);

            pthread_mutex_lock(&lock);
            send(receiver_socket, start_msg, strlen(start_msg), 0);

            unsigned char buffer[BUFFER_SIZE];
            size_t bytes_read;
            while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file_to_send)) > 0)
            {
                if (send(receiver_socket, buffer, bytes_read, 0) == -1)
                {
                    perror("Lỗi gửi dữ liệu file");
                    break;
                }
            }
            pthread_mutex_unlock(&lock);
            fclose(file_to_send);
            printf("Gửi file '%s' thành công! (%ld bytes)\n", filename, file_size);
        }
    }
    pthread_mutex_lock(&lock);
    for (int i = 0; i < client_count; i++)
    {
        if (clients[i].socket == client_socket)
        {
            clients[i] = clients[client_count - 1];
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&lock);

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
    append_log("Server đang chạy...");

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
        pthread_detach(client_thread);
    }
}

void open_server_window()
{

    window_server = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window_server), "Server Logs");
    gtk_window_set_default_size(GTK_WINDOW(window_server), 400, 300);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(window_server), vbox);

    // Nhãn trạng thái
    status_label = gtk_label_new("Server đang chạy...");
    gtk_box_pack_start(GTK_BOX(vbox), status_label, FALSE, FALSE, 5);

    // Tạo frame để bo góc
    GtkWidget *frame = gtk_frame_new(NULL);
    gtk_widget_set_name(frame, "log_frame"); // Gán ID CSS
    gtk_widget_set_margin_start(frame, 30);
    gtk_widget_set_margin_end(frame, 30);
    gtk_widget_set_margin_top(frame, 10);
    gtk_widget_set_margin_bottom(frame, 10);
    gtk_widget_set_size_request(frame, 320, 200);

    gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);

    // Thêm cửa sổ cuộn vào frame
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(frame), scrolled_window);

    // Khu vực hiển thị log
    log_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(log_view), FALSE);
    log_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(log_view));

    gtk_container_add(GTK_CONTAINER(scrolled_window), log_view);
    // Áp dụng CSS
    GtkCssProvider *css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_path(css_provider, "style.css", NULL);
    GtkStyleContext *context = gtk_widget_get_style_context(frame);
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(css_provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_USER);

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
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale("logo.png", 80, 80, TRUE, NULL);
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