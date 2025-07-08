#!/bin/bash

# Build script cho CentOS 9 64-bit
# Bài tập lớn - Lập trình Driver Linux

set -e  # Exit on any error

echo "=== Build Script for Linux Driver Project ==="
echo "Building for CentOS 9 64-bit..."
echo

# Get the project root directory (parent of scripts)
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
echo "Project root: $PROJECT_ROOT"

# Change to project root directory
cd "$PROJECT_ROOT"

# Verify we're in the correct directory
if [ ! -d "drivers" ] || [ ! -d "userspace" ] || [ ! -d "scripts" ]; then
    echo "Error: Not in project root directory. Expected directories: drivers, userspace, scripts"
    echo "Current directory: $(pwd)"
    exit 1
fi

# Check if running as root for kernel module operations
check_root() {
    if [ "$EUID" -eq 0 ]; then
        echo "Warning: Running as root. Some operations may not require root access."
    fi
}

# Check kernel headers
check_kernel_headers() {
    echo "Checking kernel headers..."
    KERNEL_VERSION=$(uname -r)
    KERNEL_HEADERS="/lib/modules/$KERNEL_VERSION/build"
    
    if [ ! -d "$KERNEL_HEADERS" ]; then
        echo "Error: Kernel headers not found at $KERNEL_HEADERS"
        echo "Please install kernel headers:"
        echo "  dnf install kernel-devel kernel-headers"
        echo "  # For development group:"
        echo "  dnf groupinstall \"Development Tools\""
        exit 1
    fi
    
    echo "✓ Kernel headers found: $KERNEL_HEADERS"
    
    # Check for minimum kernel version (5.14+ for CentOS 9)
    KERNEL_MAJOR=$(echo $KERNEL_VERSION | cut -d. -f1)
    KERNEL_MINOR=$(echo $KERNEL_VERSION | cut -d. -f2)
    
    if [ "$KERNEL_MAJOR" -lt 5 ] || ([ "$KERNEL_MAJOR" -eq 5 ] && [ "$KERNEL_MINOR" -lt 14 ]); then
        echo "Warning: Kernel version $KERNEL_VERSION may not be fully compatible."
        echo "CentOS 9 64-bit requires kernel 5.14 or higher."
    else
        echo "✓ Kernel version compatible: $KERNEL_VERSION"
    fi
}

# Check for GTK development libraries
check_gtk_dependencies() {
    echo "Checking GTK dependencies..."
    
    if ! pkg-config --exists gtk+-2.0; then
        echo "✗ GTK+ 2.0 development libraries not found"
        echo "Install with: sudo yum install gtk2-devel pkg-config"
        echo "Or: sudo apt-get install libgtk2.0-dev pkg-config"
        exit 1
    else
        echo "✓ GTK+ 2.0 development libraries found"
    fi
}

# Build crypto driver
build_crypto_driver() {
    echo
    echo "Building crypto driver..."
    
    cd drivers/crypto_driver
    
    if make clean && make; then
        echo "✓ Crypto driver built successfully"
    else
        echo "✗ Failed to build crypto driver"
        exit 1
    fi
    
    cd "$PROJECT_ROOT"
}

# Build USB keyboard driver
build_usb_driver() {
    echo
    echo "Building USB keyboard driver..."
    
    cd drivers/usb_keyboard
    
    if make clean && make; then
        echo "✓ USB keyboard driver built successfully"
    else
        echo "✗ Failed to build USB keyboard driver"
        exit 1
    fi
    
    cd "$PROJECT_ROOT"
}

# Build userspace applications
build_userspace() {
    echo
    echo "Building userspace applications..."
    
    cd userspace
    
    if make clean && make; then
        echo "✓ Userspace applications built successfully"
    else
        echo "✗ Failed to build userspace applications"
        exit 1
    fi
    
    cd "$PROJECT_ROOT"
}

# Main build function
main() {
    echo "Starting build process..."
    
    # Check prerequisites
    check_kernel_headers
    check_gtk_dependencies
    
    # Build components
    build_crypto_driver
#    build_usb_driver
    build_userspace
    
    echo
    echo "=== Build Complete ==="
    echo
    echo "Available applications:"
    echo "CLI versions:"
    echo "  - userspace/chat_server"
    echo "  - userspace/chat_client"
    echo "  - userspace/test_crypto"
    echo
    echo "GTK versions:"
    echo "  - userspace/chat_server_gtk"
    echo "  - userspace/chat_client_gtk"
    echo
    echo "Next steps:"
    echo "1. Load drivers:"
    echo "   cd scripts && sudo ./load_drivers.sh"
    echo
    echo "2. Test crypto functionality:"
    echo "   cd userspace && ./test_crypto"
    echo
    echo "3. Run chat applications:"
    echo "   CLI: Terminal 1: ./userspace/chat_server"
    echo "        Terminal 2: ./userspace/chat_client"
    echo "   GTK: Terminal 1: ./userspace/chat_server_gtk"
    echo "        Terminal 2: ./userspace/chat_client_gtk"
    echo
}

# Run main function
main "$@"
