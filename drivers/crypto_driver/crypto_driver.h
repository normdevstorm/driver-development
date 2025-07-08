#ifndef CRYPTO_DRIVER_H
#define CRYPTO_DRIVER_H

#include <linux/ioctl.h>

#define DES_KEY_SIZE 8
#define DES_BLOCK_SIZE 8
#define SHA1_DIGEST_SIZE 20
#define RSA_KEY_SIZE 128  // 1024-bit RSA key
#define RSA_BLOCK_SIZE 128

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

// RSA key structures
struct rsa_key {
    unsigned char n[RSA_KEY_SIZE];    // modulus
    unsigned char e[RSA_KEY_SIZE];    // public exponent
    unsigned char d[RSA_KEY_SIZE];    // private exponent
    unsigned char p[RSA_KEY_SIZE/2];  // prime p
    unsigned char q[RSA_KEY_SIZE/2];  // prime q
    size_t key_size;
};

struct rsa_data {
    char *input;
    char *output;
    size_t input_length;
    size_t output_length;
    struct rsa_key *key;
};

// IOCTL commands
#define CRYPTO_IOC_MAGIC 'k'
#define CRYPTO_IOC_ENCRYPT _IOWR(CRYPTO_IOC_MAGIC, 1, struct crypto_data)
#define CRYPTO_IOC_DECRYPT _IOWR(CRYPTO_IOC_MAGIC, 2, struct crypto_data)
#define CRYPTO_IOC_HASH _IOWR(CRYPTO_IOC_MAGIC, 3, struct hash_data)
#define CRYPTO_IOC_SET_KEY _IOW(CRYPTO_IOC_MAGIC, 4, char[DES_KEY_SIZE])
#define CRYPTO_IOC_RSA_KEYGEN _IOWR(CRYPTO_IOC_MAGIC, 5, struct rsa_key)
#define CRYPTO_IOC_RSA_ENCRYPT _IOWR(CRYPTO_IOC_MAGIC, 6, struct rsa_data)
#define CRYPTO_IOC_RSA_DECRYPT _IOWR(CRYPTO_IOC_MAGIC, 7, struct rsa_data)

#endif /* CRYPTO_DRIVER_H */
