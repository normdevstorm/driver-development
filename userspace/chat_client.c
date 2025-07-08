#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <termios.h>

#include "crypto_lib.h"

#define SERVER_IP "127.0.0.1"
#define PORT 8888
#define BUFFER_SIZE 1024

static int client_socket;
static volatile int client_running = 1;
static pthread_t receive_thread;

// Function prototypes
void *receive_messages(void *arg);
void signal_handler(int sig);
int encrypt_message(const char *input, char *output, size_t *output_len);
int decrypt_message(const char *input, size_t input_len, char *output);
void get_password(char *password, size_t max_len);

int main()
{
    struct sockaddr_in server_addr;
    char username[32];
    char password[32];
    char message[BUFFER_SIZE];
    char encrypted_message[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];
    char shared_des_key[DES_KEY_SIZE];
    size_t encrypted_len;
    ssize_t bytes_received;
    
    printf("=== Chat Client với mã hóa DES và SHA1 ===\n");
    
    // Initialize crypto driver
    if (crypto_init() < 0) {
        fprintf(stderr, "Failed to initialize crypto driver\n");
        exit(1);
    }
    
    // Setup signal handler
    signal(SIGINT, signal_handler);
    
    // Create socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Socket creation failed");
        crypto_cleanup();
        exit(1);
    }
    
    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(client_socket);
        crypto_cleanup();
        exit(1);
    }
    
    // Connect to server
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(client_socket);
        crypto_cleanup();
        exit(1);
    }
    
    printf("Connected to chat server\n");
    
    // RSA Key Exchange phase
    printf("Starting key exchange with server...\n");
    
    if (perform_key_exchange_client(client_socket, shared_des_key) < 0) {
        fprintf(stderr, "Key exchange failed\n");
        close(client_socket);
        crypto_cleanup();
        exit(1);
    }
    
    // Set the exchanged DES key for this session
    if (crypto_set_key(shared_des_key) < 0) {
        fprintf(stderr, "Failed to set shared DES key\n");
        close(client_socket);
        crypto_cleanup();
        exit(1);
    }
    
    printf("Key exchange completed, using shared DES key\n");
    
    // Authentication
    // Wait for username prompt
    bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {
        printf("Connection closed by server\n");
        goto cleanup;
    }
    buffer[bytes_received] = '\0';
    printf("%s ", buffer);
    
    // Send username
    if (fgets(username, sizeof(username), stdin) == NULL) {
        goto cleanup;
    }
    send(client_socket, username, strlen(username), 0);
    
    // Wait for password prompt
    bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {
        printf("Connection closed by server\n");
        goto cleanup;
    }
    buffer[bytes_received] = '\0';
    printf("%s ", buffer);
    
    // Get password (hidden input)
    get_password(password, sizeof(password));
    printf("\n");
    send(client_socket, password, strlen(password), 0);
    
    // Wait for authentication result
    bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {
        printf("Connection closed by server\n");
        goto cleanup;
    }
    
    // Try to decrypt the response
    char decrypted_response[BUFFER_SIZE];
    if (decrypt_message(buffer, bytes_received, decrypted_response) == 0) {
        printf("%s", decrypted_response);
        
        if (strstr(decrypted_response, "AUTHENTICATED") != NULL) {
            printf("Authentication successful!\n");
        } else {
            printf("Authentication failed\n");
            goto cleanup;
        }
    } else {
        // If decryption fails, it might be plain text error
        buffer[bytes_received] = '\0';
        printf("%s", buffer);
        if (strstr(buffer, "AUTHENTICATION_FAILED") != NULL) {
            printf("Authentication failed\n");
            goto cleanup;
        }
    }
    
    // Start receive thread
    if (pthread_create(&receive_thread, NULL, receive_messages, NULL) != 0) {
        perror("Failed to create receive thread");
        goto cleanup;
    }
    
    // Main message loop
    printf("\nYou can start chatting! Type '/quit' to exit.\n");
    printf("> ");
    fflush(stdout);
    
    while (client_running && fgets(message, sizeof(message), stdin) != NULL) {
        if (!client_running) break;
        
        // Check for quit command
        if (strncmp(message, "/quit", 5) == 0) {
            break;
        }
        
        // Encrypt and send message
        if (encrypt_message(message, encrypted_message, &encrypted_len) == 0) {
            if (send(client_socket, encrypted_message, encrypted_len, 0) < 0) {
                perror("Send failed");
                break;
            }
        } else {
            printf("Failed to encrypt message\n");
        }
        
        if (client_running) {
            printf("> ");
            fflush(stdout);
        }
    }
    
    // Wait for receive thread to finish
    pthread_cancel(receive_thread);
    pthread_join(receive_thread, NULL);
    
cleanup:
    close(client_socket);
    crypto_cleanup();
    printf("\nDisconnected from server\n");
    
    return 0;
}

void *receive_messages(void *arg)
{
    char buffer[BUFFER_SIZE];
    char decrypted_message[BUFFER_SIZE];
    ssize_t bytes_received;
    
    while (client_running) {
        bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0) {
            if (client_running) {
                printf("\nConnection lost\n");
            }
            client_running = 0;
            break;
        }
        
        // Decrypt received message
        if (decrypt_message(buffer, bytes_received, decrypted_message) == 0) {
            printf("\r%s> ", decrypted_message);
            fflush(stdout);
        } else {
            printf("\r[Encrypted message - decryption failed]\n> ");
            fflush(stdout);
        }
    }
    
    return NULL;
}

void signal_handler(int sig)
{
    printf("\nReceived signal %d, disconnecting...\n", sig);
    client_running = 0;
    
    if (client_socket >= 0) {
        close(client_socket);
    }
    
    exit(0);
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

void get_password(char *password, size_t max_len)
{
    struct termios old_termios, new_termios;
    int i = 0;
    char ch;
    
    // Get current terminal settings
    tcgetattr(STDIN_FILENO, &old_termios);
    new_termios = old_termios;
    
    // Disable echo
    new_termios.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_termios);
    
    // Read password character by character
    while (i < max_len - 1 && (ch = getchar()) != '\n' && ch != EOF) {
        password[i++] = ch;
        printf("*");  // Print asterisk for each character
        fflush(stdout);
    }
    password[i] = '\0';
    
    // Restore terminal settings
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_termios);
}
