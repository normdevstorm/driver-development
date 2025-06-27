#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

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
