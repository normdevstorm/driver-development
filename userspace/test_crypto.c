#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "crypto_lib.h"

void test_des_encryption(void)
{
    printf("Testing DES encryption/decryption...\n");
    
    const char *plaintext = "Hello World!";
    char padded_input[32];
    char encrypted[32];
    char decrypted_padded[32];
    char decrypted[32];
    
    size_t padded_len = crypto_pad_data(plaintext, padded_input, strlen(plaintext));
    printf("Original: %s (length: %zu)\n", plaintext, strlen(plaintext));
    printf("Padded length: %zu\n", padded_len);
    
    // Test encryption
    if (crypto_encrypt(padded_input, encrypted, padded_len) == 0) {
        printf("Encryption successful\n");
        printf("Encrypted data: ");
        crypto_print_hex(encrypted, padded_len);
    } else {
        printf("Encryption failed\n");
        return;
    }
    
    // Test decryption
    if (crypto_decrypt(encrypted, decrypted_padded, padded_len) == 0) {
        printf("Decryption successful\n");
        
        size_t unpadded_len = crypto_unpad_data(decrypted_padded, decrypted, padded_len);
        decrypted[unpadded_len] = '\0';
        
        printf("Decrypted: %s (length: %zu)\n", decrypted, unpadded_len);
        
        if (strcmp(plaintext, decrypted) == 0) {
            printf("✓ DES encryption/decryption test PASSED\n");
        } else {
            printf("✗ DES encryption/decryption test FAILED\n");
        }
    } else {
        printf("Decryption failed\n");
    }
    
    printf("\n");
}

void test_sha1_hashing(void)
{
    printf("Testing SHA1 hashing...\n");
    
    const char *input = "password123";
    char hash[SHA1_DIGEST_SIZE];
    
    printf("Input: %s\n", input);
    
    if (crypto_hash(input, strlen(input), hash) == 0) {
        printf("SHA1 hash: ");
        crypto_print_hex(hash, SHA1_DIGEST_SIZE);
        printf("✓ SHA1 hashing test PASSED\n");
    } else {
        printf("✗ SHA1 hashing test FAILED\n");
    }
    
    printf("\n");
}

void test_key_setting(void)
{
    printf("Testing DES key setting...\n");
    
    const char custom_key[DES_KEY_SIZE] = {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10};
    
    if (crypto_set_key(custom_key) == 0) {
        printf("✓ Custom key set successfully\n");
        
        // Test with custom key
        const char *plaintext = "TestKey!";
        char padded_input[16];
        char encrypted[16];
        char decrypted_padded[16];
        char decrypted[16];
        
        size_t padded_len = crypto_pad_data(plaintext, padded_input, strlen(plaintext));
        
        if (crypto_encrypt(padded_input, encrypted, padded_len) == 0 &&
            crypto_decrypt(encrypted, decrypted_padded, padded_len) == 0) {
            
            size_t unpadded_len = crypto_unpad_data(decrypted_padded, decrypted, padded_len);
            decrypted[unpadded_len] = '\0';
            
            if (strcmp(plaintext, decrypted) == 0) {
                printf("✓ Custom key encryption/decryption test PASSED\n");
            } else {
                printf("✗ Custom key encryption/decryption test FAILED\n");
            }
        } else {
            printf("✗ Custom key encryption/decryption test FAILED\n");
        }
    } else {
        printf("✗ Failed to set custom key\n");
    }
    
    printf("\n");
}

void test_padding(void)
{
    printf("Testing padding functions...\n");
    
    const char *inputs[] = {
        "A",          // 1 byte
        "AB",         // 2 bytes  
        "ABCDEFG",    // 7 bytes
        "ABCDEFGH",   // 8 bytes (exact block)
        "ABCDEFGHI",  // 9 bytes
        "Hello World! This is a longer message for testing."
    };
    
    int num_tests = sizeof(inputs) / sizeof(inputs[0]);
    
    for (int i = 0; i < num_tests; i++) {
        char padded[128];
        char unpadded[128];
        
        size_t original_len = strlen(inputs[i]);
        size_t padded_len = crypto_pad_data(inputs[i], padded, original_len);
        size_t unpadded_len = crypto_unpad_data(padded, unpadded, padded_len);
        
        unpadded[unpadded_len] = '\0';
        
        printf("Test %d: '%s' (%zu bytes)\n", i + 1, inputs[i], original_len);
        printf("  Padded length: %zu\n", padded_len);
        printf("  Unpadded: '%s' (%zu bytes)\n", unpadded, unpadded_len);
        
        if (strcmp(inputs[i], unpadded) == 0 && unpadded_len == original_len) {
            printf("  ✓ PASSED\n");
        } else {
            printf("  ✗ FAILED\n");
        }
    }
    
    printf("\n");
}

int main()
{
    printf("=== Crypto Library Test Suite ===\n\n");
    
    // Initialize crypto driver
    if (crypto_init() < 0) {
        fprintf(stderr, "Failed to initialize crypto driver\n");
        fprintf(stderr, "Make sure the crypto_driver module is loaded:\n");
        fprintf(stderr, "  sudo insmod ../drivers/crypto_driver/crypto_driver.ko\n");
        exit(1);
    }
    
    printf("Crypto driver initialized successfully\n\n");
    
    // Run tests
    test_padding();
    test_des_encryption();
    test_sha1_hashing();
    test_key_setting();
    
    // Cleanup
    crypto_cleanup();
    
    printf("=== Test Suite Complete ===\n");
    
    return 0;
}
