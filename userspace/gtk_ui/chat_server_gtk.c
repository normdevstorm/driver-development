#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include "../crypto_lib.h"

#define SERVER_PORT 8080
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

typedef struct {
    int socket_fd;
    char username[64];
    struct sockaddr_in address;
    gboolean authenticated;
} Client;

typedef struct {
    GtkWidget *window;
    GtkWidget *log_view;
    GtkWidget *client_list;
    GtkWidget *start_button;
    GtkWidget *stop_button;
    GtkWidget *status_label;
    GtkWidget *port_entry;
    GtkTextBuffer *log_buffer;
    GtkListStore *client_store;
    int server_socket;
    gboolean server_running;
    Client clients[MAX_CLIENTS];
    pthread_mutex_t clients_mutex;
    pthread_t server_thread;
} ChatServerGTK;

static ChatServerGTK *server_app = NULL;

// User database (in real application, this would be in a file or database)
typedef struct {
    char username[64];
    char password[64];
} User;

static User users[] = {
    {"admin", "admin123"},
    {"user1", "password1"},
    {"user2", "password2"},
    {"test", "test123"}
};
static int num_users = sizeof(users) / sizeof(users[0]);

// Forward declarations
gboolean update_log(gchar *message);
gboolean update_status(gchar *status);
gboolean update_client_list();

// Authenticate user
int authenticate_user(const char *username, const char *password) {
    for (int i = 0; i < num_users; i++) {
        if (strcmp(users[i].username, username) == 0 && 
            strcmp(users[i].password, password) == 0) {
            return 1;
        }
    }
    return 0;
}

// Update server log
gboolean update_log(gchar *message) {
    if (server_app && server_app->log_buffer) {
        GtkTextIter iter;
        gtk_text_buffer_get_end_iter(server_app->log_buffer, &iter);
        
        // Add timestamp
        time_t now = time(NULL);
        char timestamp[32];
        strftime(timestamp, sizeof(timestamp), "[%H:%M:%S] ", localtime(&now));
        
        gtk_text_buffer_insert(server_app->log_buffer, &iter, timestamp, -1);
        gtk_text_buffer_insert(server_app->log_buffer, &iter, message, -1);
        gtk_text_buffer_insert(server_app->log_buffer, &iter, "\n", -1);
        
        // Auto-scroll to bottom
        GtkTextMark *mark = gtk_text_buffer_get_insert(server_app->log_buffer);
        gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(server_app->log_view), mark);
    }
    
    g_free(message);
    return FALSE;
}

// Update status label
gboolean update_status(gchar *status) {
    if (server_app && server_app->status_label) {
        gtk_label_set_text(GTK_LABEL(server_app->status_label), status);
    }
    g_free(status);
    return FALSE;
}

// Update client list in GUI
gboolean update_client_list() {
    if (!server_app || !server_app->client_store) {
        return FALSE;
    }
    
    gtk_list_store_clear(server_app->client_store);
    
    pthread_mutex_lock(&server_app->clients_mutex);
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (server_app->clients[i].socket_fd > 0) {
            GtkTreeIter iter;
            gtk_list_store_append(server_app->client_store, &iter);
            
            char client_info[256];
            if (server_app->clients[i].authenticated) {
                snprintf(client_info, sizeof(client_info), "%s (%s)", 
                        server_app->clients[i].username, 
                        inet_ntoa(server_app->clients[i].address.sin_addr));
            } else {
                snprintf(client_info, sizeof(client_info), "Not authenticated (%s)", 
                        inet_ntoa(server_app->clients[i].address.sin_addr));
            }
            
            gtk_list_store_set(server_app->client_store, &iter, 0, client_info, -1);
        }
    }
    
    pthread_mutex_unlock(&server_app->clients_mutex);
    return FALSE;
}

// Broadcast message to all authenticated clients
void broadcast_message(const char *message, int sender_fd) {
    pthread_mutex_lock(&server_app->clients_mutex);
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (server_app->clients[i].socket_fd > 0 && 
            server_app->clients[i].authenticated &&
            server_app->clients[i].socket_fd != sender_fd) {
            send(server_app->clients[i].socket_fd, message, strlen(message), 0);
        }
    }
    
    pthread_mutex_unlock(&server_app->clients_mutex);
}

// Find client index by socket
int find_client_index(int socket_fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (server_app->clients[i].socket_fd == socket_fd) {
            return i;
        }
    }
    return -1;
}

// Remove client
void remove_client(int socket_fd) {
    pthread_mutex_lock(&server_app->clients_mutex);
    
    int index = find_client_index(socket_fd);
    if (index >= 0) {
        char log_msg[256];
        if (server_app->clients[index].authenticated) {
            snprintf(log_msg, sizeof(log_msg), "Client %s disconnected", 
                    server_app->clients[index].username);
        } else {
            snprintf(log_msg, sizeof(log_msg), "Unauthenticated client disconnected");
        }
        
        close(server_app->clients[index].socket_fd);
        memset(&server_app->clients[index], 0, sizeof(Client));
        
        g_idle_add((GSourceFunc)update_log, g_strdup(log_msg));
        g_idle_add((GSourceFunc)update_client_list, NULL);
    }
    
    pthread_mutex_unlock(&server_app->clients_mutex);
}

// Handle client communication
void* handle_client(void *arg) {
    int client_socket = *(int*)arg;
    free(arg);
    
    char buffer[BUFFER_SIZE];
    char decrypted_msg[BUFFER_SIZE];
    int client_index = find_client_index(client_socket);
    
    if (client_index == -1) {
        close(client_socket);
        return NULL;
    }
    
    while (server_app->server_running) {
        int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            break;
        }
        buffer[bytes_received] = '\0';
        
        // Handle authentication
        if (!server_app->clients[client_index].authenticated) {
            if (strstr(buffer, "AUTH:") == buffer) {
                char *username = buffer + 5;
                char *password = strchr(username, ':');
                if (password) {
                    *password = '\0';
                    password++;
                    
                    if (authenticate_user(username, password)) {
                        strcpy(server_app->clients[client_index].username, username);
                        server_app->clients[client_index].authenticated = TRUE;
                        
                        send(client_socket, "AUTH_SUCCESS", 12, 0);
                        
                        char log_msg[256];
                        snprintf(log_msg, sizeof(log_msg), "User %s authenticated successfully", username);
                        g_idle_add((GSourceFunc)update_log, g_strdup(log_msg));
                        g_idle_add((GSourceFunc)update_client_list, NULL);
                        
                        // Broadcast user joined
                        char join_msg[256];
                        snprintf(join_msg, sizeof(join_msg), "*** %s joined the chat ***", username);
                        broadcast_message(join_msg, client_socket);
                    } else {
                        send(client_socket, "AUTH_FAILED", 11, 0);
                        char log_msg[256];
                        snprintf(log_msg, sizeof(log_msg), "Authentication failed for user: %s", username);
                        g_idle_add((GSourceFunc)update_log, g_strdup(log_msg));
                    }
                }
            }
        } else {
            // Handle regular messages
            char *message_to_broadcast = buffer;
            
            // Check if message is encrypted
            if (strstr(buffer, "[ENCRYPTED]") == buffer) {
                char *encrypted_data = buffer + strlen("[ENCRYPTED]");
                if (decrypt_message(encrypted_data, decrypted_msg, sizeof(decrypted_msg)) == 0) {
                    message_to_broadcast = decrypted_msg;
                    
                    char log_msg[512];
                    snprintf(log_msg, sizeof(log_msg), "Encrypted message from %s: %s", 
                            server_app->clients[client_index].username, decrypted_msg);
                    g_idle_add((GSourceFunc)update_log, g_strdup(log_msg));
                } else {
                    char log_msg[256];
                    snprintf(log_msg, sizeof(log_msg), "Failed to decrypt message from %s", 
                            server_app->clients[client_index].username);
                    g_idle_add((GSourceFunc)update_log, g_strdup(log_msg));
                    continue;
                }
            } else {
                char log_msg[512];
                snprintf(log_msg, sizeof(log_msg), "Plain text message: %s", message_to_broadcast);
                g_idle_add((GSourceFunc)update_log, g_strdup(log_msg));
            }
            
            // Broadcast to all other clients
            broadcast_message(message_to_broadcast, client_socket);
        }
    }
    
    remove_client(client_socket);
    return NULL;
}

// Server main loop
void* server_main(void *arg) {
    ChatServerGTK *app = (ChatServerGTK*)arg;
    
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    // Create server socket
    app->server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (app->server_socket == -1) {
        g_idle_add((GSourceFunc)update_log, g_strdup("Failed to create server socket"));
        return NULL;
    }
    
    // Allow socket reuse
    int opt = 1;
    setsockopt(app->server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Bind socket
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);
    
    if (bind(app->server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        g_idle_add((GSourceFunc)update_log, g_strdup("Failed to bind server socket"));
        close(app->server_socket);
        return NULL;
    }
    
    // Listen for connections
    if (listen(app->server_socket, MAX_CLIENTS) == -1) {
        g_idle_add((GSourceFunc)update_log, g_strdup("Failed to listen on server socket"));
        close(app->server_socket);
        return NULL;
    }
    
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Server listening on port %d", SERVER_PORT);
    g_idle_add((GSourceFunc)update_log, g_strdup(log_msg));
    g_idle_add((GSourceFunc)update_status, g_strdup("Server running"));
    
    // Accept connections
    while (app->server_running) {
        int client_socket = accept(app->server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket == -1) {
            if (app->server_running) {
                g_idle_add((GSourceFunc)update_log, g_strdup("Failed to accept client connection"));
            }
            continue;
        }
        
        // Find empty slot for client
        pthread_mutex_lock(&app->clients_mutex);
        int slot = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (app->clients[i].socket_fd == 0) {
                slot = i;
                break;
            }
        }
        
        if (slot == -1) {
            pthread_mutex_unlock(&app->clients_mutex);
            close(client_socket);
            g_idle_add((GSourceFunc)update_log, g_strdup("Maximum clients reached, connection rejected"));
            continue;
        }
        
        // Initialize client
        app->clients[slot].socket_fd = client_socket;
        app->clients[slot].address = client_addr;
        app->clients[slot].authenticated = FALSE;
        memset(app->clients[slot].username, 0, sizeof(app->clients[slot].username));
        
        pthread_mutex_unlock(&app->clients_mutex);
        
        char client_log[256];
        snprintf(client_log, sizeof(client_log), "New client connected from %s", 
                inet_ntoa(client_addr.sin_addr));
        g_idle_add((GSourceFunc)update_log, g_strdup(client_log));
        g_idle_add((GSourceFunc)update_client_list, NULL);
        
        // Create thread to handle client
        pthread_t client_thread;
        int *client_socket_ptr = malloc(sizeof(int));
        *client_socket_ptr = client_socket;
        pthread_create(&client_thread, NULL, handle_client, client_socket_ptr);
        pthread_detach(client_thread);
    }
    
    close(app->server_socket);
    g_idle_add((GSourceFunc)update_status, g_strdup("Server stopped"));
    return NULL;
}

// Start server
void on_start_clicked(GtkButton *button, gpointer user_data) {
    ChatServerGTK *app = (ChatServerGTK*)user_data;
    
    if (app->server_running) {
        return;
    }
    
    app->server_running = TRUE;
    
    // Clear client list
    for (int i = 0; i < MAX_CLIENTS; i++) {
        memset(&app->clients[i], 0, sizeof(Client));
    }
    
    gtk_widget_set_sensitive(app->start_button, FALSE);
    gtk_widget_set_sensitive(app->stop_button, TRUE);
    gtk_widget_set_sensitive(app->port_entry, FALSE);
    
    // Start server thread
    pthread_create(&app->server_thread, NULL, server_main, app);
    
    update_log(g_strdup("Starting server..."));
}

// Stop server
void on_stop_clicked(GtkButton *button, gpointer user_data) {
    ChatServerGTK *app = (ChatServerGTK*)user_data;
    
    if (!app->server_running) {
        return;
    }
    
    app->server_running = FALSE;
    
    // Close all client connections
    pthread_mutex_lock(&app->clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (app->clients[i].socket_fd > 0) {
            close(app->clients[i].socket_fd);
            memset(&app->clients[i], 0, sizeof(Client));
        }
    }
    pthread_mutex_unlock(&app->clients_mutex);
    
    // Close server socket
    if (app->server_socket > 0) {
        close(app->server_socket);
    }
    
    // Wait for server thread to finish
    pthread_join(app->server_thread, NULL);
    
    gtk_widget_set_sensitive(app->start_button, TRUE);
    gtk_widget_set_sensitive(app->stop_button, FALSE);
    gtk_widget_set_sensitive(app->port_entry, TRUE);
    
    update_log(g_strdup("Server stopped"));
    update_client_list();
}

// Handle window close
gboolean on_window_delete(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
    ChatServerGTK *app = (ChatServerGTK*)user_data;
    
    if (app->server_running) {
        on_stop_clicked(NULL, app);
    }
    
    gtk_main_quit();
    return FALSE;
}

// Create main window
GtkWidget* create_server_window() {
    server_app = g_malloc0(sizeof(ChatServerGTK));
    server_app->server_running = FALSE;
    pthread_mutex_init(&server_app->clients_mutex, NULL);
    
    // Main window
    server_app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(server_app->window), "Encrypted Chat Server");
    gtk_window_set_default_size(GTK_WINDOW(server_app->window), 800, 600);
    gtk_container_set_border_width(GTK_CONTAINER(server_app->window), 10);
    
    // Main vbox
    GtkWidget *main_vbox = gtk_vbox_new(FALSE, 5);
    gtk_container_add(GTK_CONTAINER(server_app->window), main_vbox);
    
    // Control frame
    GtkWidget *control_frame = gtk_frame_new("Server Control");
    GtkWidget *control_hbox = gtk_hbox_new(FALSE, 5);
    gtk_container_set_border_width(GTK_CONTAINER(control_hbox), 5);
    gtk_container_add(GTK_CONTAINER(control_frame), control_hbox);
    
    // Port settings
    GtkWidget *port_label = gtk_label_new("Port:");
    server_app->port_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(server_app->port_entry), "8080");
    gtk_entry_set_width_chars(GTK_ENTRY(server_app->port_entry), 8);
    gtk_widget_set_sensitive(server_app->port_entry, FALSE); // Fixed port for now
    
    // Control buttons
    server_app->start_button = gtk_button_new_with_label("Start Server");
    server_app->stop_button = gtk_button_new_with_label("Stop Server");
    gtk_widget_set_sensitive(server_app->stop_button, FALSE);
    
    // Status label
    server_app->status_label = gtk_label_new("Server stopped");
    
    gtk_box_pack_start(GTK_BOX(control_hbox), port_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(control_hbox), server_app->port_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(control_hbox), server_app->start_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(control_hbox), server_app->stop_button, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(control_hbox), server_app->status_label, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(main_vbox), control_frame, FALSE, FALSE, 0);
    
    // Main content area
    GtkWidget *content_hbox = gtk_hbox_new(FALSE, 5);
    
    // Server log frame
    GtkWidget *log_frame = gtk_frame_new("Server Log");
    GtkWidget *log_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(log_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_set_border_width(GTK_CONTAINER(log_scroll), 5);
    
    server_app->log_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(server_app->log_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(server_app->log_view), GTK_WRAP_WORD);
    server_app->log_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(server_app->log_view));
    
    // Set font
    PangoFontDescription *font_desc = pango_font_description_from_string("Monospace 9");
    gtk_widget_modify_font(server_app->log_view, font_desc);
    pango_font_description_free(font_desc);
    
    gtk_container_add(GTK_CONTAINER(log_scroll), server_app->log_view);
    gtk_container_add(GTK_CONTAINER(log_frame), log_scroll);
    
    // Client list frame
    GtkWidget *client_frame = gtk_frame_new("Connected Clients");
    GtkWidget *client_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(client_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_set_border_width(GTK_CONTAINER(client_scroll), 5);
    
    // Create list store and tree view
    server_app->client_store = gtk_list_store_new(1, G_TYPE_STRING);
    server_app->client_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(server_app->client_store));
    
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes("Client", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(server_app->client_list), column);
    
    gtk_container_add(GTK_CONTAINER(client_scroll), server_app->client_list);
    gtk_container_add(GTK_CONTAINER(client_frame), client_scroll);
    
    // Add to content area
    gtk_box_pack_start(GTK_BOX(content_hbox), log_frame, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(content_hbox), client_frame, FALSE, FALSE, 0);
    gtk_widget_set_size_request(client_frame, 250, -1);
    
    gtk_box_pack_start(GTK_BOX(main_vbox), content_hbox, TRUE, TRUE, 0);
    
    // Connect signals
    g_signal_connect(server_app->start_button, "clicked", G_CALLBACK(on_start_clicked), server_app);
    g_signal_connect(server_app->stop_button, "clicked", G_CALLBACK(on_stop_clicked), server_app);
    g_signal_connect(server_app->window, "delete-event", G_CALLBACK(on_window_delete), server_app);
    
    // Add initial log message
    update_log(g_strdup("Server application started"));
    update_log(g_strdup("Available users: admin, user1, user2, test"));
    
    return server_app->window;
}

int main(int argc, char *argv[]) {
    // Ignore SIGPIPE signal
    signal(SIGPIPE, SIG_IGN);
    
    gtk_init(&argc, &argv);
    
    GtkWidget *window = create_server_window();
    gtk_widget_show_all(window);
    
    gtk_main();
    
    if (server_app) {
        pthread_mutex_destroy(&server_app->clients_mutex);
        g_free(server_app);
    }
    
    return 0;
}
