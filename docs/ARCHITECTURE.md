# Kiến trúc Hệ thống và Thiết kế

## Tổng quan Kiến trúc

```
┌─────────────────────────────────────────────────────┐
│                 User Space                          │
├─────────────────┬─────────────────┬─────────────────┤
│   Chat Client   │   Chat Server   │   Test Programs │
│                 │                 │                 │
├─────────────────┴─────────────────┴─────────────────┤
│              Crypto Library (crypto_lib.c)         │
├─────────────────────────────────────────────────────┤
│                System Calls (IOCTL)                │
├═════════════════════════════════════════════════════┤
│                 Kernel Space                        │
├─────────────────┬─────────────────────────────────────┤
│  Crypto Driver  │        USB Keyboard Driver        │
│   (DES/SHA1)    │                                   │
├─────────────────┼─────────────────────────────────────┤
│  Crypto API     │           USB Subsystem           │
├─────────────────┴─────────────────────────────────────┤
│                  Linux Kernel                       │
└─────────────────────────────────────────────────────┘
```

## Chi tiết Thiết kế

### 1. Crypto Driver (`crypto_driver.c`)

#### Mục đích
- Cung cấp DES encryption/decryption trong kernel space
- Cung cấp SHA1 hashing trong kernel space
- Tạo interface an toàn cho userspace applications

#### Kiến trúc
```c
// Main structures
struct crypto_data {
    char *input;      // Input data pointer
    char *output;     // Output data pointer  
    size_t length;    // Data length
};

struct hash_data {
    char *input;         // Input data pointer
    char *output;        // Output hash pointer
    size_t input_length; // Input data length
};
```

#### IOCTL Commands
- `CRYPTO_IOC_ENCRYPT`: Mã hóa dữ liệu với DES
- `CRYPTO_IOC_DECRYPT`: Giải mã dữ liệu với DES
- `CRYPTO_IOC_HASH`: Tạo SHA1 hash
- `CRYPTO_IOC_SET_KEY`: Thiết lập DES key

#### Key Features
- **Kernel Crypto API**: Sử dụng `crypto_skcipher` cho DES và `crypto_shash` cho SHA1
- **Memory Management**: Safe kernel memory allocation với `kmalloc/kzalloc`
- **Data Alignment**: Đảm bảo dữ liệu align theo DES block size (8 bytes)
- **Error Handling**: Comprehensive error checking và cleanup

### 2. USB Keyboard Driver (`usb_keyboard.c`)

#### Mục đích
- Demo driver USB device trên CentOS 32-bit
- Handle USB keyboard events
- Integrate với Linux Input Subsystem

#### Kiến trúc
```c
struct usb_keyboard {
    struct input_dev *input_dev;    // Input device
    struct usb_device *udev;        // USB device
    struct usb_interface *interface; // USB interface
    struct urb *irq_urb;           // Interrupt URB
    unsigned char *data;            // Data buffer
    dma_addr_t data_dma;           // DMA address
    char name[128];                 // Device name
    char phys[64];                  // Physical location
};
```

#### Key Features
- **USB HID Boot Protocol**: Support standard USB keyboard
- **Interrupt URBs**: Handle keyboard events via USB interrupts
- **Input Subsystem**: Integrate với Linux input layer
- **Key Mapping**: Map USB scan codes to Linux key codes
- **Modifier Keys**: Handle Ctrl, Alt, Shift, Meta keys

### 3. Crypto Library (`crypto_lib.c`)

#### Mục đích
- Userspace interface cho crypto driver
- Helper functions cho encryption/decryption
- PKCS#5 padding implementation

#### Key Functions
```c
int crypto_init(void);                    // Initialize driver connection
int crypto_set_key(const char *key);      // Set DES key
int crypto_encrypt(const char *input, char *output, size_t length);
int crypto_decrypt(const char *input, char *output, size_t length);
int crypto_hash(const char *input, size_t input_length, char *output);

// Utility functions
size_t crypto_pad_data(const char *input, char *output, size_t length);
size_t crypto_unpad_data(const char *input, char *output, size_t length);
```

### 4. Chat System

#### Chat Server (`chat_server.c`)

**Features:**
- Multi-threaded server (pthread)
- User authentication với SHA1
- Message encryption với DES
- Broadcast messaging
- Connection management

**Architecture:**
```c
struct client_info {
    int socket;                    // Client socket
    struct sockaddr_in address;    // Client address
    char username[32];             // Username
    int authenticated;             // Auth status
    pthread_t thread;              // Client thread
};
```

**Authentication Flow:**
1. Client connects
2. Server requests username/password
3. Server hashes password với SHA1
4. Compare with stored hash
5. Send encrypted welcome message

**Message Flow:**
1. Receive encrypted message from client
2. Decrypt using DES
3. Process message
4. Encrypt and broadcast to all clients

#### Chat Client (`chat_client.c`)

**Features:**
- Connect to server
- User authentication
- Send/receive encrypted messages
- Multi-threaded (send/receive threads)
- Password masking

## Protocol Specifications

### 1. Crypto Driver Protocol

#### Device Interface
- **Device File**: `/dev/crypto_dev`
- **Access Mode**: Read/Write (666 permissions)
- **Interface**: IOCTL-based

#### IOCTL Format
```c
#define CRYPTO_IOC_MAGIC 'k'
#define CRYPTO_IOC_ENCRYPT _IOWR(CRYPTO_IOC_MAGIC, 1, struct crypto_data)
#define CRYPTO_IOC_DECRYPT _IOWR(CRYPTO_IOC_MAGIC, 2, struct crypto_data)
#define CRYPTO_IOC_HASH _IOWR(CRYPTO_IOC_MAGIC, 3, struct hash_data)
#define CRYPTO_IOC_SET_KEY _IOW(CRYPTO_IOC_MAGIC, 4, char[8])
```

### 2. Chat Protocol

#### Connection Phase
```
Client -> Server: TCP connection to port 8888
Server -> Client: "USERNAME:"
Client -> Server: username\n
Server -> Client: "PASSWORD:"
Client -> Server: password\n
Server -> Client: Encrypted("AUTHENTICATED\nWelcome...") or "AUTHENTICATION_FAILED"
```

#### Message Phase
```
Client -> Server: Encrypted(message)
Server -> All: Encrypted("username: message")
```

#### Encryption Details
- **Algorithm**: DES in kernel space
- **Key**: 8-byte static key (configurable)
- **Padding**: PKCS#5 padding to 8-byte blocks
- **Mode**: ECB (for simplicity)

## Security Considerations

### 1. Kernel Space Security
- **Memory Safety**: Proper bounds checking
- **Privilege Separation**: IOCTL permission checking
- **Resource Management**: Cleanup on errors
- **Input Validation**: Validate all user inputs

### 2. Crypto Security
- **Key Management**: Keys stored in kernel memory
- **Padding**: PKCS#5 standard padding
- **Hash Security**: SHA1 for password hashing
- **Side Channel**: Basic protection against timing attacks

### 3. Network Security
- **Authentication**: Username/password with hashing
- **Encryption**: All messages encrypted
- **Session Management**: Per-client authentication state

## Performance Considerations

### 1. Kernel Performance
- **Memory Allocation**: Minimize kmalloc calls
- **Copy Operations**: Efficient user/kernel space copying
- **Crypto Operations**: Use kernel crypto API for performance

### 2. Network Performance
- **Threading**: Multi-threaded server for concurrent clients
- **Buffering**: Appropriate buffer sizes (1KB)
- **Connection Handling**: Efficient socket management

## Debugging và Monitoring

### 1. Kernel Debugging
```bash
# Driver messages
dmesg | grep crypto_driver
dmesg | grep usb_keyboard

# Module status
lsmod | grep -E "(crypto_driver|usb_keyboard)"

# Device files
ls -l /dev/crypto_dev
```

### 2. Application Debugging
```bash
# Network debugging
netstat -tlnp | grep 8888
ss -tlnp | grep 8888

# Process debugging
ps aux | grep chat
strace ./chat_client
```

### 3. USB Debugging
```bash
# USB devices
lsusb
cat /proc/bus/usb/devices

# Input devices
cat /proc/bus/input/devices
```

## Testing Strategy

### 1. Unit Testing
- **Crypto Functions**: Test encrypt/decrypt/hash individually
- **Padding Functions**: Test various input sizes
- **Error Handling**: Test error conditions

### 2. Integration Testing
- **Driver Loading**: Test module load/unload
- **Device Creation**: Test device file creation
- **IOCTL Interface**: Test all IOCTL commands

### 3. System Testing
- **Chat System**: End-to-end chat functionality
- **Multi-client**: Multiple simultaneous clients
- **Error Recovery**: Network failures, driver failures

### 4. Performance Testing
- **Throughput**: Message throughput testing
- **Latency**: Message delivery latency
- **Resource Usage**: Memory and CPU usage
