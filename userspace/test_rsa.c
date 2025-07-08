#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <pthread.h>
#include "crypto_lib.h"

int main()
{
    struct rsa_key public_key, private_key;
    char original_data[] = "Hello RSA!";
    char encrypted_data[RSA_BLOCK_SIZE];
    char decrypted_data[RSA_BLOCK_SIZE];
    size_t encrypted_len, decrypted_len;
    int i;
    
    printf("=== RSA Key Exchange Test ===\n");
    
    // Initialize crypto driver
    printf("Initializing crypto driver...\n");
    if (crypto_init() < 0) {
        fprintf(stderr, "Failed to initialize crypto driver\n");
        return 1;
    }
    printf("Crypto driver initialized successfully\n");
    
    // Test RSA key generation
    printf("Generating RSA key pair...\n");
    if (crypto_rsa_generate_keypair(&public_key, &private_key) < 0) {
        fprintf(stderr, "Failed to generate RSA key pair\n");
        crypto_cleanup();
        return 1;
    }
    
    printf("RSA key pair generated successfully\n");
    printf("Public key (first 16 bytes): ");
    for (i = 0; i < 16; i++) {
        printf("%02x", public_key.e[i]);
    }
    printf("\n");
    
    // Test RSA encryption
    printf("Encrypting data: '%s'\n", original_data);
    if (crypto_rsa_encrypt(original_data, strlen(original_data), encrypted_data, &encrypted_len, &public_key) < 0) {
        fprintf(stderr, "Failed to encrypt data\n");
        crypto_cleanup();
        return 1;
    }
    
    printf("Data encrypted successfully (length: %zu)\n", encrypted_len);
    printf("Encrypted data (first 16 bytes): ");
    for (i = 0; i < 16 && i < encrypted_len; i++) {
        printf("%02x", (unsigned char)encrypted_data[i]);
    }
    printf("\n");
    
    // Test RSA decryption
    printf("Decrypting data...\n");
    if (crypto_rsa_decrypt(encrypted_data, encrypted_len, decrypted_data, &decrypted_len, &private_key) < 0) {
        fprintf(stderr, "Failed to decrypt data\n");
        crypto_cleanup();
        return 1;
    }
    
    printf("Data decrypted successfully (length: %zu)\n", decrypted_len);
    printf("Decrypted data: '%.*s'\n", (int)decrypted_len, decrypted_data);
    
    // Verify correctness
    if (strncmp(original_data, decrypted_data, strlen(original_data)) == 0) {
        printf("✓ RSA encryption/decryption test PASSED!\n");
    } else {
        printf("✗ RSA encryption/decryption test FAILED!\n");
    }
    
    crypto_cleanup();
    return 0;
}
