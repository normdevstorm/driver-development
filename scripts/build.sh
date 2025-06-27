#!/bin/bash

# Build script cho CentOS 32-bit
# Bài tập lớn - Lập trình Driver Linux

set -e  # Exit on any error

echo "=== Build Script for Linux Driver Project ==="
echo "Building for CentOS 32-bit..."
echo

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
        echo "  yum install kernel-devel"
        echo "  # or"
        echo "  yum install kernel-headers kernel-devel"
        exit 1
    fi
    
    echo "✓ Kernel headers found: $KERNEL_HEADERS"
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
    
    cd ../..
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
    
    cd ../..
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
    
    cd ..
}

# Main build function
main() {
    echo "Starting build process..."
    
    # Check prerequisites
    check_kernel_headers
    
    # Build components
    build_crypto_driver
    build_usb_driver
    build_userspace
    
    echo
    echo "=== Build Complete ==="
    echo
    echo "Next steps:"
    echo "1. Load drivers:"
    echo "   cd scripts && sudo ./load_drivers.sh"
    echo
    echo "2. Test crypto functionality:"
    echo "   cd userspace && ./test_crypto"
    echo
    echo "3. Run chat applications:"
    echo "   Terminal 1: ./userspace/chat_server"
    echo "   Terminal 2: ./userspace/chat_client"
    echo
}

# Run main function
main "$@"
