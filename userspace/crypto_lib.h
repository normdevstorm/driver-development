#ifndef CRYPTO_LIB_H
#define CRYPTO_LIB_H

#include <stddef.h>

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

// IOCTL commands (must match driver)
#define CRYPTO_IOC_MAGIC 'k'
#define CRYPTO_IOC_ENCRYPT _IOWR(CRYPTO_IOC_MAGIC, 1, struct crypto_data)
#define CRYPTO_IOC_DECRYPT _IOWR(CRYPTO_IOC_MAGIC, 2, struct crypto_data)
#define CRYPTO_IOC_HASH _IOWR(CRYPTO_IOC_MAGIC, 3, struct hash_data)
#define CRYPTO_IOC_SET_KEY _IOW(CRYPTO_IOC_MAGIC, 4, char[DES_KEY_SIZE])
#define CRYPTO_IOC_RSA_KEYGEN _IOWR(CRYPTO_IOC_MAGIC, 5, struct rsa_key)
#define CRYPTO_IOC_RSA_ENCRYPT _IOWR(CRYPTO_IOC_MAGIC, 6, struct rsa_data)
#define CRYPTO_IOC_RSA_DECRYPT _IOWR(CRYPTO_IOC_MAGIC, 7, struct rsa_data)

// Function prototypes
int crypto_init(void);
void crypto_cleanup(void);
int crypto_set_key(const char *key);
int crypto_encrypt(const char *input, char *output, size_t length);
int crypto_decrypt(const char *input, char *output, size_t length);
int crypto_hash(const char *input, size_t input_length, char *output);

// RSA functions
int crypto_rsa_generate_keypair(struct rsa_key *public_key, struct rsa_key *private_key);
int crypto_rsa_encrypt(const char *input, size_t input_len, char *output, size_t *output_len, struct rsa_key *public_key);
int crypto_rsa_decrypt(const char *input, size_t input_len, char *output, size_t *output_len, struct rsa_key *private_key);

// Key exchange functions
int perform_key_exchange_server(int client_socket, char *shared_des_key);
int perform_key_exchange_client(int server_socket, char *shared_des_key);

// Utility functions
size_t crypto_pad_data(const char *input, char *output, size_t length);
size_t crypto_unpad_data(const char *input, char *output, size_t length);
void crypto_print_hex(const char *data, size_t length);
void crypto_bin_to_hex(const char *bin, size_t bin_len, char *hex);
size_t crypto_hex_to_bin(const char *hex, char *bin);

#endif /* CRYPTO_LIB_H */
