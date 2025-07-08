#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>

#include "crypto_lib.h"

#define PORT 8888
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024
#define USERNAME_SIZE 32
#define PASSWORD_SIZE 32

struct client_info {
    int socket;
    struct sockaddr_in address;
    char username[USERNAME_SIZE];
    int authenticated;
    pthread_t thread;
};

struct user_account {
    char username[USERNAME_SIZE];
    char password_hash[SHA1_DIGEST_SIZE * 2 + 1]; // hex string
};

// Simple user database (in real app, use proper database)
static struct user_account users[] = {
    {"admin", ""},
    {"user1", ""},
    {"user2", ""},
    {"", ""}  // sentinel
};

static struct client_info clients[MAX_CLIENTS];
static int client_count = 0;
static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
static int server_socket;
static volatile int server_running = 1;

// Function prototypes
void *client_handler(void *arg);
void broadcast_message(const char *message, int sender_socket);
int authenticate_user(const char *username, const char *password);
void initialize_users(void);
void cleanup_client(struct client_info *client);
void signal_handler(int sig);
int encrypt_message(const char *input, char *output, size_t *output_len);
int decrypt_message(const char *input, size_t input_len, char *output);

int main()
{
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    int client_socket;
    int opt = 1;
    
    printf("=== Chat Server với mã hóa DES và SHA1 ===\n");
    
    // Initialize crypto driver
    if (crypto_init() < 0) {
        fprintf(stderr, "Failed to initialize crypto driver\n");
        exit(1);
    }
    
    // Initialize user database
    initialize_users();
    
    // Setup signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        crypto_cleanup();
        exit(1);
    }
    
    // Set socket options
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(server_socket);
        crypto_cleanup();
        exit(1);
    }
    
    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    // Bind socket
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        crypto_cleanup();
        exit(1);
    }
    
    // Listen for connections
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        close(server_socket);
        crypto_cleanup();
        exit(1);
    }
    
    printf("Chat server started on port %d\n", PORT);
    printf("Waiting for connections...\n");
    
    // Initialize clients array
    memset(clients, 0, sizeof(clients));
    
    // Accept connections
    client_len = sizeof(client_addr);
    while (server_running) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket < 0) {
            if (server_running) {
                perror("Accept failed");
            }
            continue;
        }
        
        pthread_mutex_lock(&clients_mutex);
        
        // Find free slot for client
        int slot = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].socket == 0) {
                slot = i;
                break;
            }
        }
        
        if (slot == -1) {
            printf("Max clients reached, rejecting connection\n");
            close(client_socket);
            pthread_mutex_unlock(&clients_mutex);
            continue;
        }
        
        // Initialize client info
        clients[slot].socket = client_socket;
        clients[slot].address = client_addr;
        clients[slot].authenticated = 0;
        memset(clients[slot].username, 0, sizeof(clients[slot].username));
        
        client_count++;
        
        // Create thread for client
        if (pthread_create(&clients[slot].thread, NULL, client_handler, &clients[slot]) != 0) {
            perror("Thread creation failed");
            close(client_socket);
            clients[slot].socket = 0;
            client_count--;
        } else {
            printf("New client connected from %s:%d (slot %d)\n", 
                   inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), slot);
        }
        
        pthread_mutex_unlock(&clients_mutex);
    }
    
    // Cleanup
    close(server_socket);
    crypto_cleanup();
    printf("Server shutdown complete\n");
    
    return 0;
}

void *client_handler(void *arg)
{
    struct client_info *client = (struct client_info*)arg;
    char buffer[BUFFER_SIZE];
    char decrypted_buffer[BUFFER_SIZE];
    char encrypted_response[BUFFER_SIZE];
    char username[USERNAME_SIZE];
    char password[PASSWORD_SIZE];
    char shared_des_key[DES_KEY_SIZE];
    size_t encrypted_len;
    ssize_t bytes_received;
    
    // RSA Key Exchange phase
    printf("Starting key exchange with client %s:%d\n", 
           inet_ntoa(client->address.sin_addr), ntohs(client->address.sin_port));
    
    if (perform_key_exchange_server(client->socket, shared_des_key) < 0) {
        fprintf(stderr, "Key exchange failed with client\n");
        goto cleanup;
    }
    
    // Set the exchanged DES key for this client session
    if (crypto_set_key(shared_des_key) < 0) {
        fprintf(stderr, "Failed to set shared DES key\n");
        goto cleanup;
    }
    
    printf("Key exchange completed, using shared DES key for client %s:%d\n",
           inet_ntoa(client->address.sin_addr), ntohs(client->address.sin_port));
    
    // Authentication phase
    send(client->socket, "USERNAME:", 9, 0);
    bytes_received = recv(client->socket, username, sizeof(username) - 1, 0);
    if (bytes_received <= 0) {
        goto cleanup;
    }
    username[bytes_received] = '\0';
    
    // Remove newline if present
    char *newline = strchr(username, '\n');
    if (newline) *newline = '\0';
    newline = strchr(username, '\r');
    if (newline) *newline = '\0';
    
    send(client->socket, "PASSWORD:", 9, 0);
    bytes_received = recv(client->socket, password, sizeof(password) - 1, 0);
    if (bytes_received <= 0) {
        goto cleanup;
    }
    password[bytes_received] = '\0';
    
    // Remove newline if present
    newline = strchr(password, '\n');
    if (newline) *newline = '\0';
    newline = strchr(password, '\r');
    if (newline) *newline = '\0';
    
    if (authenticate_user(username, password)) {
        strcpy(client->username, username);
        client->authenticated = 1;
        
        const char *welcome_msg = "AUTHENTICATED\nWelcome to encrypted chat!\n";
        if (encrypt_message(welcome_msg, encrypted_response, &encrypted_len) == 0) {
            send(client->socket, encrypted_response, encrypted_len, 0);
        }
        
        // Broadcast user joined
        char join_msg[256];
        snprintf(join_msg, sizeof(join_msg), "*** %s joined the chat ***\n", username);
        broadcast_message(join_msg, client->socket);
        
        printf("User %s authenticated successfully\n", username);
    } else {
        const char *error_msg = "AUTHENTICATION_FAILED\n";
        send(client->socket, error_msg, strlen(error_msg), 0);
        printf("Authentication failed for user %s\n", username);
        goto cleanup;
    }
    
    // Chat phase
    while (server_running && client->authenticated) {
        bytes_received = recv(client->socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            break;
        }
        
        // Decrypt received message
        if (decrypt_message(buffer, bytes_received, decrypted_buffer) < 0) {
            printf("Failed to decrypt message from %s\n", client->username);
            continue;
        }
        
        // Handle special commands
        if (strncmp(decrypted_buffer, "/quit", 5) == 0) {
            break;
        }
        
        // Broadcast message to all clients
        char formatted_msg[BUFFER_SIZE];
        snprintf(formatted_msg, sizeof(formatted_msg), "%s: %s", 
                 client->username, decrypted_buffer);
        broadcast_message(formatted_msg, client->socket);
        
        printf("Message from %s: %s", client->username, decrypted_buffer);
    }
    
cleanup:
    if (client->authenticated) {
        char leave_msg[256];
        snprintf(leave_msg, sizeof(leave_msg), "*** %s left the chat ***\n", client->username);
        broadcast_message(leave_msg, client->socket);
        printf("User %s disconnected\n", client->username);
    }
    
    cleanup_client(client);
    return NULL;
}

void broadcast_message(const char *message, int sender_socket)
{
    char encrypted_msg[BUFFER_SIZE];
    size_t encrypted_len;
    
    if (encrypt_message(message, encrypted_msg, &encrypted_len) < 0) {
        printf("Failed to encrypt broadcast message\n");
        return;
    }
    
    pthread_mutex_lock(&clients_mutex);
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != 0 && 
            clients[i].authenticated && 
            clients[i].socket != sender_socket) {
            
            if (send(clients[i].socket, encrypted_msg, encrypted_len, 0) < 0) {
                printf("Failed to send message to client %s\n", clients[i].username);
            }
        }
    }
    
    pthread_mutex_unlock(&clients_mutex);
}

int authenticate_user(const char *username, const char *password)
{
    char password_hash[SHA1_DIGEST_SIZE];
    char password_hash_hex[SHA1_DIGEST_SIZE * 2 + 1];
    
    // Hash the provided password
    if (crypto_hash(password, strlen(password), password_hash) < 0) {
        return 0;
    }
    
    // Convert to hex string
    for (int i = 0; i < SHA1_DIGEST_SIZE; i++) {
        sprintf(&password_hash_hex[i * 2], "%02x", (unsigned char)password_hash[i]);
    }
    password_hash_hex[SHA1_DIGEST_SIZE * 2] = '\0';
    
    // Check against user database
    for (int i = 0; users[i].username[0] != '\0'; i++) {
        if (strcmp(users[i].username, username) == 0) {
            return strcmp(users[i].password_hash, password_hash_hex) == 0;
        }
    }
    
    return 0;
}

void initialize_users(void)
{
    const char *passwords[] = {"admin123", "password1", "password2"};
    char password_hash[SHA1_DIGEST_SIZE];
    
    printf("Initializing user database...\n");
    
    for (int i = 0; users[i].username[0] != '\0' && i < 3; i++) {
        if (crypto_hash(passwords[i], strlen(passwords[i]), password_hash) == 0) {
            for (int j = 0; j < SHA1_DIGEST_SIZE; j++) {
                sprintf(&users[i].password_hash[j * 2], "%02x", (unsigned char)password_hash[j]);
            }
            users[i].password_hash[SHA1_DIGEST_SIZE * 2] = '\0';
            printf("User: %s, Password: %s\n", users[i].username, passwords[i]);
        }
    }
}

void cleanup_client(struct client_info *client)
{
    pthread_mutex_lock(&clients_mutex);
    
    close(client->socket);
    client->socket = 0;
    client->authenticated = 0;
    memset(client->username, 0, sizeof(client->username));
    client_count--;
    
    pthread_mutex_unlock(&clients_mutex);
}

void signal_handler(int sig)
{
    printf("\nReceived signal %d, shutting down server...\n", sig);
    server_running = 0;
    
    // Close server socket to break accept loop
    if (server_socket >= 0) {
        close(server_socket);
    }
    
    // Close all client connections
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket != 0) {
            close(clients[i].socket);
            clients[i].socket = 0;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

int encrypt_message(const char *input, char *output, size_t *output_len)
{
    char padded_input[BUFFER_SIZE];
    size_t padded_len;
    
    padded_len = crypto_pad_data(input, padded_input, strlen(input));
    
    if (crypto_encrypt(padded_input, output, padded_len) < 0) {
        return -1;
    }
    
    *output_len = padded_len;
    return 0;
}

int decrypt_message(const char *input, size_t input_len, char *output)
{
    char decrypted_padded[BUFFER_SIZE];
    size_t unpadded_len;
    
    if (crypto_decrypt(input, decrypted_padded, input_len) < 0) {
        return -1;
    }
    
    unpadded_len = crypto_unpad_data(decrypted_padded, output, input_len);
    output[unpadded_len] = '\0';
    
    return 0;
}
