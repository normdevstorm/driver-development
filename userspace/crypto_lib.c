#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/socket.h>
#include <time.h>

#include "crypto_lib.h"

static int crypto_fd = -1;

int crypto_init(void)
{
    crypto_fd = open("/dev/crypto_dev", O_RDWR);
    if (crypto_fd < 0) {
        perror("Failed to open crypto device");
        return -1;
    }
    return 0;
}

void crypto_cleanup(void)
{
    if (crypto_fd >= 0) {
        close(crypto_fd);
        crypto_fd = -1;
    }
}

int crypto_set_key(const char *key)
{
    if (crypto_fd < 0) {
        fprintf(stderr, "Crypto device not initialized\n");
        return -1;
    }
    
    // Note: Don't use strlen() for binary key data as it may contain null bytes
    // The key is always DES_KEY_SIZE (8) bytes for DES
    
    if (ioctl(crypto_fd, CRYPTO_IOC_SET_KEY, key) < 0) {
        perror("Failed to set encryption key");
        return -1;
    }
    
    return 0;
}

int crypto_encrypt(const char *input, char *output, size_t length)
{
    struct crypto_data data;
    
    if (crypto_fd < 0) {
        fprintf(stderr, "Crypto device not initialized\n");
        return -1;
    }
    
    // Ensure length is multiple of DES block size
    if (length % DES_BLOCK_SIZE != 0) {
        fprintf(stderr, "Input length must be multiple of %d bytes\n", DES_BLOCK_SIZE);
        return -1;
    }
    
    data.input = (char *)input;
    data.output = output;
    data.length = length;
    
    if (ioctl(crypto_fd, CRYPTO_IOC_ENCRYPT, &data) < 0) {
        perror("Encryption failed");
        return -1;
    }
    
    return 0;
}

int crypto_decrypt(const char *input, char *output, size_t length)
{
    struct crypto_data data;
    
    if (crypto_fd < 0) {
        fprintf(stderr, "Crypto device not initialized\n");
        return -1;
    }
    
    // Ensure length is multiple of DES block size
    if (length % DES_BLOCK_SIZE != 0) {
        fprintf(stderr, "Input length must be multiple of %d bytes\n", DES_BLOCK_SIZE);
        return -1;
    }
    
    data.input = (char *)input;
    data.output = output;
    data.length = length;
    
    if (ioctl(crypto_fd, CRYPTO_IOC_DECRYPT, &data) < 0) {
        perror("Decryption failed");
        return -1;
    }
    
    return 0;
}

int crypto_hash(const char *input, size_t input_length, char *output)
{
    struct hash_data data;
    
    if (crypto_fd < 0) {
        fprintf(stderr, "Crypto device not initialized\n");
        return -1;
    }
    
    data.input = (char *)input;
    data.output = output;
    data.input_length = input_length;
    
    if (ioctl(crypto_fd, CRYPTO_IOC_HASH, &data) < 0) {
        perror("Hashing failed");
        return -1;
    }
    
    return 0;
}

// Utility function to pad data to DES block size
size_t crypto_pad_data(const char *input, char *output, size_t length)
{
    size_t padded_length = ((length + DES_BLOCK_SIZE - 1) / DES_BLOCK_SIZE) * DES_BLOCK_SIZE;
    
    memcpy(output, input, length);
    
    // PKCS#5 padding
    int pad_value = padded_length - length;
    for (size_t i = length; i < padded_length; i++) {
        output[i] = pad_value;
    }
    
    return padded_length;
}

// Utility function to remove padding
size_t crypto_unpad_data(const char *input, char *output, size_t length)
{
    if (length == 0 || length % DES_BLOCK_SIZE != 0) {
        return 0;
    }
    
    int pad_value = input[length - 1];
    if (pad_value <= 0 || pad_value > DES_BLOCK_SIZE) {
        // Invalid padding
        memcpy(output, input, length);
        return length;
    }
    
    size_t unpadded_length = length - pad_value;
    
    // Verify padding
    for (int i = 0; i < pad_value; i++) {
        if (input[length - 1 - i] != pad_value) {
            // Invalid padding
            memcpy(output, input, length);
            return length;
        }
    }
    
    memcpy(output, input, unpadded_length);
    return unpadded_length;
}

void crypto_print_hex(const char *data, size_t length)
{
    for (size_t i = 0; i < length; i++) {
        printf("%02x", (unsigned char)data[i]);
    }
    printf("\n");
}

// Utility function to convert binary data to hex string
void crypto_bin_to_hex(const char *bin, size_t bin_len, char *hex) {
    const char hex_chars[] = "0123456789abcdef";
    for (size_t i = 0; i < bin_len; i++) {
        unsigned char byte = (unsigned char)bin[i];
        hex[i * 2] = hex_chars[byte >> 4];
        hex[i * 2 + 1] = hex_chars[byte & 0x0f];
    }
    hex[bin_len * 2] = '\0';
}

// Utility function to convert hex string to binary data
size_t crypto_hex_to_bin(const char *hex, char *bin) {
    size_t hex_len = strlen(hex);
    if (hex_len % 2 != 0) {
        return 0; // Invalid hex string
    }
    
    size_t bin_len = hex_len / 2;
    for (size_t i = 0; i < bin_len; i++) {
        char hex_byte[3] = {hex[i * 2], hex[i * 2 + 1], '\0'};
        bin[i] = (char)strtol(hex_byte, NULL, 16);
    }
    return bin_len;
}

// RSA functions
int crypto_rsa_generate_keypair(struct rsa_key *public_key, struct rsa_key *private_key)
{
    if (crypto_fd < 0) {
        fprintf(stderr, "Crypto device not initialized\n");
        return -1;
    }
    
    // For simplicity, we'll generate the key pair in userspace
    // In a production system, this should be done in kernel space
    
    // Initialize keys
    memset(public_key, 0, sizeof(struct rsa_key));
    memset(private_key, 0, sizeof(struct rsa_key));
    
    // Simple RSA key generation (for demonstration)
    // In production, use proper cryptographic libraries
    srand(time(NULL));
    
    // Generate simple keys (not cryptographically secure - for demo only)
    public_key->key_size = RSA_KEY_SIZE;
    private_key->key_size = RSA_KEY_SIZE;
    
    // Set simple values for demonstration
    for (int i = 0; i < RSA_KEY_SIZE; i++) {
        public_key->n[i] = rand() % 256;
        private_key->n[i] = public_key->n[i];  // Same modulus
        public_key->e[i] = (i < 4) ? 0x01 : 0x00;  // Simple public exponent
        private_key->d[i] = (i < 4) ? 0x01 : 0x00;  // Simple private exponent
    }
    
    return 0;
}

int crypto_rsa_encrypt(const char *input, size_t input_len, char *output, size_t *output_len, struct rsa_key *public_key)
{
    if (crypto_fd < 0) {
        fprintf(stderr, "Crypto device not initialized\n");
        return -1;
    }
    
    if (input_len > RSA_BLOCK_SIZE - 11) {  // PKCS#1 padding overhead
        fprintf(stderr, "Input too large for RSA encryption\n");
        return -1;
    }
    
    // Simple XOR encryption for demonstration (not secure)
    // In production, use proper RSA implementation
    *output_len = RSA_BLOCK_SIZE;
    memcpy(output, input, input_len);
    
    // Pad to RSA block size
    for (size_t i = input_len; i < RSA_BLOCK_SIZE; i++) {
        output[i] = (char)(RSA_BLOCK_SIZE - input_len);
    }
    
    // Simple XOR with public key
    for (size_t i = 0; i < RSA_BLOCK_SIZE; i++) {
        output[i] ^= public_key->e[i % RSA_KEY_SIZE];
    }
    
    return 0;
}

int crypto_rsa_decrypt(const char *input, size_t input_len, char *output, size_t *output_len, struct rsa_key *private_key)
{
    if (crypto_fd < 0) {
        fprintf(stderr, "Crypto device not initialized\n");
        return -1;
    }
    
    if (input_len != RSA_BLOCK_SIZE) {
        fprintf(stderr, "Invalid RSA ciphertext length\n");
        return -1;
    }
    
    // Simple XOR decryption for demonstration (not secure)
    // In production, use proper RSA implementation
    memcpy(output, input, input_len);
    
    // Simple XOR with private key
    for (size_t i = 0; i < RSA_BLOCK_SIZE; i++) {
        output[i] ^= private_key->d[i % RSA_KEY_SIZE];
    }
    
    // Remove padding
    size_t padding = (unsigned char)output[RSA_BLOCK_SIZE - 1];
    if (padding > 0 && padding < RSA_BLOCK_SIZE) {
        *output_len = RSA_BLOCK_SIZE - padding;
        output[*output_len] = '\0';
    } else {
        *output_len = RSA_BLOCK_SIZE;
    }
    
    return 0;
}

// Key exchange functions
int perform_key_exchange_server(int client_socket, char *shared_des_key)
{
    struct rsa_key server_public, server_private;
    struct rsa_key client_public;
    char buffer[RSA_BLOCK_SIZE];
    char decrypted_key[DES_KEY_SIZE];
    size_t decrypted_len;
    ssize_t bytes_received, bytes_sent;
    
    printf("Starting RSA key exchange (server side)...\n");
    
    // Generate server RSA key pair
    if (crypto_rsa_generate_keypair(&server_public, &server_private) < 0) {
        fprintf(stderr, "Failed to generate server RSA key pair\n");
        return -1;
    }
    
    // Send server public key to client
    bytes_sent = send(client_socket, &server_public, sizeof(server_public), 0);
    if (bytes_sent != sizeof(server_public)) {
        fprintf(stderr, "Failed to send server public key\n");
        return -1;
    }
    
    // Receive client public key
    bytes_received = recv(client_socket, &client_public, sizeof(client_public), 0);
    if (bytes_received != sizeof(client_public)) {
        fprintf(stderr, "Failed to receive client public key\n");
        return -1;
    }
    
    // Receive encrypted DES key from client
    bytes_received = recv(client_socket, buffer, RSA_BLOCK_SIZE, 0);
    if (bytes_received != RSA_BLOCK_SIZE) {
        fprintf(stderr, "Failed to receive encrypted DES key\n");
        return -1;
    }
    
    // Decrypt DES key with server private key
    if (crypto_rsa_decrypt(buffer, RSA_BLOCK_SIZE, decrypted_key, &decrypted_len, &server_private) < 0) {
        fprintf(stderr, "Failed to decrypt DES key\n");
        return -1;
    }
    
    // Copy the decrypted key (ensure it's exactly DES_KEY_SIZE bytes)
    if (decrypted_len >= DES_KEY_SIZE) {
        memcpy(shared_des_key, decrypted_key, DES_KEY_SIZE);
    } else {
        // Pad with zeros if needed
        memcpy(shared_des_key, decrypted_key, decrypted_len);
        memset(shared_des_key + decrypted_len, 0, DES_KEY_SIZE - decrypted_len);
    }
    
    printf("Key exchange completed successfully (server)\n");
    return 0;
}

int perform_key_exchange_client(int server_socket, char *shared_des_key)
{
    struct rsa_key client_public, client_private;
    struct rsa_key server_public;
    char buffer[RSA_BLOCK_SIZE];
    char des_key[DES_KEY_SIZE];
    size_t encrypted_len;
    ssize_t bytes_received, bytes_sent;
    
    printf("Starting RSA key exchange (client side)...\n");
    
    // Generate client RSA key pair
    if (crypto_rsa_generate_keypair(&client_public, &client_private) < 0) {
        fprintf(stderr, "Failed to generate client RSA key pair\n");
        return -1;
    }
    
    // Receive server public key
    bytes_received = recv(server_socket, &server_public, sizeof(server_public), 0);
    if (bytes_received != sizeof(server_public)) {
        fprintf(stderr, "Failed to receive server public key\n");
        return -1;
    }
    
    // Send client public key to server
    bytes_sent = send(server_socket, &client_public, sizeof(client_public), 0);
    if (bytes_sent != sizeof(client_public)) {
        fprintf(stderr, "Failed to send client public key\n");
        return -1;
    }
    
    // Generate random DES key
    srand(time(NULL));
    for (int i = 0; i < DES_KEY_SIZE; i++) {
        des_key[i] = rand() % 256;
    }
    
    // Encrypt DES key with server public key
    if (crypto_rsa_encrypt(des_key, DES_KEY_SIZE, buffer, &encrypted_len, &server_public) < 0) {
        fprintf(stderr, "Failed to encrypt DES key\n");
        return -1;
    }
    
    // Send encrypted DES key to server
    bytes_sent = send(server_socket, buffer, encrypted_len, 0);
    if (bytes_sent != (ssize_t)encrypted_len) {
        fprintf(stderr, "Failed to send encrypted DES key\n");
        return -1;
    }
    
    // Copy the generated DES key
    memcpy(shared_des_key, des_key, DES_KEY_SIZE);
    
    printf("Key exchange completed successfully (client)\n");
    return 0;
}
