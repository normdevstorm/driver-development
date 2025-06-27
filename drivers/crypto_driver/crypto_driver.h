#ifndef CRYPTO_DRIVER_H
#define CRYPTO_DRIVER_H

#include <linux/ioctl.h>

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

// IOCTL commands
#define CRYPTO_IOC_MAGIC 'k'
#define CRYPTO_IOC_ENCRYPT _IOWR(CRYPTO_IOC_MAGIC, 1, struct crypto_data)
#define CRYPTO_IOC_DECRYPT _IOWR(CRYPTO_IOC_MAGIC, 2, struct crypto_data)
#define CRYPTO_IOC_HASH _IOWR(CRYPTO_IOC_MAGIC, 3, struct hash_data)
#define CRYPTO_IOC_SET_KEY _IOW(CRYPTO_IOC_MAGIC, 4, char[DES_KEY_SIZE])

#endif /* CRYPTO_DRIVER_H */
