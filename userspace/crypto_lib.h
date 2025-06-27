#ifndef CRYPTO_LIB_H
#define CRYPTO_LIB_H

#include <stddef.h>

#define DES_KEY_SIZE 8
#define DES_BLOCK_SIZE 8
#define SHA1_DIGEST_SIZE 20

struct crypto_data {
    char *input;
    char *output;
    size_t length;
};

struct hash_data {
    char *input;
    char *output;
    size_t input_length;
};

// IOCTL commands (must match driver)
#define CRYPTO_IOC_MAGIC 'k'
#define CRYPTO_IOC_ENCRYPT _IOWR(CRYPTO_IOC_MAGIC, 1, struct crypto_data)
#define CRYPTO_IOC_DECRYPT _IOWR(CRYPTO_IOC_MAGIC, 2, struct crypto_data)
#define CRYPTO_IOC_HASH _IOWR(CRYPTO_IOC_MAGIC, 3, struct hash_data)
#define CRYPTO_IOC_SET_KEY _IOW(CRYPTO_IOC_MAGIC, 4, char[DES_KEY_SIZE])

// Function prototypes
int crypto_init(void);
void crypto_cleanup(void);
int crypto_set_key(const char *key);
int crypto_encrypt(const char *input, char *output, size_t length);
int crypto_decrypt(const char *input, char *output, size_t length);
int crypto_hash(const char *input, size_t input_length, char *output);

// Utility functions
size_t crypto_pad_data(const char *input, char *output, size_t length);
size_t crypto_unpad_data(const char *input, char *output, size_t length);
void crypto_print_hex(const char *data, size_t length);

#endif /* CRYPTO_LIB_H */
