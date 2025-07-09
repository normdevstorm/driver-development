#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include "../crypto_lib.h"

#define SERVER_PORT 8888
#define BUFFER_SIZE 1024

typedef struct {
    GtkWidget *window;
    GtkWidget *chat_view;
    GtkWidget *message_entry;
    GtkWidget *username_entry;
    GtkWidget *password_entry;
    GtkWidget *server_ip_entry;
    GtkWidget *port_entry;
    GtkWidget *connect_button;
    GtkWidget *signup_button;
    GtkWidget *disconnect_button;
    GtkWidget *send_button;
    GtkWidget *status_label;
    GtkTextBuffer *chat_buffer;
    int socket_fd;
    gboolean connected;
    char username[64];
    pthread_t receive_thread;
} ChatClientGTK;

static ChatClientGTK *client_app = NULL;

// Forward declarations
gboolean update_chat_display(gchar *message);
gboolean update_chat_display_aligned(gchar *message, gboolean is_own_message);
gboolean update_status(gchar *status);
int encrypt_message(const char *input, char *output, size_t *output_len);
int decrypt_message(const char *input, size_t input_len, char *output);
void on_signup_clicked(GtkButton *button, gpointer user_data);

// Structure to pass both message and alignment info
typedef struct {
    gchar *message;
    gboolean is_own_message;
} MessageData;

// Wrapper function for g_idle_add
gboolean update_chat_display_wrapper(gpointer data) {
    MessageData *msg_data = (MessageData*)data;
    gboolean result = update_chat_display_aligned(msg_data->message, msg_data->is_own_message);
    g_free(msg_data);
    return result;
}

// Thread for receiving messages
void* receive_messages(void *arg) {
    ChatClientGTK *app = (ChatClientGTK*)arg;
    char buffer[BUFFER_SIZE];
    char decrypted_msg[BUFFER_SIZE];
    
    while (app->connected) {
        int bytes_received = recv(app->socket_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            if (app->connected) {
                g_idle_add((GSourceFunc)update_status, g_strdup("Disconnected from server"));
                app->connected = FALSE;
            }
            break;
        }
        buffer[bytes_received] = '\0';
        
        // Decrypt message if it's encrypted
        if (strstr(buffer, "[ENCRYPTED]") == buffer) {
            // Remove [ENCRYPTED] prefix and decrypt
            char *encrypted_data = buffer + strlen("[ENCRYPTED]");
            if (decrypt_message(encrypted_data, strlen(encrypted_data), decrypted_msg) == 0) {
                g_idle_add((GSourceFunc)update_chat_display, g_strdup(decrypted_msg));
            } else {
                g_idle_add((GSourceFunc)update_chat_display, g_strdup("Failed to decrypt message"));
            }
        } else {
            // Plain text message
            g_idle_add((GSourceFunc)update_chat_display, g_strdup(buffer));
        }
    }
    
    return NULL;
}

// Update chat display (called from main thread)
gboolean update_chat_display(gchar *message) {
    return update_chat_display_aligned(message, FALSE);
}

// Update chat display with alignment
gboolean update_chat_display_aligned(gchar *message, gboolean is_own_message) {
    if (client_app && client_app->chat_buffer) {
        GtkTextIter iter;
        gtk_text_buffer_get_end_iter(client_app->chat_buffer, &iter);
        
        // Add timestamp
        time_t now = time(NULL);
        char timestamp[32];
        strftime(timestamp, sizeof(timestamp), "[%H:%M:%S] ", localtime(&now));
        
        // Parse message to extract sender and content
        char *sender = NULL;
        char *content = message;
        char *colon_pos = strchr(message, ':');
        
        if (colon_pos && !is_own_message) {
            // For received messages, extract sender
            *colon_pos = '\0';
            sender = message;
            content = colon_pos + 2; // Skip ': '
        }
        
        if (is_own_message) {
            // Right-align own messages
            gtk_text_buffer_insert(client_app->chat_buffer, &iter, "                                          ", -1);
            gtk_text_buffer_insert(client_app->chat_buffer, &iter, timestamp, -1);
            gtk_text_buffer_insert(client_app->chat_buffer, &iter, "You: ", -1);
            gtk_text_buffer_insert(client_app->chat_buffer, &iter, content, -1);
        } else {
            // Left-align received messages
            gtk_text_buffer_insert(client_app->chat_buffer, &iter, timestamp, -1);
            if (sender) {
                gtk_text_buffer_insert(client_app->chat_buffer, &iter, sender, -1);
                gtk_text_buffer_insert(client_app->chat_buffer, &iter, ": ", -1);
                gtk_text_buffer_insert(client_app->chat_buffer, &iter, content, -1);
            } else {
                gtk_text_buffer_insert(client_app->chat_buffer, &iter, message, -1);
            }
        }
        
        gtk_text_buffer_insert(client_app->chat_buffer, &iter, "\n", -1);
        
        // Auto-scroll to bottom
        GtkTextMark *mark = gtk_text_buffer_get_insert(client_app->chat_buffer);
        gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(client_app->chat_view), mark);
    }
    
    g_free(message);
    return FALSE;
}

// Update status label
gboolean update_status(gchar *status) {
    if (client_app && client_app->status_label) {
        gtk_label_set_text(GTK_LABEL(client_app->status_label), status);
    }
    g_free(status);
    return FALSE;
}

// Connect to server
void on_connect_clicked(GtkButton *button, gpointer user_data) {
    (void)button; // Suppress unused parameter warning
    ChatClientGTK *app = (ChatClientGTK*)user_data;
    
    const char *username = gtk_entry_get_text(GTK_ENTRY(app->username_entry));
    const char *password = gtk_entry_get_text(GTK_ENTRY(app->password_entry));
    const char *server_ip = gtk_entry_get_text(GTK_ENTRY(app->server_ip_entry));
    const char *port_text = gtk_entry_get_text(GTK_ENTRY(app->port_entry));
    
    // Clear any previous status message
    gtk_label_set_text(GTK_LABEL(app->status_label), "");
    
    if (strlen(username) == 0 || strlen(password) == 0) {
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(app->window),
                                                   GTK_DIALOG_MODAL,
                                                   GTK_MESSAGE_ERROR,
                                                   GTK_BUTTONS_OK,
                                                   "Please enter username and password");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }
    
    if (strlen(server_ip) == 0) {
        gtk_entry_set_text(GTK_ENTRY(app->server_ip_entry), "127.0.0.1");
        server_ip = "127.0.0.1";
    }
    
    // Parse port
    int port = SERVER_PORT; // Default port
    if (strlen(port_text) > 0) {
        port = atoi(port_text);
        if (port <= 0 || port > 65535) {
            gtk_label_set_text(GTK_LABEL(app->status_label), "Invalid port number (1-65535)");
            return;
        }
    }
    
    gtk_label_set_text(GTK_LABEL(app->status_label), "Connecting...");
    
    // Create socket
    app->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (app->socket_fd == -1) {
        gtk_label_set_text(GTK_LABEL(app->status_label), "Failed to create socket");
        return;
    }
    
    // Connect to server
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    // Use inet_pton for better IP address parsing
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Invalid IP address: %s", server_ip);
        gtk_label_set_text(GTK_LABEL(app->status_label), error_msg);
        close(app->socket_fd);
        return;
    }
    
    if (connect(app->socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Connection failed to %s:%d", server_ip, port);
        gtk_label_set_text(GTK_LABEL(app->status_label), error_msg);
        close(app->socket_fd);
        return;
    }
    
    // Perform RSA key exchange
    gtk_label_set_text(GTK_LABEL(app->status_label), "Performing key exchange...");
    char shared_des_key[DES_KEY_SIZE];
    
    if (perform_key_exchange_client(app->socket_fd, shared_des_key) < 0) {
        gtk_label_set_text(GTK_LABEL(app->status_label), "Key exchange failed");
        close(app->socket_fd);
        return;
    }
    
    // Set the exchanged DES key for encryption
    if (crypto_set_key(shared_des_key) < 0) {
        gtk_label_set_text(GTK_LABEL(app->status_label), "Failed to set encryption key");
        close(app->socket_fd);
        return;
    }
    
    gtk_label_set_text(GTK_LABEL(app->status_label), "Authenticating...");
    
    // Send authentication
    char auth_msg[BUFFER_SIZE];
    snprintf(auth_msg, sizeof(auth_msg), "AUTH:%s:%s", username, password);
    send(app->socket_fd, auth_msg, strlen(auth_msg), 0);
    
    // Wait for authentication response
    char response[BUFFER_SIZE];
    int bytes_received = recv(app->socket_fd, response, sizeof(response) - 1, 0);
    if (bytes_received > 0) {
        response[bytes_received] = '\0';
        if (strstr(response, "AUTH_SUCCESS") == response) {
            app->connected = TRUE;
            strcpy(app->username, username);
            
            gtk_widget_set_sensitive(app->connect_button, FALSE);
            gtk_widget_set_sensitive(app->disconnect_button, TRUE);
            gtk_widget_set_sensitive(app->send_button, TRUE);
            gtk_widget_set_sensitive(app->message_entry, TRUE);
            gtk_widget_set_sensitive(app->username_entry, FALSE);
            gtk_widget_set_sensitive(app->password_entry, FALSE);
            gtk_widget_set_sensitive(app->server_ip_entry, FALSE);
            gtk_widget_set_sensitive(app->port_entry, FALSE);
            
            gtk_label_set_text(GTK_LABEL(app->status_label), "Connected");
            
            // Start receive thread
            pthread_create(&app->receive_thread, NULL, receive_messages, app);
            
            // Add welcome message
            char welcome_msg[256];
            snprintf(welcome_msg, sizeof(welcome_msg), "Connected as %s", username);
            update_chat_display(g_strdup(welcome_msg));
        } else {
            gtk_label_set_text(GTK_LABEL(app->status_label), "Authentication failed");
            close(app->socket_fd);
        }
    } else {
        gtk_label_set_text(GTK_LABEL(app->status_label), "No response from server");
        close(app->socket_fd);
    }
}

// Disconnect from server
void on_disconnect_clicked(GtkButton *button, gpointer user_data) {
    (void)button; // Suppress unused parameter warning
    ChatClientGTK *app = (ChatClientGTK*)user_data;
    
    if (app->connected) {
        app->connected = FALSE;
        close(app->socket_fd);
        pthread_join(app->receive_thread, NULL);
        
        gtk_widget_set_sensitive(app->connect_button, TRUE);
        gtk_widget_set_sensitive(app->disconnect_button, FALSE);
        gtk_widget_set_sensitive(app->send_button, FALSE);
        gtk_widget_set_sensitive(app->message_entry, FALSE);
        gtk_widget_set_sensitive(app->username_entry, TRUE);
        gtk_widget_set_sensitive(app->password_entry, TRUE);
        gtk_widget_set_sensitive(app->server_ip_entry, TRUE);
        gtk_widget_set_sensitive(app->port_entry, TRUE);
        
        gtk_label_set_text(GTK_LABEL(app->status_label), "Disconnected");
        update_chat_display(g_strdup("Disconnected from server"));
    }
}

// Signup button callback
void on_signup_clicked(GtkButton *button, gpointer user_data) {
    (void)button; // Suppress unused parameter warning
    ChatClientGTK *app = (ChatClientGTK*)user_data;
    
    const char *username = gtk_entry_get_text(GTK_ENTRY(app->username_entry));
    const char *password = gtk_entry_get_text(GTK_ENTRY(app->password_entry));
    const char *server_ip = gtk_entry_get_text(GTK_ENTRY(app->server_ip_entry));
    const char *port_text = gtk_entry_get_text(GTK_ENTRY(app->port_entry));
    
    // Clear any previous status message
    gtk_label_set_text(GTK_LABEL(app->status_label), "");
    
    if (strlen(username) == 0 || strlen(password) == 0) {
        gtk_label_set_text(GTK_LABEL(app->status_label), "Please enter username and password");
        return;
    }
    
    if (strlen(server_ip) == 0) {
        gtk_entry_set_text(GTK_ENTRY(app->server_ip_entry), "127.0.0.1");
        server_ip = "127.0.0.1";
    }
    
    // Parse port
    int port = SERVER_PORT; // Default port
    if (strlen(port_text) > 0) {
        port = atoi(port_text);
        if (port <= 0 || port > 65535) {
            gtk_label_set_text(GTK_LABEL(app->status_label), "Invalid port number (1-65535)");
            return;
        }
    }
    
    gtk_label_set_text(GTK_LABEL(app->status_label), "Connecting for signup...");
    
    // Create socket
    app->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (app->socket_fd == -1) {
        gtk_label_set_text(GTK_LABEL(app->status_label), "Failed to create socket");
        return;
    }
    
    // Connect to server
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    // Use inet_pton for better IP address parsing
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Invalid IP address: %s", server_ip);
        gtk_label_set_text(GTK_LABEL(app->status_label), error_msg);
        close(app->socket_fd);
        return;
    }
    
    if (connect(app->socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Signup connection failed to %s:%d", server_ip, port);
        gtk_label_set_text(GTK_LABEL(app->status_label), error_msg);
        close(app->socket_fd);
        return;
    }
    
    // Perform RSA key exchange for signup
    gtk_label_set_text(GTK_LABEL(app->status_label), "Performing key exchange...");
    char shared_des_key[DES_KEY_SIZE];
    
    if (perform_key_exchange_client(app->socket_fd, shared_des_key) < 0) {
        gtk_label_set_text(GTK_LABEL(app->status_label), "Key exchange failed");
        close(app->socket_fd);
        return;
    }
    
    // Set the exchanged DES key for encryption
    if (crypto_set_key(shared_des_key) < 0) {
        gtk_label_set_text(GTK_LABEL(app->status_label), "Failed to set encryption key");
        close(app->socket_fd);
        return;
    }
    
    gtk_label_set_text(GTK_LABEL(app->status_label), "Signing up...");
    
    // Send signup request
    char signup_msg[BUFFER_SIZE];
    snprintf(signup_msg, sizeof(signup_msg), "SIGNUP:%s:%s", username, password);
    send(app->socket_fd, signup_msg, strlen(signup_msg), 0);
    
    // Wait for signup response
    char response[BUFFER_SIZE];
    int bytes_received = recv(app->socket_fd, response, sizeof(response) - 1, 0);
    if (bytes_received > 0) {
        response[bytes_received] = '\0';
        if (strstr(response, "SIGNUP_SUCCESS") == response) {
            gtk_label_set_text(GTK_LABEL(app->status_label), "Signup successful! Please login.");
            // Clear password field for security
            gtk_entry_set_text(GTK_ENTRY(app->password_entry), "");
        } else {
            gtk_label_set_text(GTK_LABEL(app->status_label), "Signup failed. Username might already exist.");
        }
    } else {
        gtk_label_set_text(GTK_LABEL(app->status_label), "No response from server");
    }
    
    close(app->socket_fd);
    app->socket_fd = -1;
}

// Send message
void on_send_clicked(GtkButton *button, gpointer user_data) {
    (void)button; // Suppress unused parameter warning
    ChatClientGTK *app = (ChatClientGTK*)user_data;
    
    const char *message = gtk_entry_get_text(GTK_ENTRY(app->message_entry));
    if (strlen(message) > 0 && app->connected) {
        char full_message[BUFFER_SIZE];
        char encrypted_msg[BUFFER_SIZE];
        
        // Format message with username
        snprintf(full_message, sizeof(full_message), "%s: %s", app->username, message);
        
        // Display message on right side (own message)
        MessageData *msg_data = g_malloc(sizeof(MessageData));
        msg_data->message = g_strdup(message);
        msg_data->is_own_message = TRUE;
        g_idle_add(update_chat_display_wrapper, msg_data);
        
        // Try to encrypt message
        size_t encrypted_len;
        if (encrypt_message(full_message, encrypted_msg, &encrypted_len) == 0) {
            // Send encrypted message with prefix
            char final_message[BUFFER_SIZE];
            snprintf(final_message, sizeof(final_message), "[ENCRYPTED]%s", encrypted_msg);
            send(app->socket_fd, final_message, strlen(final_message), 0);
        } else {
            // Send plain text if encryption fails
            send(app->socket_fd, full_message, strlen(full_message), 0);
        }
        
        gtk_entry_set_text(GTK_ENTRY(app->message_entry), "");
    }
}

// Handle Enter key in message entry
gboolean on_message_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    (void)widget; // Suppress unused parameter warning
    if (event->keyval == GDK_Return || event->keyval == GDK_KP_Enter) {
        on_send_clicked(NULL, user_data);
        return TRUE;
    }
    return FALSE;
}

// Handle window close
gboolean on_window_delete(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
    (void)widget; // Suppress unused parameter warning
    (void)event;  // Suppress unused parameter warning
    ChatClientGTK *app = (ChatClientGTK*)user_data;
    
    if (app->connected) {
        app->connected = FALSE;
        close(app->socket_fd);
        pthread_join(app->receive_thread, NULL);
    }
    
    gtk_main_quit();
    return FALSE;
}

// Create main window
GtkWidget* create_chat_window() {
    client_app = g_malloc0(sizeof(ChatClientGTK));
    client_app->connected = FALSE;
    
    // Main window
    client_app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(client_app->window), "Encrypted Chat Client");
    gtk_window_set_default_size(GTK_WINDOW(client_app->window), 700, 500);
    gtk_container_set_border_width(GTK_CONTAINER(client_app->window), 10);
    
    // Main vbox
    GtkWidget *main_vbox = gtk_vbox_new(FALSE, 5);
    gtk_container_add(GTK_CONTAINER(client_app->window), main_vbox);
    
    // Connection frame
    GtkWidget *connection_frame = gtk_frame_new("Connection");
    GtkWidget *connection_table = gtk_table_new(5, 3, FALSE);
    gtk_container_add(GTK_CONTAINER(connection_frame), connection_table);
    gtk_container_set_border_width(GTK_CONTAINER(connection_table), 5);
    
    // Server IP
    GtkWidget *ip_label = gtk_label_new("Server IP:");
    client_app->server_ip_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(client_app->server_ip_entry), "127.0.0.1");
    gtk_table_attach(GTK_TABLE(connection_table), ip_label, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 5, 2);
    gtk_table_attach(GTK_TABLE(connection_table), client_app->server_ip_entry, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 5, 2);
    
    // Port
    GtkWidget *port_label = gtk_label_new("Port:");
    client_app->port_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(client_app->port_entry), "8888");
    gtk_table_attach(GTK_TABLE(connection_table), port_label, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 5, 2);
    gtk_table_attach(GTK_TABLE(connection_table), client_app->port_entry, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_FILL, 5, 2);
    
    // Username
    GtkWidget *username_label = gtk_label_new("Username:");
    client_app->username_entry = gtk_entry_new();
    gtk_table_attach(GTK_TABLE(connection_table), username_label, 0, 1, 2, 3, GTK_FILL, GTK_FILL, 5, 2);
    gtk_table_attach(GTK_TABLE(connection_table), client_app->username_entry, 1, 2, 2, 3, GTK_EXPAND | GTK_FILL, GTK_FILL, 5, 2);
    
    // Password
    GtkWidget *password_label = gtk_label_new("Password:");
    client_app->password_entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(client_app->password_entry), FALSE);
    gtk_table_attach(GTK_TABLE(connection_table), password_label, 0, 1, 3, 4, GTK_FILL, GTK_FILL, 5, 2);
    gtk_table_attach(GTK_TABLE(connection_table), client_app->password_entry, 1, 2, 3, 4, GTK_EXPAND | GTK_FILL, GTK_FILL, 5, 2);
    
    // Connection buttons
    GtkWidget *button_hbox = gtk_hbox_new(FALSE, 5);
    client_app->connect_button = gtk_button_new_with_label("Connect");
    client_app->signup_button = gtk_button_new_with_label("Sign Up");
    client_app->disconnect_button = gtk_button_new_with_label("Disconnect");
    gtk_widget_set_sensitive(client_app->disconnect_button, FALSE);
    
    gtk_box_pack_start(GTK_BOX(button_hbox), client_app->connect_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(button_hbox), client_app->signup_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(button_hbox), client_app->disconnect_button, TRUE, TRUE, 0);
    gtk_table_attach(GTK_TABLE(connection_table), button_hbox, 0, 2, 4, 5, GTK_EXPAND | GTK_FILL, GTK_FILL, 5, 2);
    
    // Status label
    client_app->status_label = gtk_label_new("Not connected");
    gtk_table_attach(GTK_TABLE(connection_table), client_app->status_label, 2, 3, 0, 5, GTK_FILL, GTK_FILL, 5, 2);
    
    gtk_box_pack_start(GTK_BOX(main_vbox), connection_frame, FALSE, FALSE, 0);
    
    // Chat area
    GtkWidget *chat_frame = gtk_frame_new("Chat");
    GtkWidget *chat_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(chat_scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_set_border_width(GTK_CONTAINER(chat_scroll), 5);
    
    client_app->chat_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(client_app->chat_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(client_app->chat_view), GTK_WRAP_WORD);
    client_app->chat_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(client_app->chat_view));
    
    // Set font
    PangoFontDescription *font_desc = pango_font_description_from_string("Monospace 10");
    gtk_widget_modify_font(client_app->chat_view, font_desc);
    pango_font_description_free(font_desc);
    
    gtk_container_add(GTK_CONTAINER(chat_scroll), client_app->chat_view);
    gtk_container_add(GTK_CONTAINER(chat_frame), chat_scroll);
    
    gtk_box_pack_start(GTK_BOX(main_vbox), chat_frame, TRUE, TRUE, 0);
    
    // Message input frame
    GtkWidget *input_frame = gtk_frame_new("Send Message");
    GtkWidget *input_hbox = gtk_hbox_new(FALSE, 5);
    gtk_container_set_border_width(GTK_CONTAINER(input_hbox), 5);
    
    client_app->message_entry = gtk_entry_new();
    client_app->send_button = gtk_button_new_with_label("Send");
    
    gtk_widget_set_sensitive(client_app->send_button, FALSE);
    gtk_widget_set_sensitive(client_app->message_entry, FALSE);
    
    gtk_box_pack_start(GTK_BOX(input_hbox), client_app->message_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(input_hbox), client_app->send_button, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(input_frame), input_hbox);
    
    gtk_box_pack_start(GTK_BOX(main_vbox), input_frame, FALSE, FALSE, 0);
    
    // Connect signals
    g_signal_connect(client_app->connect_button, "clicked", G_CALLBACK(on_connect_clicked), client_app);
    g_signal_connect(client_app->signup_button, "clicked", G_CALLBACK(on_signup_clicked), client_app);
    g_signal_connect(client_app->disconnect_button, "clicked", G_CALLBACK(on_disconnect_clicked), client_app);
    g_signal_connect(client_app->send_button, "clicked", G_CALLBACK(on_send_clicked), client_app);
    g_signal_connect(client_app->message_entry, "key-press-event", G_CALLBACK(on_message_key_press), client_app);
    g_signal_connect(client_app->window, "delete-event", G_CALLBACK(on_window_delete), client_app);
    
    return client_app->window;
}

// Encrypt message function
int encrypt_message(const char *input, char *output, size_t *output_len)
{
    char padded_input[BUFFER_SIZE];
    char encrypted_binary[BUFFER_SIZE];
    size_t padded_len;
    
    padded_len = crypto_pad_data(input, padded_input, strlen(input));
    
    if (crypto_encrypt(padded_input, encrypted_binary, padded_len) < 0) {
        return -1;
    }
    
    // Convert binary encrypted data to hex string
    crypto_bin_to_hex(encrypted_binary, padded_len, output);
    *output_len = padded_len * 2; // Hex string is twice the length
    
    return 0;
}

// Decrypt message function
int decrypt_message(const char *input, size_t input_len, char *output)
{
    char encrypted_binary[BUFFER_SIZE];
    char decrypted_padded[BUFFER_SIZE];
    size_t unpadded_len;
    size_t binary_len;
    
    // Convert hex string back to binary
    binary_len = crypto_hex_to_bin(input, encrypted_binary);
    if (binary_len == 0) {
        return -1;
    }
    
    if (crypto_decrypt(encrypted_binary, decrypted_padded, binary_len) < 0) {
        return -1;
    }
    
    unpadded_len = crypto_unpad_data(decrypted_padded, output, binary_len);
    output[unpadded_len] = '\0';
    
    return 0;
}

int main(int argc, char *argv[]) {
    // Initialize crypto library
    if (crypto_init() < 0) {
        fprintf(stderr, "Failed to initialize crypto library\n");
        return 1;
    }
    
    // Set default key
    char key[8] = "mykey123";
    if (crypto_set_key(key) < 0) {
        fprintf(stderr, "Failed to set encryption key\n");
        crypto_cleanup();
        return 1;
    }
    
    gtk_init(&argc, &argv);
    
    GtkWidget *window = create_chat_window();
    gtk_widget_show_all(window);
    
    gtk_main();
    
    if (client_app) {
        if (client_app->connected) {
            close(client_app->socket_fd);
        }
        g_free(client_app);
    }
    
    crypto_cleanup();
    return 0;
}