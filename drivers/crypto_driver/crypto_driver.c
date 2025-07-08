#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/crypto.h>
#include <crypto/hash.h>
#include <crypto/skcipher.h>
#include <linux/scatterlist.h>
#include <linux/random.h>
#include <linux/version.h>

// Modern kernel crypto API - no need for simple cipher
#undef USE_SIMPLE_CIPHER

#define DEVICE_NAME "crypto_dev"
#define CLASS_NAME "crypto_class"
#define DES_KEY_SIZE 8
#define DES_BLOCK_SIZE 8
#define SHA1_DIGEST_SIZE 20
#define RSA_KEY_SIZE 128  // 1024-bit RSA key
#define RSA_BLOCK_SIZE 128

// IOCTL commands
#define CRYPTO_IOC_MAGIC 'k'
#define CRYPTO_IOC_ENCRYPT _IOWR(CRYPTO_IOC_MAGIC, 1, struct crypto_data)
#define CRYPTO_IOC_DECRYPT _IOWR(CRYPTO_IOC_MAGIC, 2, struct crypto_data)
#define CRYPTO_IOC_HASH _IOWR(CRYPTO_IOC_MAGIC, 3, struct hash_data)
#define CRYPTO_IOC_SET_KEY _IOW(CRYPTO_IOC_MAGIC, 4, char[DES_KEY_SIZE])
#define CRYPTO_IOC_RSA_KEYGEN _IOWR(CRYPTO_IOC_MAGIC, 5, struct rsa_key)
#define CRYPTO_IOC_RSA_ENCRYPT _IOWR(CRYPTO_IOC_MAGIC, 6, struct rsa_data)
#define CRYPTO_IOC_RSA_DECRYPT _IOWR(CRYPTO_IOC_MAGIC, 7, struct rsa_data)

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

static int major_number;
static struct class *crypto_class = NULL;
static struct device *crypto_device = NULL;
static struct cdev crypto_cdev;
static dev_t dev_num;

// Crypto contexts - use modern skcipher API
static struct crypto_skcipher *des_tfm = NULL;
static struct crypto_shash *sha1_tfm = NULL;
static char des_key[DES_KEY_SIZE] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};

// Function prototypes
static int crypto_open(struct inode *, struct file *);
static int crypto_release(struct inode *, struct file *);
static ssize_t crypto_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t crypto_write(struct file *, const char __user *, size_t, loff_t *);
static long crypto_ioctl(struct file *, unsigned int, unsigned long);

// RSA function prototypes
static int rsa_generate_keypair(struct rsa_key *public_key, struct rsa_key *private_key);
static int rsa_encrypt(const char *input, char *output, size_t input_len, size_t *output_len, struct rsa_key *key);
static int rsa_decrypt(const char *input, char *output, size_t input_len, size_t *output_len, struct rsa_key *key);

static struct file_operations crypto_fops = {
    .owner = THIS_MODULE,
    .open = crypto_open,
    .release = crypto_release,
    .read = crypto_read,
    .write = crypto_write,
    .unlocked_ioctl = crypto_ioctl,
};

static int crypto_open(struct inode *inodep, struct file *filep)
{
    printk(KERN_INFO "crypto_driver: Device opened\n");
    return 0;
}

static int crypto_release(struct inode *inodep, struct file *filep)
{
    printk(KERN_INFO "crypto_driver: Device closed\n");
    return 0;
}

static ssize_t crypto_read(struct file *filep, char __user *buffer, size_t len, loff_t *offset)
{
    printk(KERN_INFO "crypto_driver: Read operation\n");
    return 0;
}

static ssize_t crypto_write(struct file *filep, const char __user *buffer, size_t len, loff_t *offset)
{
    printk(KERN_INFO "crypto_driver: Write operation\n");
    return len;
}

static int des_encrypt_decrypt(const char *input, char *output, size_t length, bool encrypt)
{
    struct skcipher_request *req;
    struct scatterlist sg_in, sg_out;
    char *aligned_input, *aligned_output;
    int ret;
    
    // Allocate aligned memory for DES (must be multiple of block size)
    size_t aligned_length = ((length + DES_BLOCK_SIZE - 1) / DES_BLOCK_SIZE) * DES_BLOCK_SIZE;
    
    aligned_input = kzalloc(aligned_length, GFP_KERNEL);
    aligned_output = kzalloc(aligned_length, GFP_KERNEL);
    if (!aligned_input || !aligned_output) {
        kfree(aligned_input);
        kfree(aligned_output);
        return -ENOMEM;
    }
    
    memcpy(aligned_input, input, length);
    
    req = skcipher_request_alloc(des_tfm, GFP_KERNEL);
    if (!req) {
        kfree(aligned_input);
        kfree(aligned_output);
        return -ENOMEM;
    }
    
    sg_init_one(&sg_in, aligned_input, aligned_length);
    sg_init_one(&sg_out, aligned_output, aligned_length);
    
    skcipher_request_set_crypt(req, &sg_in, &sg_out, aligned_length, NULL);
    
    if (encrypt)
        ret = crypto_skcipher_encrypt(req);
    else
        ret = crypto_skcipher_decrypt(req);
    
    if (ret == 0) {
        memcpy(output, aligned_output, length);
    }
    
    skcipher_request_free(req);
    kfree(aligned_input);
    kfree(aligned_output);
    
    return ret;
}

static int sha1_hash(const char *input, size_t input_length, char *output)
{
    struct shash_desc *desc;
    int ret;
    
    desc = kzalloc(sizeof(*desc) + crypto_shash_descsize(sha1_tfm), GFP_KERNEL);
    if (!desc)
        return -ENOMEM;
    
    desc->tfm = sha1_tfm;
    
    ret = crypto_shash_init(desc);
    if (ret)
        goto out;
    
    ret = crypto_shash_update(desc, input, input_length);
    if (ret)
        goto out;
    
    ret = crypto_shash_final(desc, output);
    
out:
    kfree(desc);
    return ret;
}

static long crypto_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
    struct crypto_data crypto_data;
    struct hash_data hash_data;
    char *kernel_input, *kernel_output;
    int ret = 0;
    
    switch (cmd) {
    case CRYPTO_IOC_ENCRYPT:
        if (copy_from_user(&crypto_data, (struct crypto_data __user *)arg, sizeof(crypto_data)))
            return -EFAULT;
        
        kernel_input = kzalloc(crypto_data.length, GFP_KERNEL);
        kernel_output = kzalloc(crypto_data.length, GFP_KERNEL);
        if (!kernel_input || !kernel_output) {
            kfree(kernel_input);
            kfree(kernel_output);
            return -ENOMEM;
        }
        
        if (copy_from_user(kernel_input, crypto_data.input, crypto_data.length)) {
            kfree(kernel_input);
            kfree(kernel_output);
            return -EFAULT;
        }
        
        ret = des_encrypt_decrypt(kernel_input, kernel_output, crypto_data.length, true);
        if (ret == 0) {
            if (copy_to_user(crypto_data.output, kernel_output, crypto_data.length))
                ret = -EFAULT;
        }
        
        kfree(kernel_input);
        kfree(kernel_output);
        break;
        
    case CRYPTO_IOC_DECRYPT:
        if (copy_from_user(&crypto_data, (struct crypto_data __user *)arg, sizeof(crypto_data)))
            return -EFAULT;
        
        kernel_input = kzalloc(crypto_data.length, GFP_KERNEL);
        kernel_output = kzalloc(crypto_data.length, GFP_KERNEL);
        if (!kernel_input || !kernel_output) {
            kfree(kernel_input);
            kfree(kernel_output);
            return -ENOMEM;
        }
        
        if (copy_from_user(kernel_input, crypto_data.input, crypto_data.length)) {
            kfree(kernel_input);
            kfree(kernel_output);
            return -EFAULT;
        }
        
        ret = des_encrypt_decrypt(kernel_input, kernel_output, crypto_data.length, false);
        if (ret == 0) {
            if (copy_to_user(crypto_data.output, kernel_output, crypto_data.length))
                ret = -EFAULT;
        }
        
        kfree(kernel_input);
        kfree(kernel_output);
        break;
        
    case CRYPTO_IOC_HASH:
        if (copy_from_user(&hash_data, (struct hash_data __user *)arg, sizeof(hash_data)))
            return -EFAULT;
        
        kernel_input = kzalloc(hash_data.input_length, GFP_KERNEL);
        kernel_output = kzalloc(SHA1_DIGEST_SIZE, GFP_KERNEL);
        if (!kernel_input || !kernel_output) {
            kfree(kernel_input);
            kfree(kernel_output);
            return -ENOMEM;
        }
        
        if (copy_from_user(kernel_input, hash_data.input, hash_data.input_length)) {
            kfree(kernel_input);
            kfree(kernel_output);
            return -EFAULT;
        }
        
        ret = sha1_hash(kernel_input, hash_data.input_length, kernel_output);
        if (ret == 0) {
            if (copy_to_user(hash_data.output, kernel_output, SHA1_DIGEST_SIZE))
                ret = -EFAULT;
        }
        
        kfree(kernel_input);
        kfree(kernel_output);
        break;
        
    case CRYPTO_IOC_SET_KEY:
        if (copy_from_user(des_key, (char __user *)arg, DES_KEY_SIZE))
            return -EFAULT;
        
        ret = crypto_skcipher_setkey(des_tfm, des_key, DES_KEY_SIZE);
        break;
        
    case CRYPTO_IOC_RSA_KEYGEN:
        {
            struct rsa_key public_key, private_key;
            
            ret = rsa_generate_keypair(&public_key, &private_key);
            if (ret == 0) {
                // For simplicity, return the public key
                if (copy_to_user((struct rsa_key __user *)arg, &public_key, sizeof(struct rsa_key)))
                    ret = -EFAULT;
            }
        }
        break;
        
    case CRYPTO_IOC_RSA_ENCRYPT:
        {
            struct rsa_data rsa_data;
            struct rsa_key kernel_key;
            char *kernel_input, *kernel_output;
            size_t output_len;
            
            if (copy_from_user(&rsa_data, (struct rsa_data __user *)arg, sizeof(rsa_data)))
                return -EFAULT;
                
            if (copy_from_user(&kernel_key, rsa_data.key, sizeof(struct rsa_key)))
                return -EFAULT;
            
            kernel_input = kzalloc(rsa_data.input_length, GFP_KERNEL);
            kernel_output = kzalloc(RSA_BLOCK_SIZE, GFP_KERNEL);
            if (!kernel_input || !kernel_output) {
                kfree(kernel_input);
                kfree(kernel_output);
                return -ENOMEM;
            }
            
            if (copy_from_user(kernel_input, rsa_data.input, rsa_data.input_length)) {
                kfree(kernel_input);
                kfree(kernel_output);
                return -EFAULT;
            }
            
            ret = rsa_encrypt(kernel_input, kernel_output, rsa_data.input_length, &output_len, &kernel_key);
            if (ret == 0) {
                if (copy_to_user(rsa_data.output, kernel_output, output_len))
                    ret = -EFAULT;
                else {
                    rsa_data.output_length = output_len;
                    if (copy_to_user((struct rsa_data __user *)arg, &rsa_data, sizeof(rsa_data)))
                        ret = -EFAULT;
                }
            }
            
            kfree(kernel_input);
            kfree(kernel_output);
        }
        break;
        
    case CRYPTO_IOC_RSA_DECRYPT:
        {
            struct rsa_data rsa_data;
            struct rsa_key kernel_key;
            char *kernel_input, *kernel_output;
            size_t output_len;
            
            if (copy_from_user(&rsa_data, (struct rsa_data __user *)arg, sizeof(rsa_data)))
                return -EFAULT;
                
            if (copy_from_user(&kernel_key, rsa_data.key, sizeof(struct rsa_key)))
                return -EFAULT;
            
            kernel_input = kzalloc(rsa_data.input_length, GFP_KERNEL);
            kernel_output = kzalloc(RSA_BLOCK_SIZE, GFP_KERNEL);
            if (!kernel_input || !kernel_output) {
                kfree(kernel_input);
                kfree(kernel_output);
                return -ENOMEM;
            }
            
            if (copy_from_user(kernel_input, rsa_data.input, rsa_data.input_length)) {
                kfree(kernel_input);
                kfree(kernel_output);
                return -EFAULT;
            }
            
            ret = rsa_decrypt(kernel_input, kernel_output, rsa_data.input_length, &output_len, &kernel_key);
            if (ret == 0) {
                if (copy_to_user(rsa_data.output, kernel_output, output_len))
                    ret = -EFAULT;
                else {
                    rsa_data.output_length = output_len;
                    if (copy_to_user((struct rsa_data __user *)arg, &rsa_data, sizeof(rsa_data)))
                        ret = -EFAULT;
                }
            }
            
            kfree(kernel_input);
            kfree(kernel_output);
        }
        break;
        
    default:
        return -EINVAL;
    }
    
    return ret;
}

static int __init crypto_driver_init(void)
{
    int ret;
    
    printk(KERN_INFO "crypto_driver: Initializing crypto driver\n");
    
    // Allocate device number
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ALERT "crypto_driver: Failed to allocate device number\n");
        return ret;
    }
    major_number = MAJOR(dev_num);
    
    // Initialize cdev
    cdev_init(&crypto_cdev, &crypto_fops);
    crypto_cdev.owner = THIS_MODULE;
    
    ret = cdev_add(&crypto_cdev, dev_num, 1);
    if (ret < 0) {
        unregister_chrdev_region(dev_num, 1);
        return ret;
    }
    
    // Create device class - updated for newer kernel API
    crypto_class = class_create(CLASS_NAME);
    if (IS_ERR(crypto_class)) {
        cdev_del(&crypto_cdev);
        unregister_chrdev_region(dev_num, 1);
        return PTR_ERR(crypto_class);
    }
    
    // Create device
    crypto_device = device_create(crypto_class, NULL, dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(crypto_device)) {
        class_destroy(crypto_class);
        cdev_del(&crypto_cdev);
        unregister_chrdev_region(dev_num, 1);
        return PTR_ERR(crypto_device);
    }
    
    // Initialize crypto transforms
    printk(KERN_INFO "crypto_driver: Allocating DES transform...\n");
    des_tfm = crypto_alloc_skcipher("ecb(des)", 0, 0);
    if (IS_ERR(des_tfm)) {
        printk(KERN_ALERT "crypto_driver: Failed to allocate DES transform (error: %ld)\n", PTR_ERR(des_tfm));
        printk(KERN_ALERT "crypto_driver: Make sure des_generic module is loaded\n");
        device_destroy(crypto_class, dev_num);
        class_destroy(crypto_class);
        cdev_del(&crypto_cdev);
        unregister_chrdev_region(dev_num, 1);
        return PTR_ERR(des_tfm);
    }
    printk(KERN_INFO "crypto_driver: DES transform allocated successfully\n");
    
    printk(KERN_INFO "crypto_driver: Allocating SHA1 transform...\n");
    sha1_tfm = crypto_alloc_shash("sha1", 0, 0);
    if (IS_ERR(sha1_tfm)) {
        printk(KERN_ALERT "crypto_driver: Failed to allocate SHA1 transform (error: %ld)\n", PTR_ERR(sha1_tfm));
        crypto_free_skcipher(des_tfm);
        device_destroy(crypto_class, dev_num);
        class_destroy(crypto_class);
        cdev_del(&crypto_cdev);
        unregister_chrdev_region(dev_num, 1);
        return PTR_ERR(sha1_tfm);
    }
    printk(KERN_INFO "crypto_driver: SHA1 transform allocated successfully\n");
    
    // Set default DES key
    ret = crypto_skcipher_setkey(des_tfm, des_key, DES_KEY_SIZE);
    if (ret) {
        printk(KERN_ALERT "crypto_driver: Failed to set DES key\n");
        crypto_free_shash(sha1_tfm);
        crypto_free_skcipher(des_tfm);
        device_destroy(crypto_class, dev_num);
        class_destroy(crypto_class);
        cdev_del(&crypto_cdev);
        unregister_chrdev_region(dev_num, 1);
        return ret;
    }
    
    printk(KERN_INFO "crypto_driver: Driver initialized successfully\n");
    return 0;
}

static void __exit crypto_driver_exit(void)
{
    crypto_free_shash(sha1_tfm);
    crypto_free_skcipher(des_tfm);
    device_destroy(crypto_class, dev_num);
    class_destroy(crypto_class);
    cdev_del(&crypto_cdev);
    unregister_chrdev_region(dev_num, 1);
    printk(KERN_INFO "crypto_driver: Driver exited\n");
}

module_init(crypto_driver_init);
module_exit(crypto_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Student");
MODULE_DESCRIPTION("DES Encryption and SHA1 Hashing Driver for CentOS 9 64-bit");
MODULE_VERSION("2.0");
MODULE_DESCRIPTION("DES Encryption and SHA1 Hashing Driver for CentOS 9 64-bit");
MODULE_VERSION("2.0");

// Simple RSA operations (for demonstration - not cryptographically secure)
static int rsa_generate_keypair(struct rsa_key *public_key, struct rsa_key *private_key)
{
    int i;
    
    // Initialize keys
    memset(public_key, 0, sizeof(struct rsa_key));
    memset(private_key, 0, sizeof(struct rsa_key));
    
    public_key->key_size = RSA_KEY_SIZE;
    private_key->key_size = RSA_KEY_SIZE;
    
    // Generate random values for demonstration
    get_random_bytes(public_key->n, RSA_KEY_SIZE);
    get_random_bytes(public_key->e, RSA_KEY_SIZE);
    get_random_bytes(private_key->d, RSA_KEY_SIZE);
    
    // Copy modulus
    memcpy(private_key->n, public_key->n, RSA_KEY_SIZE);
    
    // Set simple public exponent
    for (i = 0; i < RSA_KEY_SIZE; i++) {
        public_key->e[i] = (i < 4) ? 0x01 : 0x00;
        private_key->d[i] = (i < 4) ? 0x01 : 0x00;
    }
    
    return 0;
}

static int rsa_encrypt(const char *input, char *output, size_t input_len, size_t *output_len, struct rsa_key *key)
{
    size_t i;
    
    if (input_len > RSA_BLOCK_SIZE - 11) {
        return -EINVAL;
    }
    
    // Simple XOR encryption for demonstration
    *output_len = RSA_BLOCK_SIZE;
    memcpy(output, input, input_len);
    
    // Pad to RSA block size
    for (i = input_len; i < RSA_BLOCK_SIZE; i++) {
        output[i] = (char)(RSA_BLOCK_SIZE - input_len);
    }
    
    // Simple XOR with key
    for (i = 0; i < RSA_BLOCK_SIZE; i++) {
        output[i] ^= key->e[i % RSA_KEY_SIZE];
    }
    
    return 0;
}

static int rsa_decrypt(const char *input, char *output, size_t input_len, size_t *output_len, struct rsa_key *key)
{
    size_t i, padding;
    
    if (input_len != RSA_BLOCK_SIZE) {
        return -EINVAL;
    }
    
    // Simple XOR decryption for demonstration
    memcpy(output, input, input_len);
    
    // Simple XOR with key
    for (i = 0; i < RSA_BLOCK_SIZE; i++) {
        output[i] ^= key->d[i % RSA_KEY_SIZE];
    }
    
    // Remove padding
    padding = (unsigned char)output[RSA_BLOCK_SIZE - 1];
    if (padding > 0 && padding < RSA_BLOCK_SIZE) {
        *output_len = RSA_BLOCK_SIZE - padding;
        output[*output_len] = '\0';
    } else {
        *output_len = RSA_BLOCK_SIZE;
    }
    
    return 0;
}
