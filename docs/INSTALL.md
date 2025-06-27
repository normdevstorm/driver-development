# Hướng dẫn Cài đặt và Sử dụng

## Yêu cầu Hệ thống

### Phần cứng
- Máy tính với CentOS 32-bit
- Ít nhất 512MB RAM
- Bàn phím USB (để test driver USB)

### Phần mềm
- CentOS 32-bit (khuyến nghị CentOS 6.x hoặc 7.x)
- Kernel headers và development tools
- GCC compiler
- Make utility

## Cài đặt Dependencies

### 1. Cài đặt kernel headers và build tools
```bash
# CentOS/RHEL
sudo yum update
sudo yum groupinstall "Development Tools"
sudo yum install kernel-devel kernel-headers

# Kiểm tra kernel headers
ls /lib/modules/$(uname -r)/build
```

### 2. Kiểm tra quyền truy cập
```bash
# Đảm bảo user có quyền truy cập USB và device files
sudo usermod -a -G disk,dialout $USER
```

## Hướng dẫn Build

### 1. Clone/download dự án
```bash
cd /path/to/project
# Nếu từ git:
# git clone <repository-url>
```

### 2. Build toàn bộ dự án
```bash
cd bai_tap_lon
chmod +x scripts/*.sh
./scripts/build.sh
```

### 3. Load drivers
```bash
sudo ./scripts/load_drivers.sh
```

### 4. Test hệ thống
```bash
./scripts/test_system.sh
```

## Cách sử dụng

### 1. Test Driver Crypto
```bash
cd userspace
./test_crypto
```

Expected output:
```
=== Crypto Library Test Suite ===

Crypto driver initialized successfully

Testing padding functions...
Testing DES encryption/decryption...
Original: Hello World! (length: 12)
Padded length: 16
Encryption successful
Encrypted data: [hex data]
Decryption successful
Decrypted: Hello World! (length: 12)
✓ DES encryption/decryption test PASSED
```

### 2. Chạy Chat System

#### Terminal 1 - Server:
```bash
cd userspace
./chat_server
```

Output:
```
=== Chat Server với mã hóa DES và SHA1 ===
Initializing user database...
User: admin, Password: admin123
User: user1, Password: password1
User: user2, Password: password2
Chat server started on port 8888
Waiting for connections...
```

#### Terminal 2 - Client:
```bash
cd userspace
./chat_client
```

### 3. Test USB Keyboard Driver

Kết nối bàn phím USB và kiểm tra dmesg:
```bash
dmesg | grep usb_keyboard
```

Expected output:
```
usb_keyboard: USB keyboard detected
usb_keyboard: USB Keyboard 046d:c31c initialized
```

## Tài khoản Test

| Username | Password   |
|----------|------------|
| admin    | admin123   |
| user1    | password1  |
| user2    | password2  |

## Troubleshooting

### 1. "Failed to initialize crypto driver"
```bash
# Kiểm tra driver đã load chưa
lsmod | grep crypto_driver

# Nếu chưa load:
sudo ./scripts/load_drivers.sh

# Kiểm tra device file
ls -l /dev/crypto_dev
```

### 2. "Socket creation failed" / "Bind failed"
```bash
# Kiểm tra port đã được sử dụng chưa
netstat -tlnp | grep 8888

# Kill process đang sử dụng port
sudo pkill chat_server
```

### 3. "Kernel headers not found"
```bash
# Cài đặt kernel headers cho kernel hiện tại
sudo yum install kernel-devel-$(uname -r)

# Hoặc cài đặt kernel headers mới nhất
sudo yum install kernel-devel
```

### 4. Build errors
```bash
# Clean và rebuild
cd drivers/crypto_driver
make clean && make

cd ../usb_keyboard  
make clean && make

cd ../../userspace
make clean && make
```

### 5. Permission denied khi load driver
```bash
# Đảm bảo chạy với quyền root
sudo ./scripts/load_drivers.sh

# Kiểm tra SELinux (nếu enabled)
sudo setenforce 0  # Temporary disable
```

### 6. USB keyboard not detected
```bash
# Kiểm tra USB devices
lsusb

# Kiểm tra kernel messages
dmesg | grep -i usb

# Reload USB driver
sudo rmmod usb_keyboard
sudo insmod drivers/usb_keyboard/usb_keyboard.ko
```

## Uninstall

### 1. Unload drivers
```bash
sudo ./scripts/unload_drivers.sh
```

### 2. Clean build files
```bash
cd drivers/crypto_driver && make clean
cd ../usb_keyboard && make clean
cd ../../userspace && make clean
```

### 3. Remove executables (optional)
```bash
cd userspace
rm -f chat_server chat_client test_crypto *.o
```

## Kiến trúc và Hoạt động

### Driver Crypto
- **File**: `drivers/crypto_driver/crypto_driver.c`
- **Device**: `/dev/crypto_dev`
- **Chức năng**: DES encryption/decryption, SHA1 hashing
- **Interface**: IOCTL commands

### Driver USB Keyboard
- **File**: `drivers/usb_keyboard/usb_keyboard.c`
- **Chức năng**: Detect và handle USB keyboard events
- **Protocol**: USB HID Boot Protocol

### Chat Application
- **Server**: `userspace/chat_server.c`
- **Client**: `userspace/chat_client.c`
- **Protocol**: TCP socket với DES encryption
- **Authentication**: SHA1 password hashing
