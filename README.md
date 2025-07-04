# Bài Tập Lớn - Lập Trình Driver Linux

Dự án phát triển driver Linux và ứng dụng chat với mã hóa cho CentOS 9 64-bit.

## Mô tả dự án

1. **Chương trình chat dựa trên socket**: Cho phép xác thực người dùng và mã hóa tin nhắn
2. **Driver mã hóa**: Sử dụng thuật toán DES và hashing SHA1 trong kernel space
3. **Driver bàn phím USB**: Minh họa driver thiết bị USB trên CentOS 9 64-bit

## Cấu trúc dự án

```
├── drivers/                    # Kernel drivers
│   ├── crypto_driver/         # Driver mã hóa DES/SHA1
│   └── usb_keyboard/          # Driver bàn phím USB
├── userspace/                 # Ứng dụng user space
│   ├── chat_client/           # Client chat
│   └── chat_server/           # Server chat
├── scripts/                   # Scripts build và test
└── docs/                      # Tài liệu
```

## Yêu cầu hệ thống

- CentOS 9 64-bit (Red Hat Enterprise Linux 9 compatible)
- Kernel headers (kernel-devel package)
- GCC compiler (version 11+)
- Make utility
- Modern kernel (5.14+)

## Hướng dẫn build

### 1. Build toàn bộ dự án
```bash
./scripts/build.sh
```

### 2. Load drivers
```bash
sudo ./scripts/load_drivers.sh
```

### 3. Test hệ thống
```bash
./scripts/test_system.sh
```

### 4. Test crypto driver
```bash
cd userspace
./test_crypto
```

### 5. Chạy ứng dụng chat
```bash
# Terminal 1 - Server
cd userspace
./chat_server

# Terminal 2 - Client (trong terminal khác)
cd userspace
./chat_client
```

### 6. Dọn dẹp (unload drivers)
```bash
sudo ./scripts/unload_drivers.sh
```

## Tính năng

### Driver mã hóa
- ✅ Mã hóa DES trong kernel space
- ✅ Hash SHA1 cho xác thực
- ✅ Device file interface (/dev/crypto_dev)
- ✅ IOCTL commands cho mã hóa/giải mã

### Ứng dụng chat
- ✅ Socket TCP cho giao tiếp
- ✅ Xác thực người dùng với SHA1
- ✅ Mã hóa tin nhắn với DES
- ✅ Giao diện console đơn giản

### Driver bàn phím USB
- ✅ Detect USB keyboard
- ✅ Handle key press events
- ✅ Output key codes

## Tài liệu tham khảo

- [Linux Device Drivers](https://lwn.net/Kernel/LDD3/)
- [Linux Kernel Module Programming Guide](https://tldp.org/LDP/lkmpg/2.6/html/)
- [USB Driver Development](https://www.kernel.org/doc/html/latest/driver-api/usb/index.html)
- [Modern Crypto API](https://www.kernel.org/doc/html/latest/crypto/index.html)
- [CentOS 9 Documentation](https://docs.centos.org/en-US/centos/CentOS-9-docs/)

## Tác giả

Bài tập lớn môn Lập trình Driver - Migrated to CentOS 9 64-bit

## Migration Notes

This project has been migrated from CentOS 32-bit to CentOS 9 64-bit. Key changes include:
- Modern kernel crypto API usage (skcipher instead of deprecated cipher API)
- 64-bit architecture support
- Updated build system for DNF package manager
- Enhanced compatibility with kernel 5.14+

See [MIGRATION.md](MIGRATION.md) for detailed migration information.

## Tài khoản test

| Username | Password   | Mô tả        |
|----------|------------|--------------|
| admin    | admin123   | Admin user   |
| user1    | password1  | Test user 1  |
| user2    | password2  | Test user 2  |

## Cấu trúc file quan trọng

```
drivers/crypto_driver/crypto_driver.c    # Driver DES/SHA1 (CentOS 9)
drivers/usb_keyboard/usb_keyboard.c       # Driver bàn phím USB (CentOS 9)
userspace/crypto_lib.c                    # Library giao tiếp với driver
userspace/chat_server.c                   # Chat server
userspace/chat_client.c                   # Chat client
userspace/test_crypto.c                   # Test crypto functionality
scripts/build.sh                          # Build toàn bộ dự án (CentOS 9)
scripts/load_drivers.sh                   # Load drivers
scripts/test_system.sh                    # Test hệ thống
MIGRATION.md                              # Migration documentation
```
