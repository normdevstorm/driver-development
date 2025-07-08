# GTK UI Setup Instructions

## Prerequisites

### Install GTK Development Libraries

For CentOS/RHEL/Rocky Linux:
```bash
sudo yum groupinstall "Development Tools"
sudo yum install gtk2-devel pkg-config
```

For Debian/Ubuntu:
```bash
sudo apt-get update
sudo apt-get install build-essential libgtk2.0-dev pkg-config
```

### Verify Installation
```bash
pkg-config --exists gtk+-2.0 && echo "GTK+ 2.0 found" || echo "GTK+ 2.0 not found"
```

## Building the Applications

### Build Everything (CLI + GTK)
```bash
cd scripts
./build.sh
```

### Build Only GTK Applications
```bash
cd userspace
make gtk
```

### Build Only CLI Applications
```bash
cd userspace
make cli
```

## Running the Applications

### GTK Chat Server
```bash
cd userspace
./chat_server_gtk
```

Features:
- Start/Stop server controls
- Real-time server log
- Connected clients list
- Port configuration (default: 8080)
- Encrypted message handling

### GTK Chat Client
```bash
cd userspace
./chat_client_gtk
```

Features:
- Server IP configuration
- Username/Password authentication
- Real-time chat interface
- Message encryption/decryption
- Connection status indicator

## User Accounts

Default users for testing:
- Username: `admin`, Password: `admin123`
- Username: `user1`, Password: `password1`
- Username: `user2`, Password: `password2`
- Username: `test`, Password: `test123`

## Features

### Security Features
- DES encryption for messages using kernel crypto driver
- SHA1 password hashing
- Secure authentication protocol

### UI Features
- Modern GTK+ interface
- Real-time message display
- Auto-scrolling chat view
- Client connection monitoring
- Status indicators

### Network Features
- TCP socket communication
- Multi-client support (up to 10 clients)
- Message broadcasting
- Connection management

## Troubleshooting

### GTK Not Found
```bash
# Check if GTK is installed
pkg-config --list-all | grep gtk

# Install if missing
sudo yum install gtk2-devel pkg-config
```

### Build Errors
```bash
# Clean and rebuild
cd userspace
make clean-all
make all
```

### Runtime Issues
```bash
# Check if crypto driver is loaded
lsmod | grep crypto_driver

# Load drivers if needed
cd scripts
sudo ./load_drivers.sh
```

## File Structure
```
userspace/
├── gtk_ui/
│   ├── chat_client_gtk.c    # GTK chat client source
│   ├── chat_server_gtk.c    # GTK chat server source
│   └── Makefile             # GTK build configuration
├── chat_client_gtk          # GTK client executable
├── chat_server_gtk          # GTK server executable
├── chat_client.c            # CLI client source
├── chat_server.c            # CLI server source
├── crypto_lib.c             # Encryption library
└── Makefile                 # Main build configuration
```
